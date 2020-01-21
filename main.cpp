#include "f4se/PluginAPI.h"
#include "f4se/GameAPI.h"
#include "f4se/GameData.h"
#include "f4se/GameReferences.h"
#include "f4se/GameForms.h"
#include "f4se/GameMenus.h"
#include "f4se/GameRTTI.h"
#include "f4se_common\SafeWrite.h"
#include <shlobj.h>
#include <string>
#include <vector>
#include "xbyak/xbyak.h"
#include "f4se_common\BranchTrampoline.h"
#include "main.h"
#include "f4se_common/f4se_version.h"
#include "f4se/PapyrusVM.h"
#include "f4se/PapyrusNativeFunctions.h"
#include "f4se/PapyrusEvents.h"
#include "f4se\GameExtraData.h"
#include "Globals.h"
#include "Utils.h"
#include "f4se/Translation.h"
#include "f4se/ScaleformLoader.h"
#include "f4se/CustomMenu.h"
#include "f4se\GameTypes.h"
#include "F4NVSerialization.h"
#include "F4NVForms.h"
#include "HookUtil.h"
#include "PapyrusFunctions.h"
#include "ScaleformF4NV.h"
#include "INIReader.h"

#include <functional>
#include <chrono>
#include <future>
#include <cstdio>

std::string esmName = "BulletCountedReload.esm";
std::string mName = "BulletCountedReload";
IDebugLog	gLog;


int ammoCount = 0;
int ammoCapacity = 0;
int totalAmmoCount = 0;

TESIdleForm* idle = nullptr;
TESIdleForm* idle3rd = nullptr;
TESIdleForm* idlePA = nullptr;
TESIdleForm* idlePA3rd = nullptr;
void* action = nullptr;
TESObjectWEAP::InstanceData* weapInstance = nullptr;
ActorValueInfo* checkAvif = nullptr;

BSTEventDispatcher<TESObjectLoadedEvent>* eventDispatcherGlobal = nullptr;

bool logEnabled = false;

PluginHandle			    g_pluginHandle = kPluginHandle_Invalid;
F4SEMessagingInterface		* g_messaging = nullptr;
F4SEPapyrusInterface   *g_papyrus = NULL;

F4SEScaleformInterface		*g_scaleform = NULL;
F4SESerializationInterface	*g_serialization = NULL;





typedef void(*_PlaySubgraphAnimation)(VirtualMachine* vm, UInt32 stackId, Actor* target, BSFixedString asEventName);
RelocAddr <_PlaySubgraphAnimation> PlaySubgraphAnimationInternal(0x138A130);

void PlaySubgraphAnimation(Actor* target, BSFixedString asEventName) {
	PlaySubgraphAnimationInternal((*g_gameVM)->m_virtualMachine, 0, target, asEventName);
}

BSTEventDispatcher<void*>* GetGlobalEventDispatcher(BSTGlobalEvent* globalEvents, const char * dispatcherName)
{
	for (int i = 0; i < globalEvents->eventSources.count; i++) {
		const char* name = GetObjectClassName(globalEvents->eventSources[i]) + 15;    // ?$EventSource@V
		if (strstr(name, dispatcherName) == name) {
			return &globalEvents->eventSources[i]->eventDispatcher;
		}
	}
	return nullptr;
}
#define GET_EVENT_DISPATCHER(EventName) (BSTEventDispatcher<EventName>*) GetGlobalEventDispatcher(*g_globalEvents, #EventName);
//
//STATIC_ASSERT(sizeof(BSTEventSource<void*>) == 0x70);


class BSAnimationGraphEvent {
public:
	TESForm* unk00;
	BSFixedString eventName;
	//etc
};

typedef UInt8(*_tf1)(void * thissink, BSAnimationGraphEvent* evnstruct, void** dispatcher);
RelocAddr <_tf1> tf1_HookTarget(0x2D442E0);
_tf1 tf1_Original;



bool reloadStarted = false;
bool reloadEnd = true;
bool cullBone = false;
bool uncullbone = false;
int incrementor = 0;
int toAdd = 0;
bool animationGoingToPlay = true;
bool stopPressed = false;
bool readyForStopPress = false;

//Play stop idle based on camera mode and handle reload end
void PlayNeededIdle() {
	bool isBoltAction = false;
	if (weapInstance && weapInstance->flags & TESObjectWEAP::InstanceData::kFlag_BoltAction) {
		isBoltAction = true;
	}
	if (GetCameraStateInt() == 0) {
		animationGoingToPlay = PlayIdle((*g_player), idle, idlePA, isBoltAction);
	}
	else {
		animationGoingToPlay = PlayIdle((*g_player), idle3rd, idlePA3rd, isBoltAction);
	}
	if (animationGoingToPlay) {
		if (stopPressed) {
			reloadEndHandle(true);
		}
		else {
			reloadEndHandle();
		}
	}

	logIfNeeded("is animation going to play: " + std::to_string(animationGoingToPlay));
}

//Plays stop idle if R or LMB pressed
void StopIfNeeded() {
	if (stopPressed && incrementor - 1 < toAdd) {
		PlayNeededIdle();
	}
}

void StopLesserAmmo() {
	if ((totalAmmoCount + ammoCount) - ammoCapacity < 0) {
		toAdd = totalAmmoCount;
	}
}

//Animation event hook
UInt8 tf1_Hook(void* arg1, BSAnimationGraphEvent* arg2, void** arg3) {
	if (weapInstance && weapInstance->skill == checkAvif) {
		const char* evnam = arg2->eventName.c_str();
		//reload ended by engine, do reload end logic
		if (!_strcmpi("reloadEnd", evnam)) {
			reloadEndHandle();
			logIfNeeded("reload end");
		}
		//reload state enter, initialize everything
		if (!_strcmpi("Event00", evnam)) {
			reloadStartHandle();
			StopLesserAmmo();
			logIfNeeded("reload start");
		}
		//Bone hide
		if (reloadStarted && !reloadEnd && uncullbone && !_strcmpi("CullBone", evnam)) {
			cullBone = true;
			uncullbone = false;
			logIfNeeded("cullbone");
			if (toAdd != ammoCapacity) {
				incrementor++;
				if (incrementor >= toAdd) {
					PlayNeededIdle();
				}
			}
			StopIfNeeded();


		}
		//Bone show
		if (reloadStarted && !reloadEnd && !_strcmpi("UncullBone", evnam)) {
			cullBone = false;
			uncullbone = true;
			logIfNeeded("uncullbone");
			SetWeapAmmoCapacity(ammoCount + 1);

			if (incrementor >= toAdd) {
				PlayNeededIdle();
			}
			//with 3rd person faster speeds, game unable to play reload end, so making sure its played once more in first uncullbone
			if (!animationGoingToPlay) {
				PlayNeededIdle();

			}
		}
		//shell insert
		if (reloadStarted && !reloadEnd && !_strcmpi("ReloadComplete", evnam)) {
			readyForStopPress = true;
			//StopIfNeeded();
		}

		//Manually handle relaod end for various situations
		if (reloadStarted && !reloadEnd && !_strcmpi("pipboyOpened", evnam)) {
			reloadEndHandle();
			logIfNeeded("pipboy opened");
		}
		if (reloadStarted && !reloadEnd && !_strcmpi("weaponSwing", evnam)) {
			reloadEndHandle();
			logIfNeeded("weapon swing");
		}
		if (reloadStarted && !reloadEnd && !_strcmpi("throwEnd", evnam)) {
			reloadEndHandle();
			logIfNeeded("throw end");
		}
		/*if (!_strcmpi("weaponDraw", evnam) && reloadStarted && !reloadEnd) {
		reloadEndHandle();
		logIfNeeded("Weapon Draw");
		}*/
		if (reloadStarted && !reloadEnd && !_strcmpi("impactLandEnd", evnam)) {
			reloadEndHandle();
			logIfNeeded("Weapon Draw");
		}

		/*if (reloadStarted && !reloadEnd) {
		_MESSAGE("%s", evnam);
		}*/
	}
	return tf1_Original(arg1, arg2, arg3);
}


struct TESEquipEvent
{
	TESObjectREFR*    owner;        // 00
	UInt32            FormID;        // 08
	char			  padding[9];
	bool		      isEquipping;
	char              padding2[111];

	BGSInventoryItem::Stack * invItem;
	char			  padding3[15];
	TESObject*		  item;
	TESObjectWEAP::InstanceData* instanceData;
};
STATIC_ASSERT(offsetof(TESEquipEvent, instanceData) == 0xa8);
STATIC_ASSERT(offsetof(TESEquipEvent, invItem) == 0x88);


DECLARE_EVENT_DISPATCHER(TESEquipEvent, 0x00442750);


//Equip Handler
void HanldeWeaponEquip(TESObjectWEAP* f, BGSInventoryItem::Stack * stack) {
	TESObjectWEAP::InstanceData* equippedWeaponInstance = GetInstanceDataFromExtraDataList(stack->extraData, f, true);
	if (equippedWeaponInstance && equippedWeaponInstance->skill == checkAvif) {
		ammoCapacity = equippedWeaponInstance->ammoCapacity;
		weapInstance = equippedWeaponInstance;
		logIfNeeded("weapon instance equipped with ammo capacity of:" + std::to_string(weapInstance->ammoCapacity));
	}
	else {
		logIfNeeded("problem with equip");
	}
}


//Equip event (not used anymore)
class EquipEventSink : public BSTEventSink<TESEquipEvent>
{
public:
	virtual	EventResult	ReceiveEvent(TESEquipEvent * evn, void * dispatcher) override
	{
		/*if (evn->owner && evn->owner == (*g_player)) {
		TESForm * ff = LookupFormByID(evn->FormID);
		if (ff && ff->formType == kFormType_WEAP) {
		TESObjectWEAP* f = (TESObjectWEAP*)ff;
		if (f && evn->item && evn->invItem) {
		reloadEndHandle();
		HanldeWeaponEquip(f, evn->invItem);
		}
		}
		}*/


		return kEvent_Continue;
	}
};
EquipEventSink equipEventSink;

//Called after game load to fill weapon instance if its equipped
void FillEquipDataFromEquippedItem() {
	auto inventory = (*g_player)->inventoryList;
	if (inventory)
	{
		inventory->inventoryLock.LockForReadAndWrite();
		for (size_t i = 0; i < inventory->items.count; i++)
		{
			TESForm* form = inventory->items[i].form;
			BGSInventoryItem it = inventory->items[i];
			BGSInventoryItem::Stack* stack = inventory->items[i].stack;

			if (form && form->formType == kFormType_WEAP && stack)
			{
				TESObjectWEAP* weap = (TESObjectWEAP*)form;

				stack->Visit([&](BGSInventoryItem::Stack * stackx)
				{
					if (stackx) {
						if (stackx->flags & 7)
						{
							HanldeWeaponEquip(weap, stackx);
						}

					}
					return true;
				});
			}
		}
		inventory->inventoryLock.Unlock();
	}

}


struct PlayerAmmoCountEvent
{
	UInt32							ammoCount;        // 00
	UInt32							totalAmmoCount;    // 04
	UInt64							unk08;            // 08
	TESObjectWEAP*					weapon;            // 10
	TESObjectWEAP::InstanceData*    weaponInstance;
	//...
};
STATIC_ASSERT(offsetof(PlayerAmmoCountEvent, weapon) == 0x10);

//Ammo count handle
class PlayerAmmoCountEventSink : public BSTEventSink<PlayerAmmoCountEvent>
{
public:
	virtual ~PlayerAmmoCountEventSink() { };

	virtual    EventResult    ReceiveEvent(PlayerAmmoCountEvent * evn, void * dispatcher) override
	{

		if (evn->weapon || evn->weaponInstance || evn->unk08 == 1) {
			if (evn->unk08 == 1 && evn->weapon && evn->weaponInstance && evn->weaponInstance != weapInstance) {
				weapInstance = evn->weaponInstance;
				ammoCapacity = weapInstance->ammoCapacity;
			}
			ammoCount = evn->ammoCount;
			totalAmmoCount = evn->totalAmmoCount;
			logIfNeeded("ammo count: " + std::to_string(ammoCount));
		}
		return kEvent_Continue;
	}
};
PlayerAmmoCountEventSink playerAmmoCountEventSink;





bool RegisterAfterLoadEvents() {


	auto eventDispatcher4 = GET_EVENT_DISPATCHER(PlayerAmmoCountEvent);
	if (eventDispatcher4) {
		eventDispatcher4->AddEventSink(&playerAmmoCountEventSink);
	}
	else {
		logMessage("cant register PlayerAmmoCountEvent:");
		return false;
	}

	//equip even was unsatble when testing by scriv, so its not used anymore
	/*auto eventDispatcher3 = GetEventDispatcher<TESEquipEvent>();
	if (eventDispatcher3) {
	eventDispatcher3->eventSinks.Push(&equipEventSink);
	}
	else {
	logMessage("cant register TESEquipEvent:");
	return false;
	}*/


	return true;
}


bool oncePerSession = false;

class TESLoadGameHandler : public BSTEventSink<TESLoadGameEvent>
{
public:
	virtual ~TESLoadGameHandler() { };
	virtual    EventResult    ReceiveEvent(TESLoadGameEvent * evn, void * dispatcher) override
	{
		FillEquipDataFromEquippedItem();
		if (!oncePerSession) {
			if (!RegisterAfterLoadEvents()) {
				logMessage("unable to register for events");
			}
			oncePerSession = true;
		}
		return kEvent_Continue;
	}
};




void logMessage(std::string aString)
{
	_MESSAGE(("[" + mName + "] " + aString).c_str());
}
void GameDataReady()
{
	static auto pLoadGameHandler = new TESLoadGameHandler();
	GetEventDispatcher<TESLoadGameEvent>()->AddEventSink(pLoadGameHandler);

	HandleIniFiles();
}





void F4SEMessageHandler(F4SEMessagingInterface::Message* msg)
{
	switch (msg->type)
	{
	case F4SEMessagingInterface::kMessage_GameDataReady:
	{
		bool isReady = reinterpret_cast<bool>(msg->data);
		if (isReady)
		{

			if (!GetForms()) {
				logMessage("Unable to get one of the form, game wont work");
			}

			GameDataReady();
			ReceiveKeys();
		}
		break;
	}
	case F4SEMessagingInterface::kMessage_NewGame:
	{
		if (!oncePerSession) {
			if (!RegisterAfterLoadEvents()) {
				logMessage("unable to register for events");
			}
			oncePerSession = true;
		}
		break;
	}
	}
}



void DoSerialization() {
	g_serialization->SetUniqueID(g_pluginHandle, 'F4NV');
	g_serialization->SetRevertCallback(g_pluginHandle, F4NVSerialization::RevertCallback);
	g_serialization->SetLoadCallback(g_pluginHandle, F4NVSerialization::LoadCallback);
	g_serialization->SetSaveCallback(g_pluginHandle, F4NVSerialization::SaveCallback);
}

extern "C"
{

	bool F4SEPlugin_Query(const F4SEInterface * f4se, PluginInfo * info)
	{
		// Check game version
		if (f4se->runtimeVersion != RUNTIME_VERSION_1_10_163) {
			_WARNING("WARNING: Unsupported runtime version %08X. This DLL is built for v1.10.163 only.", f4se->runtimeVersion);
			MessageBox(NULL, (LPCSTR)("Unsupported runtime version (expected v1.10.163). \n" + mName + " will be disabled.").c_str(), (LPCSTR)mName.c_str(), MB_OK | MB_ICONEXCLAMATION);
			return false;
		}

		gLog.OpenRelative(CSIDL_MYDOCUMENTS, (const char*)("\\My Games\\Fallout4\\F4SE\\" + mName + ".log").c_str());
		logMessage("v1.0");
		logMessage("query");
		// populate info structure
		info->infoVersion = PluginInfo::kInfoVersion;
		info->name = mName.c_str();
		info->version = 1;

		// store plugin handle so we can identify ourselves later
		g_pluginHandle = f4se->GetPluginHandle();

		g_scaleform = (F4SEScaleformInterface *)f4se->QueryInterface(kInterface_Scaleform);
		if (!g_scaleform) {
			_FATALERROR("couldn't get scaleform interface");
			return false;
		}
		else {
			_MESSAGE("got it");
		}

		g_papyrus = (F4SEPapyrusInterface *)f4se->QueryInterface(kInterface_Papyrus);
		if (!g_papyrus) {
			_FATALERROR("couldn't get papyrus interface");
			return false;
		}
		else {
			_MESSAGE("got it papyrus");
		}

		g_messaging = (F4SEMessagingInterface *)f4se->QueryInterface(kInterface_Messaging);
		if (!g_messaging)
		{
			_FATALERROR("couldn't get messaging interface");
			return false;
		}
		g_serialization = (F4SESerializationInterface *)f4se->QueryInterface(kInterface_Serialization);
		if (!g_serialization) {
			_MESSAGE("couldn't get serialization interface");
			return false;
		}
		return true;
	}

	bool F4SEPlugin_Load(const F4SEInterface *f4se)
	{
		if (g_messaging)
			g_messaging->RegisterListener(g_pluginHandle, "F4SE", F4SEMessageHandler);

		if (!g_localTrampoline.Create(1024 * 64, nullptr)) {
			_ERROR("couldn't create codegen buffer. this is fatal. skipping remainder of init process.");
			return false;
		}

		if (!g_branchTrampoline.Create(1024 * 64)) {
			_ERROR("couldn't create branch trampoline. this is fatal. skipping remainder of init process.");
			return false;
		}
		if (!g_localTrampoline.Create(1024 * 64, nullptr))
		{
			_ERROR("couldn't create codegen buffer. this is fatal. skipping remainder of init process.");
			return false;
		}

		tf1_Original = HookUtil::SafeWrite64(tf1_HookTarget.GetUIntPtr(), &tf1_Hook);
		logMessage("load");
		return true;

	}
};
