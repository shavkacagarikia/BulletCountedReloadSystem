
#pragma once
#include "f4se\GameForms.h"
#include "Globals.h"

bool GetForms() {
	bool toReturn = true;
	idle = reinterpret_cast<TESIdleForm*>(GetFormFromIdentifier("Fallout4.esm|4AD3"));
	//idle3rd = reinterpret_cast<TESIdleForm*>(GetFormFromIdentifier("LeverActionReloadOverhaul.esp|1E314"));
	idle3rd = reinterpret_cast<TESIdleForm*>(GetFormFromIdentifier("Fallout4.esm|179617"));

	idlePA = reinterpret_cast<TESIdleForm*>(GetFormFromIdentifier("Fallout4.esm|1922AB"));

	idlePA3rd = reinterpret_cast<TESIdleForm*>(GetFormFromIdentifier("Fallout4.esm|18AADB"));

	checkAvif = reinterpret_cast<ActorValueInfo*>(GetFormFromIdentifier("Fallout4.esm|300"));

	action = reinterpret_cast<void*>(GetFormFromIdentifier("Fallout4.esm|B259D"));

	return toReturn;
}