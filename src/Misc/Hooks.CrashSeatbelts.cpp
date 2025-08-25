// Performance and stability seatbelts for crash-prone game code
#include <Phobos.h>
#include <Helpers/Macro.h>

// Safe read helper – no headers needed, SEH is a compiler keyword on x86 MSVC
static __forceinline bool SafeRead32(const void* p, DWORD& out)
{
	__try { out = *reinterpret_cast<const DWORD*>(p); return true; }
	__except (1) { out = 0; return false; }
}

// Patch the crashing compare inside the loop:
// Original:
//   00520EBC  3B55FC            cmp edx, dword ptr [ebp-4]
//   00520EBF  75 28             jne 0x520EE9
//   00520EC1  8D8E 9C000000     lea ecx, [esi+9Ch]
//   ...
DEFINE_HOOK(0x520EBC, CrashSeatbelt_CmpEbpMinus4, 0x0B)  // overwrite 11 bytes
{
	enum : DWORD
	{
		ContinueEqual = 0x520EC7, // after the LEA we overwrote
		ContinueNotEqual = 0x520EE9  // original JNE target (loop continuation)
	};

	// We need EDX (left operand), EBP (for [EBP-4]), and ESI (to rebuild ECX on equal)
	GET(DWORD, edx, EDX);
	GET(DWORD, ebp, EBP);
	GET(DWORD, esi, ESI);

	DWORD rhs = 0;
	const bool ok = SafeRead32(reinterpret_cast<const void*>(ebp - 4), rhs);

	if (ok && edx == rhs)
	{
		// Recreate: lea ecx, [esi+9Ch]
		R->ECX(esi + 0x9C);
		return ContinueEqual;
	}

	// If the read faults or values differ, behave like original JNE -> skip to 0x520EE9
	return ContinueNotEqual;
}