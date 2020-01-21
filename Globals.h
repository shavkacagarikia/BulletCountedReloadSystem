#pragma once
#include "f4se\GameForms.h"
#include "f4se\GameExtraData.h"
#include "f4se\GameObjects.h"
#include <vector>



extern bool reloadStarted;
extern bool reloadEnd;
extern bool cullBone;
extern bool uncullbone;
extern bool canBeInterrupted;
extern bool animationGoingToPlay;
extern bool stopPressed;
extern bool readyForStopPress;

extern int ammoCount;
extern int ammoCapacity;
extern int incrementor;
extern int toAdd;

extern TESIdleForm* idle;
extern TESIdleForm* idle3rd;
extern TESIdleForm* idlePA;
extern TESIdleForm* idlePA3rd;
extern void* action;

extern ActorValueInfo* checkAvif;

extern bool logEnabled;

extern TESObjectWEAP::InstanceData* weapInstance;


