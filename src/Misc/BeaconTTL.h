#pragma once
#include <Utilities/Stream.h>   // PhobosStreamReader / PhobosStreamWriter

// Config for beacon TTL behavior
struct BeaconTTL_Config
{
	bool Enabled = true;
	int  TTLTicks = 60 * 30;            // default: 60s @ 30 tps
	int  CullStride = 30;               // manager cadence (ticks)
	bool PlayersOnly = true;            // expire only human beacons
	bool ReplaceOldestOnFourth = true;  // on 4th placement, drop oldest first
};

// Tracked engine slot (house, slot 0..2)
struct BeaconTTL_Record
{
	int House = -1;
	int Slot = -1;
	int CreatedTick = 0;                // for “oldest” choice
	int ExpireTick = 0;                // when TTL removes it
};

namespace BeaconTTL
{
	extern BeaconTTL_Config Cfg;

	// lifecycle
	void __stdcall Reset();
	void __stdcall OnRulesParsed(const BeaconTTL_Config& in);

	// maintenance: call once per logic frame; self-throttled
	void __stdcall Tick();

	// save/load with Scenario
	void __stdcall Serialize(PhobosStreamWriter& W);
	void __stdcall Deserialize(PhobosStreamReader& R);

	// called RIGHT BEFORE engine PlaceBeacon executes (from our pre-hook)
	void __stdcall RegisterPending(int house, int now);

	// called RIGHT BEFORE engine PlaceBeacon to ensure capacity for the new ping
	void __stdcall EnsureFreeSlotForHouse(int house, int now);

}
