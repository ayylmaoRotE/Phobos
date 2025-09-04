#pragma once
#include <VoxClass.h>
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
