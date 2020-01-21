#include "Utils.h"

struct PropertyInfo {
	BSFixedString		scriptName;		// 00
	BSFixedString		propertyName;	// 08
	UInt64				unk10;			// 10
	void*				unk18;			// 18
	void*				unk20;			// 20
	void*				unk28;			// 28
	SInt32				index;			// 30	-1 if not found
	UInt32				unk34;			// 34
	BSFixedString		unk38;			// 38
};
STATIC_ASSERT(offsetof(PropertyInfo, index) == 0x30);
STATIC_ASSERT(sizeof(PropertyInfo) == 0x40);

typedef bool(*_GetPropertyValueByIndex)(VirtualMachine* vm, VMIdentifier** identifier, int idx, VMValue* outValue);
const int Idx_GetPropertyValueByIndex = 0x25;

typedef void* (*_GetPropertyInfo)(VMObjectTypeInfo* objectTypeInfo, void* outInfo, BSFixedString* propertyName, bool unk4);
RelocAddr <_GetPropertyInfo> GetPropertyInfo_Internal(0x026F3850);

typedef bool(*_IKeywordFormBase_HasKeyword)(IKeywordFormBase* keywordFormBase, BGSKeyword* keyword, UInt32 unk3);

TESForm * GetFormFromIdentifier(const std::string & identifier)
{
	auto delimiter = identifier.find('|');
	if (delimiter != std::string::npos) {
		std::string modName = identifier.substr(0, delimiter);
		std::string modForm = identifier.substr(delimiter + 1);

		const ModInfo* mod = (*g_dataHandler)->LookupModByName(modName.c_str());
		if (mod && mod->modIndex != -1) {
			UInt32 formID = std::stoul(modForm, nullptr, 16) & 0xFFFFFF;
			UInt32 flags = GetOffset<UInt32>(mod, 0x334);
			if (flags & (1 << 9)) {
				// ESL
				formID &= 0xFFF;
				formID |= 0xFE << 24;
				formID |= GetOffset<UInt16>(mod, 0x372) << 12;	// ESL load order
			}
			else {
				formID |= (mod->modIndex) << 24;
			}
			return LookupFormByID(formID);
		}
	}
	return nullptr;
}
void GetPropertyInfo(VMObjectTypeInfo * objectTypeInfo, PropertyInfo * outInfo, BSFixedString * propertyName)
{
	GetPropertyInfo_Internal(objectTypeInfo, outInfo, propertyName, 1);
}
bool GetPropertyValue(const char * formIdentifier, const char * scriptName, const char * propertyName, VMValue * valueOut)
{
	TESForm* targetForm = GetFormFromIdentifier(formIdentifier);
	if (!targetForm) {
		_WARNING("Warning: Cannot retrieve property value %s from a None form. (%s)", propertyName, formIdentifier);
		return false;
	}

	VirtualMachine* vm = (*g_gameVM)->m_virtualMachine;
	VMScript script(targetForm, scriptName);

	if (!script.m_identifier) {
		_WARNING("Warning: Cannot retrieve a property value %s from a form with no scripts attached. (%s)", propertyName, formIdentifier);
		return false;
	}

	// Find the property
	PropertyInfo pInfo = {};
	pInfo.index = -1;
	GetPropertyInfo(script.m_identifier->m_typeInfo, &pInfo, &BSFixedString(propertyName));

	if (pInfo.index != -1) {
		vm->GetPropertyValueByIndex(&script.m_identifier, pInfo.index, valueOut);
		return true;
	}
	else {
		_WARNING("Warning: Property %s does not exist on script %s", propertyName, script.m_identifier->m_typeInfo->m_typeName.c_str());
		return false;
	}
}

BSFixedString GetDisplayName(ExtraDataList* extraDataList, TESForm * kbaseForm)
{
	TESForm * baseForm = kbaseForm;

	if (baseForm)
	{
		if (extraDataList)
		{
			BSExtraData * extraData = extraDataList->GetByType(ExtraDataType::kExtraData_TextDisplayData);
			if (extraData)
			{
				ExtraTextDisplayData * displayText = DYNAMIC_CAST(extraData, BSExtraData, ExtraTextDisplayData);
				if (displayText)
				{
					return *CALL_MEMBER_FN(displayText, GetReferenceName)(baseForm);
				}
			}
		}

		TESFullName* pFullName = DYNAMIC_CAST(baseForm, TESForm, TESFullName);
		if (pFullName)
			return pFullName->name;
	}

	return BSFixedString();
}

//
bool HasKeyword(TESForm* form, BGSKeyword* keyword) {
	IKeywordFormBase* keywordFormBase = DYNAMIC_CAST(form, TESForm, IKeywordFormBase);
	if (keywordFormBase) {
		auto HasKeyword_Internal = GetVirtualFunction<_IKeywordFormBase_HasKeyword>(keywordFormBase, 1);
		if (HasKeyword_Internal(keywordFormBase, keyword, 0)) {
			return true;
		}
	}
	return false;
}

bool InstWEAPHasKeyword(TESObjectWEAP::InstanceData* thisInstance, BGSKeyword* kwdToCheck) {
	BGSKeywordForm * keywordForm = nullptr;

	keywordForm = thisInstance->keywords;
	if (!keywordForm)
		return false;

	for (UInt32 i = 0; i < keywordForm->numKeywords; i++)
	{
		BGSKeyword* curr = keywordForm->keywords[i];
		if (curr == kwdToCheck) {
			return true;
		}
	}

	return false;
}

//
BSFixedString GetFormDescription(TESForm * thisForm)
{
	if (!thisForm)
		return BSFixedString();

	TESDescription * pDescription = DYNAMIC_CAST(thisForm, TESForm, TESDescription);
	if (pDescription) {
		BSString str;
		CALL_MEMBER_FN(pDescription, Get)(&str, nullptr);
		return str.Get();
	}

	return BSFixedString();
}


//helper function to get instance data of weapon from extradatalist
TESObjectWEAP::InstanceData* GetInstanceDataFromExtraDataList(ExtraDataList* extraDataList, TESForm* form, bool shouldCreateInstanceData) {
	TESObjectWEAP::InstanceData* currweapInst = nullptr;

	TBO_InstanceData* neededInst = nullptr;
	if (extraDataList && extraDataList->m_data && extraDataList->m_refCount && extraDataList->m_presence) {
		BSExtraData * extraData = extraDataList->GetByType(ExtraDataType::kExtraData_InstanceData);
		if (extraData) {
			ExtraInstanceData * objectModData = DYNAMIC_CAST(extraData, BSExtraData, ExtraInstanceData);
			if (objectModData)
				neededInst = objectModData->instanceData;
		}
		else {
			if (form && shouldCreateInstanceData) {
				TESBoundObject * boundObject = DYNAMIC_CAST(form, TESForm, TESBoundObject);
				TBO_InstanceData * instanceData = nullptr;
				if (boundObject) {
					instanceData = boundObject->CloneInstanceData(nullptr);
					if (instanceData) {
						ExtraInstanceData * objectModData = ExtraInstanceData::Create(form, instanceData);
						if (objectModData) {
							extraDataList->Add(ExtraDataType::kExtraData_InstanceData, objectModData);
							neededInst = instanceData;
						}
					}
				}
			}
		}
	}
	currweapInst = (TESObjectWEAP::InstanceData*)Runtime_DynamicCast(neededInst, RTTI_TBO_InstanceData, RTTI_TESObjectWEAP__InstanceData);
	return currweapInst;
}
//same for armor
TESObjectARMO::InstanceData* GetInstanceDataFromExtraDataListARMO(ExtraDataList* extraDataList) {
	TESObjectARMO::InstanceData* currweapInst = nullptr;

	TBO_InstanceData* neededInst = nullptr;
	if (extraDataList) {
		BSExtraData * extraData = extraDataList->GetByType(ExtraDataType::kExtraData_InstanceData);
		if (extraData) {
			ExtraInstanceData * objectModData = DYNAMIC_CAST(extraData, BSExtraData, ExtraInstanceData);
			if (objectModData)
				neededInst = objectModData->instanceData;
		}
		else {
			//_MESSAGE("Item dont have instance data");
		}
	}
	currweapInst = (TESObjectARMO::InstanceData*)Runtime_DynamicCast(neededInst, RTTI_TBO_InstanceData, RTTI_TESObjectARMO__InstanceData);
	return currweapInst;
}

void logIfNeeded(std::string text) {
	if (logEnabled) {
		_MESSAGE(text.c_str());
	}
}