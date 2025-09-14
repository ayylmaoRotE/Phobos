#pragma once
#include <VoxClass.h>
#include <VocClass.h>
#include <HouseClass.h>
#include <Utilities/SyncGuard.h> // or SyncGuard.h — your guard header

inline void PlayIndex_Safe(int evaIndex, int priority = -1, int houseIdx = -1)
{
	SyncGuard _g;           // snapshots/restores Scenario RNG
	VoxClass::PlayIndex(evaIndex, priority, houseIdx);
}

inline void PlayName_Safe(const char* name, int priority = -1, int houseIdx = -1)
{
	SyncGuard _g;
	VoxClass::Play(name, priority, houseIdx);
}

inline void PlayGlobal_Safe(int vocIndex, int pan = 0x2000, float vol = 1.0f)
{
	if (vocIndex < 0) return;
	SyncGuard _g;                     // snapshot/restore sim RNG
	VocClass::PlayGlobal(vocIndex, pan, vol);
}
