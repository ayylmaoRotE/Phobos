#include <Utilities/EVADetour.h>
#include <Windows.h>
#include <cstdint>

PlayIndex_t g_RealPlayIndex = nullptr;
int g_EVA_NukeLaunched = -1;
EVADedupe g_EvaDedup {};

// Read the target behind a PlayIndex JMP stub (&VoxClass::PlayIndex).
// Handles both: E9 rel32  and  FF 25 [imm32]  (absolute indirect jump).
static uintptr_t GetJmpTarget(void* stub)
{
	auto* p = reinterpret_cast<uint8_t*>(stub);

	// Case 1: near jump: E9 rel32
	if (p[0] == 0xE9)
	{
		int32_t rel = *reinterpret_cast<int32_t*>(p + 1);
		return reinterpret_cast<uintptr_t>(p + 5 + rel);
	}

	// Case 2: absolute indirect jump: FF 25 [imm32]
	// Encoding: FF 25 xx xx xx xx  -> jmp dword ptr [imm32]
	if (p[0] == 0xFF && p[1] == 0x25)
	{
		// read the absolute address that holds the jump target
		auto ptrToTarget = *reinterpret_cast<uint32_t*>(p + 2);
		// that memory holds the 32-bit absolute target address
		return *reinterpret_cast<uint32_t*>(ptrToTarget);
	}

	// Unknown stub shape – return 0 to signal "can't resolve"
	return 0;
}

static void WriteJmp(void* at, void* to)
{
	DWORD oldProt {};
	auto* p = reinterpret_cast<uint8_t*>(at);
	VirtualProtect(p, 5, PAGE_EXECUTE_READWRITE, &oldProt);
	intptr_t rel = reinterpret_cast<uint8_t*>(to) - (p + 5);
	p[0] = 0xE9;
	*reinterpret_cast<int32_t*>(p + 1) = static_cast<int32_t>(rel);
	VirtualProtect(p, 5, oldProt, &oldProt);
}

void __fastcall PlayIndex_Sync(int index, int unk, int houseIdx)
{
	// Lazy resolve once it’s actually called (safe even if very early)
	if (g_EVA_NukeLaunched == -1)
	{
		g_EVA_NukeLaunched = VoxClass::FindIndex("EVA_NuclearMissileLaunched");
		// If not found, remains -1 and we just pass-through.
	}

	// Never let audio touch the sim RNG
	SyncGuard _guard;

	// Normalize the nuke launch EVA so all clients do the same thing.
	// (Optional—keep if you want it global & deterministic.)
	if (index == g_EVA_NukeLaunched && index != -1)
	{
		if (!DedupFrame(index))
		{
			g_RealPlayIndex(index, -1, -1); // neutral priority/house => same path everywhere
		}
		return;
	}

	// Everything else is RNG-guarded pass-through
	g_RealPlayIndex(index, unk, houseIdx);
}

void InstallEVADetour()
{
	// 1) Find the **real** PlayIndex implementation behind the JMP stub.
	void* const stub = reinterpret_cast<void*>(&VoxClass::PlayIndex);
	const uintptr_t target = GetJmpTarget(stub);

	// If we couldn't resolve a target or it equals the stub, bail out safely.
	if (target == 0 || target == reinterpret_cast<uintptr_t>(stub))
	{
		// Don't install – avoids recursion/CTD
		return;
	}

	// 2) Save the unmodified target as the real function
	g_RealPlayIndex = reinterpret_cast<PlayIndex_t>(target);

	// 3) Patch the **stub** to jump to our detour (DO NOT patch the target)
	//    This avoids recursion – our pass-through calls `target` directly.
	WriteJmp(stub, reinterpret_cast<void*>(&PlayIndex_Sync));
}
