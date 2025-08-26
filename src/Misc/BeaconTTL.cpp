#include <Phobos.h>
#include <Misc/BeaconTTL.h>

#include <vector>
#include <deque>
#include <algorithm>
#include <limits>
#include <cwchar>
#include <climits>

#include <ArrayClasses.h>
#include <HouseClass.h>
#include <BeaconManagerClass.h>   // includes BeaconClass
#include <Fundamentals.h>         // Unsorted::CurrentFrame

namespace
{
	std::vector<BeaconTTL_Record> gRecs;
	int gNextWakeTick = 0;

	// Placement-driven timestamps (per house) to avoid MP desync:
	// when PlaceBeacon is invoked (replicated), we push Created/Expire.
	// On discovery of the new slot we pop and bind those values.
	std::deque<int> gPendingCreated[8];
	std::deque<int> gPendingExpire[8];

	inline BeaconManagerClass* Mgr() { return &BeaconManagerClass::Instance; }

	inline BeaconClass* GetSlotPtr(int h, int s)
	{
		if (h < 0 || h >= 8 || s < 0 || s >= 3) return nullptr;
		return Mgr()->Beacons[h][s];
	}

	inline bool IsHumanHouse(int idx)
	{
		DynamicVectorClass<HouseClass*>& arr = HouseClass::Array;
		return (idx >= 0 && idx < arr.Count && arr.Items[idx] && arr.Items[idx]->IsControlledByHuman());
	}

	inline bool SlotQualifies(int h, int s)
	{
		if (!GetSlotPtr(h, s)) return false;
		if (BeaconTTL::Cfg.PlayersOnly && !IsHumanHouse(h)) return false;
		return true;
	}

	inline void HardRemove(int h, int s)
	{
		auto* m = Mgr();
		if (!m) return;
		if (h < 0 || h >= 8 || s < 0 || s >= 3) return;

		if (m->Beacons[h][s])
		{
			m->Beacons[h][s] = nullptr;      // free the real engine slot
			if (m->AllocatedCount > 0)
			{
				--m->AllocatedCount;         // keep manager count coherent
			}
		}
	}

	inline void DropRecordForSlot(int h, int s)
	{
		for (size_t i = 0; i < gRecs.size();)
		{
			if (gRecs[i].House == h && gRecs[i].Slot == s)
			{
				gRecs[i] = gRecs.back(); gRecs.pop_back();
			}
			else { ++i; }
		}
	}

	inline void DiscoverNewSlots(int now)
	{
		auto* m = Mgr();
		if (!m) return;

		if (m->AllocatedCount <= 0)
		{
			gRecs.clear();
			return;
		}

		for (int h = 0; h < 8; ++h)
		{
			for (int s = 0; s < 3; ++s)
			{
				if (!SlotQualifies(h, s)) continue;

				bool known = false;
				for (const auto& r : gRecs)
				{
					if (r.House == h && r.Slot == s) { known = true; break; }
				}
				if (!known)
				{
					// Bind placement-driven times if we have them; otherwise use now (shouldn't happen)
					int c = now, e = now + BeaconTTL::Cfg.TTLTicks;
					if (!gPendingCreated[h].empty() && !gPendingExpire[h].empty())
					{
						c = gPendingCreated[h].front(); gPendingCreated[h].pop_front();
						e = gPendingExpire[h].front(); gPendingExpire[h].pop_front();
					}
					gRecs.push_back(BeaconTTL_Record { h, s, c, e });
				}
			}
		}
	}

	inline int CountLive(int house)
	{
		if (house < 0 || house >= 8) return 0;
		int c = 0; for (int s = 0; s < 3; ++s) if (GetSlotPtr(house, s)) ++c; return c;
	}
} // anon

BeaconTTL_Config BeaconTTL::Cfg {};

void __stdcall BeaconTTL::Reset()
{
	gRecs.clear(); gNextWakeTick = 0;
	for (int h = 0; h < 8; ++h) { gPendingCreated[h].clear(); gPendingExpire[h].clear(); }
}

void __stdcall BeaconTTL::OnRulesParsed(const BeaconTTL_Config& in)
{
	Cfg = in;
	if (Cfg.TTLTicks < 5 * 30) Cfg.TTLTicks = 5 * 30;  // clamp ≥5s
	if (Cfg.CullStride < 1)      Cfg.CullStride = 30;      // default 1s
}

void __stdcall BeaconTTL::Tick()
{
	if (!Cfg.Enabled) return;
	const int now = Unsorted::CurrentFrame;
	if (now < gNextWakeTick) return;

	// discover new placements (24 slots worst case)
	DiscoverNewSlots(now);

	// expire and compute next due time
	int nextDue = INT_MAX;

	for (size_t i = 0; i < gRecs.size(); )
	{
		auto& r = gRecs[i];

		if (!GetSlotPtr(r.House, r.Slot))
		{
			gRecs[i] = gRecs.back(); gRecs.pop_back();
			continue;
		}

		if (now >= r.ExpireTick)
		{
			HardRemove(r.House, r.Slot);
			gRecs[i] = gRecs.back(); gRecs.pop_back();
			continue;
		}

		if (r.ExpireTick < nextDue) nextDue = r.ExpireTick;
		++i;
	}

	const int coarse = now + Cfg.CullStride;
	gNextWakeTick = (nextDue == INT_MAX) ? coarse : (nextDue < coarse ? nextDue : coarse);
}

void __stdcall BeaconTTL::RegisterPending(int house, int now)
{
	if (!Cfg.Enabled) return;
	if (house < 0 || house >= 8) return;
	gPendingCreated[house].push_back(now);
	gPendingExpire[house].push_back(now + Cfg.TTLTicks);
}

void __stdcall BeaconTTL::EnsureFreeSlotForHouse(int house, int now)
{
	if (!Cfg.Enabled || !Cfg.ReplaceOldestOnFourth) return;
	if (house < 0 || house >= 8) return;
	if (Cfg.PlayersOnly && !IsHumanHouse(house)) return;

	int live[3] = { -1, -1, -1 }, cnt = 0;
	for (int s = 0; s < 3; ++s) if (GetSlotPtr(house, s)) live[cnt++] = s;
	if (cnt < 3) return;

	// choose oldest tracked; fallback to lowest slot index
	int bestSlot = live[0], bestCreated = INT_MAX; bool found = false;
	for (const auto& r : gRecs)
	{
		if (r.House != house) continue;
		if (!GetSlotPtr(house, r.Slot)) continue;
		found = true;
		if (r.CreatedTick < bestCreated || (r.CreatedTick == bestCreated && r.Slot < bestSlot))
		{
			bestCreated = r.CreatedTick; bestSlot = r.Slot;
		}
	}
	if (!found)
	{
		for (int i = 0; i < 3; ++i) if (live[i] != -1) { bestSlot = live[i]; break; }
	}

	HardRemove(house, bestSlot);
	DropRecordForSlot(house, bestSlot);
	gNextWakeTick = now; // wake to discover the new beacon after placement
}

void __stdcall BeaconTTL::Serialize(PhobosStreamWriter& W)
{
	W.Process(Cfg.Enabled);
	W.Process(Cfg.TTLTicks);
	W.Process(Cfg.CullStride);
	W.Process(Cfg.PlayersOnly);
	W.Process(Cfg.ReplaceOldestOnFourth);

	int n = static_cast<int>(gRecs.size());
	W.Process(n);
	for (const auto& r : gRecs)
	{
		W.Process(r.House);
		W.Process(r.Slot);
		W.Process(r.CreatedTick);
		W.Process(r.ExpireTick);
	}
	W.Process(gNextWakeTick);

	// Pending queues (for saves; optional but recommended)
	for (int h = 0; h < 8; ++h)
	{
		int pc = (int)gPendingCreated[h].size(); W.Process(pc);
		for (int i = 0; i < pc; ++i) { int v = gPendingCreated[h][i]; W.Process(v); }
		int pe = (int)gPendingExpire[h].size();  W.Process(pe);
		for (int i = 0; i < pe; ++i) { int v = gPendingExpire[h][i];  W.Process(v); }
	}
}

void __stdcall BeaconTTL::Deserialize(PhobosStreamReader& R)
{
	R.Process(Cfg.Enabled);
	R.Process(Cfg.TTLTicks);
	R.Process(Cfg.CullStride);
	R.Process(Cfg.PlayersOnly);
	R.Process(Cfg.ReplaceOldestOnFourth);

	int n = 0; R.Process(n);
	gRecs.clear();
	if (n > 0) gRecs.reserve(n);
	for (int i = 0; i < n; ++i)
	{
		BeaconTTL_Record r {};
		R.Process(r.House);
		R.Process(r.Slot);
		R.Process(r.CreatedTick);
		R.Process(r.ExpireTick);
		gRecs.push_back(r);
	}
	R.Process(gNextWakeTick);

	for (int h = 0; h < 8; ++h)
	{
		int pc = 0; R.Process(pc); gPendingCreated[h].clear();
		for (int i = 0; i < pc; ++i) { int v = 0; R.Process(v); gPendingCreated[h].push_back(v); }
		int pe = 0; R.Process(pe); gPendingExpire[h].clear();
		for (int i = 0; i < pe; ++i) { int v = 0; R.Process(v); gPendingExpire[h].push_back(v); }
	}
}
