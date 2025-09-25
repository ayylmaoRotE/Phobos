#include <Utilities/EVADetour.h>
#include <Windows.h>
#include <cstdint>

PlayIndex_t g_RealPlayIndex = nullptr;
int g_EVA_NukeLaunched = -1;
EVADedupe g_EvaDedup {};
std::atomic_flag EVAReentryGate::flag = ATOMIC_FLAG_INIT;

// Read the target behind a PlayIndex JMP stub (&VoxClass::PlayIndex).
// Handles E9 rel32  and  FF 25 [imm32] (absolute indirect jump).
static uintptr_t GetJmpTarget(void* stub)
{
	auto* p = reinterpret_cast<uint8_t*>(stub);

	// near jump: E9 rel32
	if (p[0] == 0xE9)
	{
		int32_t rel = *reinterpret_cast<int32_t*>(p + 1);
		return reinterpret_cast<uintptr_t>(p + 5 + rel);
	}

	// absolute indirect jump: FF 25 [imm32]
	if (p[0] == 0xFF && p[1] == 0x25)
	{
		auto ptrToTarget = *reinterpret_cast<uint32_t*>(p + 2);
		return *reinterpret_cast<uint32_t*>(ptrToTarget);
	}

	return 0; // unknown stub shape
}

static void WriteJmp(void* at, void* to)
{
	DWORD oldProt {};
	auto* p = reinterpret_cast<uint8_t*>(at);
	VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &oldProt);
	const intptr_t rel = reinterpret_cast<uint8_t*>(to) - (p + 5);
	p[0] = 0xE9;
	*reinterpret_cast<int32_t*>(p + 1) = static_cast<int32_t>(rel);
	VirtualProtect(p, 5, oldProt, &oldProt);
}

// Resolve the “nuke launched” EVA ID once, after the world is up a few frames.
// If not found, stays -1 (callers must early-out on index<0).
static void ResolveNukeIndexOnce()
{
	static bool tried = false;
	if (tried) return;

	// avoid probing Ares EVA tables too early
	if (Unsorted::CurrentFrame < 30) return;

	tried = true;
	if (g_EVA_NukeLaunched < 0)
	{
		g_EVA_NukeLaunched = VoxClass::FindIndex("EVA_NuclearMissileLaunched");
	}
}

void __fastcall PlayIndex_Sync(int index, int unk, int houseIdx)
{
	if (!g_RealPlayIndex) return;  // defensive

	if (g_EVA_NukeLaunched == -1)
	{
		g_EVA_NukeLaunched = VoxClass::FindIndex("EVA_NuclearMissileLaunched");
	}

	SyncGuard _guard;

	if (index == g_EVA_NukeLaunched && index != -1)
	{
		if (!DedupFrame(index))
		{
			g_RealPlayIndex(index, -1, -1);
		}
		return;
	}

	g_RealPlayIndex(index, unk, houseIdx);
}

void InstallEVADetour()
{
	static bool installed = false;
	if (installed) return;

	void* const stub = reinterpret_cast<void*>(&VoxClass::PlayIndex);
	const uintptr_t target = GetJmpTarget(stub);

	// Resolve once and only if sane
	if (!target || target == reinterpret_cast<uintptr_t>(stub))
	{
		g_RealPlayIndex = nullptr;
		return; // bail: no patch, no crash
	}

	g_RealPlayIndex = reinterpret_cast<PlayIndex_t>(target);
	WriteJmp(stub, reinterpret_cast<void*>(&PlayIndex_Sync));
	installed = true;
}
