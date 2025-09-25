#pragma once
#include <VoxClass.h>
#include <VocClass.h>
#include <HouseClass.h>
#include <Unsorted.h>
#include <Utilities/SyncGuard.h>

// Optional: delay user-triggered EVAs until the world is stable.
// #define EVA_INIT_GUARD_FRAMES 30

inline void PlayIndex_Safe(int evaIndex, int priority = -1, int houseIdx = -1)
{
	// Robustness: ignore invalid ids
	if (evaIndex < 0) return;
#ifdef EVA_INIT_GUARD_FRAMES
	if (Unsorted::CurrentFrame < EVA_INIT_GUARD_FRAMES) return;
#endif
	SyncGuard _g; // snapshot/restore sim RNG
	VoxClass::PlayIndex(evaIndex, priority, houseIdx);
}

inline void PlayName_Safe(const char* name, int priority = -1, int houseIdx = -1)
{
	if (!name || !*name) return;
#ifdef EVA_INIT_GUARD_FRAMES
	if (Unsorted::CurrentFrame < EVA_INIT_GUARD_FRAMES) return;
#endif
	SyncGuard _g;
	VoxClass::Play(name, priority, houseIdx);
}

inline void PlayGlobal_Safe(int vocIndex, int pan = 0x2000, float vol = 1.0f)
{
	if (vocIndex < 0) return;
#ifdef EVA_INIT_GUARD_FRAMES
	if (Unsorted::CurrentFrame < EVA_INIT_GUARD_FRAMES) return;
#endif
	SyncGuard _g; // snapshot/restore sim RNG
	VocClass::PlayGlobal(vocIndex, pan, vol);
}
