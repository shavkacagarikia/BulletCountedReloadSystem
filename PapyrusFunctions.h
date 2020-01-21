#pragma once
#include "f4se\GameForms.h"
#include "f4se/GameTypes.h"
#include "f4se\PapyrusArgs.h"
#include "f4se\PapyrusEvents.h"
#include "f4se\GameObjects.h"
#include "f4se\GameReferences.h"
#include "f4se\GameRTTI.h"
#include "f4se/GameCamera.h"

#include "Globals.h"
#include "Utils.h"



void ShowNotification(std::string asNotificationText);

void ShowMessagebox(std::string asText);

void PlaySubgraphAnimation(Actor* target, BSFixedString asEventName);

void SetAnimationVariableBool(TESObjectREFR* ref, BSFixedString asVariableName, bool newVal);

bool PlayIdle(Actor* actor, TESIdleForm* idle, TESIdleForm* idlePA, bool isBoltAction);

void reloadStartHandle();

void reloadEndHandle(bool threaded = false);

void SetWeapAmmoCapacity(int amount);

bool ForceStop(TESIdleForm* idlePA);

SInt32 GetCameraStateInt();