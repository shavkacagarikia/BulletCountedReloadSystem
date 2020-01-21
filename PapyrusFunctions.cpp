#include "PapyrusFunctions.h"

#include <functional>
#include <chrono>
#include <future>
#include <cstdio>



class later
{
public:
	template <class callable, class... arguments>
	later(int after, bool async, callable&& f, arguments&&... args)
	{
		std::function<typename std::result_of<callable(arguments...)>::type()> task(std::bind(std::forward<callable>(f), std::forward<arguments>(args)...));

		if (async)
		{
			std::thread([after, task]() {
				std::this_thread::sleep_for(std::chrono::milliseconds(after));
				task();
			}).detach();
		}
		else
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(after));
			task();
		}
	}

};

typedef void(*_SetAnimationVariableBool)(VirtualMachine* vm, UInt32 stackId, TESObjectREFR* ref, BSFixedString asVariableName, bool newVal);
RelocAddr <_SetAnimationVariableBool > SetAnimationVariableBoolInternal(0x140EA10);

typedef bool(*_PlayIdle)(VirtualMachine* vm, UInt32 stackId, Actor* actor, TESIdleForm* idle);
RelocAddr <_PlayIdle > PlayIdleInternal(0x13863A0);

typedef bool(*_PlayIdle2)(Actor* actor, TESIdleForm* idle, UInt64 unk, VirtualMachine* vm, UInt32 stackId);
RelocAddr <_PlayIdle2 > PlayIdleInternal2(0x13864C0);

typedef bool(*_PlayIdleAction)(Actor* actor, void* action, TESObjectREFR target, VirtualMachine* vm, UInt32 stackId);
RelocAddr <_PlayIdleAction > PlayIdleAction(0x13864A0);


void SetAnimationVariableBool(TESObjectREFR* ref, BSFixedString asVariableName, bool newVal) {
	SetAnimationVariableBoolInternal((*g_gameVM)->m_virtualMachine, 1, ref, asVariableName, newVal);
}


bool playAnimEvent(Actor* actor, TESIdleForm* idlePA, BSFixedString name) {
	BSFixedString tempname("");

	tempname = idlePA->idleName;
	idlePA->idleName = name;
	bool gonnaplay = false;
	gonnaplay = PlayIdleInternal2(actor, idlePA, 0, (*g_gameVM)->m_virtualMachine, 0);

	idlePA->idleName = tempname;

	return gonnaplay;
}

bool playAnimEvent2(Actor* actor, TESIdleForm* idlePA, BSFixedString name) {
	BSFixedString tempname("");
	//idlePA->animationFilePath = BSFixedString("Actors\\DLC03\\Character\\_1stPerson\\Animations\\LeverAction\\WPNBoltChargeReady.hkx");
	tempname = idlePA->idleName;
	idlePA->idleName = name;
	bool gonnaplay = false;
	gonnaplay = PlayIdleInternal2(actor, idlePA, 0, (*g_gameVM)->m_virtualMachine, 0);

	idlePA->idleName = tempname;
	//idlePA->animationFilePath = BSFixedString("");
	return gonnaplay;
}



//Play reload end idles when needed
bool PlayIdle(Actor* actor, TESIdleForm* idle, TESIdleForm* idlePA, bool isBoltAction) {
	static BSFixedString reloadEndAnimEvent("reloadEnd");
	static BSFixedString reloadEndAnimEvent2("reloadEndSlave");
	//static BSFixedString reloadEndAnimEvent3("rifleSightedEnd");
	bool resp = false;
	if (idlePA) {
		resp = playAnimEvent(actor, idlePA, reloadEndAnimEvent);

		playAnimEvent(actor, idlePA, reloadEndAnimEvent2);


		/*if (isBoltAction) {
		bool t = false;
		t = playAnimEvent2(actor, idlePA, reloadEndAnimEvent3);
		}*/


	}
	logIfNeeded("is animation stopped:");
	logIfNeeded(std::to_string(resp).c_str());
	return resp;
	/*VMArray<VMVariable> varr;
	VMVariable b1;
	b1.Set(&idle);
	varr.Push(&b1);
	CallFunctionNoWait(actor, "PlayIdle", varr);
	return true;*/
}



void ShowNotification(std::string asNotificationText) {
	CallGlobalFunctionNoWait1<BSFixedString>("Debug", "Notification", BSFixedString(asNotificationText.c_str()));
}

void ShowMessagebox(std::string asText) {
	CallGlobalFunctionNoWait1<BSFixedString>("Debug", "Messagebox", BSFixedString(asText.c_str()));
}


//------------------------------------------------------------------------------------------------------------------------
//Non papyrus

//Set weapon capacity to needed amount to be sure reloadComplete fills needed amount of ammo
void SetWeapAmmoCapacity(int amount) {
	if (weapInstance) {
		if (amount > ammoCapacity) {
			weapInstance->ammoCapacity = ammoCapacity;
		}
		else {
			weapInstance->ammoCapacity = amount;
		}
	}
	else {
		_MESSAGE("weapon instance is nullptr");
	}
}

//clear needed stuff when reload ends
void reloadEndHandle(bool threaded) {
	reloadEnd = true;
	reloadStarted = false;
	incrementor = 0;
	toAdd = 0;
	stopPressed = false;
	animationGoingToPlay = true;
	uncullbone = false;
	readyForStopPress = false;
	if (threaded) {

		later a(500, true, &SetWeapAmmoCapacity, ammoCapacity);
	}
	else {
		SetWeapAmmoCapacity(ammoCapacity);
	}

}



//ready needed stuff when reload is started
void reloadStartHandle() {
	incrementor = 0;
	toAdd = ammoCapacity - ammoCount;
	reloadStarted = true;
	reloadEnd = false;
	animationGoingToPlay = true;
	stopPressed = false;
	uncullbone = false;
	readyForStopPress = false;
	SetWeapAmmoCapacity(ammoCount);
}

// returns the current camera state
// -1 - unknown / there is no camera yet
// 0 - first person
// 1 - auto vanity
// 2 - VATS
// 3 - free
// 4 - iron sights
// 5 - transition
// 6 - tween menu
// 7 - third person 1
// 8 - third person 2
// 9 - furniture
// 10 - horse
// 11 - bleedout
// 12 - dialogue
SInt32 GetCameraStateInt()
{
	PlayerCamera * playerCamera = *g_playerCamera;
	if (playerCamera)
		return playerCamera->GetCameraStateId(playerCamera->cameraState);

	return -1;
}