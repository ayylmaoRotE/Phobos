#pragma once
#include <GameClasses.h>
#include <GadgetClass.h>
#include <SidebarClass.h>
#include <GScreenClass.h>
#include <vector>

// Required for SW.AuxTechnos.Required validation
#include <Ext/SWType/Body.h>

namespace UISafeOps
{
	// Static data
	static std::vector<int>          Queue;
	static std::vector<bool>         PendingFlag;
	static size_t                    Cursor = 0;
	static int                       FrameEnqueued = -1;
	static int                       LastProcessed = -1;
	static int                       BudgetPerFrame = 3;
	static std::vector<GadgetClass*> RemoveQ;
	static int                       FrameEnqueuedRem = -1;

	static inline void ensureCapacity(int idx)
	{
		if (idx >= 0 && idx >= static_cast<int>(PendingFlag.size()))
			PendingFlag.resize(static_cast<size_t>(idx) + 1, false);
	}

	// Configure how many cameo additions to process per frame (default: 3)
	inline void SetPerFrameBudget(int adds) noexcept
	{
		if (adds > 0 && adds < 64) BudgetPerFrame = adds;
	}
	
	// Queue a superweapon cameo for deferred addition to avoid frame-spike
	// IMPORTANT: This function must respect SW.AuxTechnos.Required checks
	inline void EnqueueAddCameo(int swIndex) noexcept
	{
		if (swIndex < 0) return;
		
		// Validate the same requirements as SWSidebarClass::AddButton()
		const auto pSWType = SuperWeaponTypeClass::Array.GetItemOrDefault(swIndex);
		if (!pSWType) return;
		
		const auto pSWExt = SWTypeExt::ExtMap.Find(pSWType);
		if (pSWExt->SW_AuxTechnos_Required && !pSWExt->IsAvailable(HouseClass::CurrentPlayer))
			return; // Don't queue if requirements not met
		
		ensureCapacity(swIndex);
		if (PendingFlag[swIndex]) return;
		PendingFlag[swIndex] = true;
		Queue.push_back(swIndex);
		FrameEnqueued = Unsorted::CurrentFrame;
	}
	
	// Queue a gadget for deferred removal (safer than immediate removal during iteration)
	inline void EnqueueRemoveGadget(GadgetClass* g) noexcept
	{
		if (!g) return;
		RemoveQ.push_back(g);
		FrameEnqueuedRem = Unsorted::CurrentFrame;
	}
	
	// Process all queued operations - call this AFTER layout/sort operations complete
	inline void ProcessDeferredFromSidebarSort() noexcept
	{
		const int now = Unsorted::CurrentFrame;

		// Adds (keep your existing budget)
		if (!Queue.empty() && LastProcessed != now && FrameEnqueued != now)
		{
			int processed = 0;
			while (Cursor < Queue.size() && processed < BudgetPerFrame)
			{
				const int idx = Queue[Cursor++];
				if (idx >= 0 && idx < static_cast<int>(PendingFlag.size()))
					PendingFlag[idx] = false;
				SidebarClass::Instance.AddCameo(AbstractType::Special, idx);
				++processed;
			}
			if (Cursor >= Queue.size()) { Queue.clear(); Cursor = 0; }
		}

		// Removes (cheap — do all, but never same-frame)
		if (!RemoveQ.empty() && FrameEnqueuedRem != now)
		{
			for (auto* g : RemoveQ)
			{
				if (!g) continue;
				GScreenClass::Instance.RemoveButton(g);
				GameDelete(g);
			}
			RemoveQ.clear();
		}

		LastProcessed = now;
	}
}