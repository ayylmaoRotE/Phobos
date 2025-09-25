#pragma once
#include <atomic>
#include <VoxClass.h>
#include <Unsorted.h>
#include <Utilities/SyncGuard.h>  // your existing RNG guard

// x86 fastcall: ECX=index, EDX=unk, [esp+4]=houseIdx
using PlayIndex_t = void(__fastcall*)(int index, int unk, int houseIdx);

extern PlayIndex_t g_RealPlayIndex;
extern int g_EVA_NukeLaunched;
void  InstallEVADetour();

// --- one-frame de-dup (cheap, lock-free) ---
struct EVADedupe { int frame = -1, idx = -1; };
extern EVADedupe g_EvaDedup;
inline bool DedupFrame(int idx)
{
	const int f = static_cast<int>(Unsorted::CurrentFrame);
	if (g_EvaDedup.frame == f && g_EvaDedup.idx == idx)
	{
		return true; // suppress duplicate in same frame
	}
	g_EvaDedup.frame = f;
	g_EvaDedup.idx = idx;
	return false; // allow
}

// --- non-blocking re-entrancy gate (prevents recursive detour re-entry) ---
struct EVAReentryGate
{
	static std::atomic_flag flag;
	bool entered = false;
	EVAReentryGate() { entered = !flag.test_and_set(std::memory_order_acquire); }
	~EVAReentryGate() { if (entered) flag.clear(std::memory_order_release); }
	explicit operator bool() const { return entered; }
};

// Our replacement
void __fastcall PlayIndex_Sync(int index, int unk, int houseIdx);
