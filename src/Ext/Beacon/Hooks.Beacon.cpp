#include <Phobos.h>
#include <Misc/BeaconTTL.h>
#include <Helpers/Macro.h>
#include <Fundamentals.h>
#include <BeaconManagerClass.h>
// Unsorted::CurrentFrame

// BeaconManagerClass::PlaceBeacon(int houseId, CoordStruct, int), addr 0x430BA0
// Stack at entry: [ret] [houseId] [coord.x] [coord.y] [coord.z] [houseBeaconId]
DEFINE_HOOK(0x430BA0, BeaconManagerClass_PlaceBeacon_PreEnsureSlot, 0x6)
{
	GET_STACK(int, houseId, 0x4);                 // 1st arg = houseId
	const int now = Unsorted::CurrentFrame;
	const int idx = (houseId >= 0 && houseId < 8) ? houseId : -1;

	// MP-deterministic timestamps, then make room if we’re already at 3
	BeaconTTL::RegisterPending(idx, now);
	BeaconTTL::EnsureFreeSlotForHouse(idx, now);

	return 0;                                      // continue into original PlaceBeacon
}

// BeaconManagerClass::CanPlaceBeacon(int houseId) — entry: 0x430F30 (__thiscall)
// Stack at entry: [ret] [houseId]
DEFINE_HOOK(0x430F30, BeaconManagerClass_CanPlaceBeacon_UiAllow, 0x6)
{
	// Read the houseId argument
	GET_STACK(int, houseId, 0x4);
	const int h = (houseId >= 0 && houseId < 8) ? houseId : -1;

	// If our rule is ON and this is exactly the “3 beacons live” case,
	// we force-enable the UI (return true). Otherwise, let the engine handle it.
	if (h >= 0 && BeaconTTL::Cfg.Enabled && BeaconTTL::Cfg.ReplaceOldestOnFourth)
	{
		int live = 0;
		for (int s = 0; s < 3; ++s)
		{
			if (BeaconManagerClass::Instance.Beacons[h][s]) ++live;
		}
		if (live >= 3)
		{
			R->EAX(1);           // TRUE (enabled)
			// IMPORTANT: return to the function’s epilogue (skip original)
			// Replace 0xDEADBEEF with the epilogue address (see below).
			return 0x430F51;
		}
	}

	// Fall-through: run the original CanPlaceBeacon for all other cases
	return 0;
}
