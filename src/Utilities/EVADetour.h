#pragma once
#include <VoxClass.h>
#include <Unsorted.h>
#include <Utilities/SyncGuard.h>  // your existing raw-bytes guard

// x86 fastcall: ECX=index, EDX=unk, [esp+4]=houseIdx
using PlayIndex_t = void(__fastcall*)(int index, int unk, int houseIdx);

extern PlayIndex_t g_RealPlayIndex;
extern int g_EVA_NukeLaunched;
void  InstallEVADetour();

// Simple one-frame de-dup
struct EVADedupe { int frame = -1, idx = -1; };
extern EVADedupe g_EvaDedup;
inline bool DedupFrame(int idx)
{
	const int f = Unsorted::CurrentFrame;
	if (g_EvaDedup.frame == f && g_EvaDedup.idx == idx) return true;
	g_EvaDedup.frame = f; g_EvaDedup.idx = idx; return false;
}

// Our replacement
void __fastcall PlayIndex_Sync(int index, int unk, int houseIdx);
