#include "UISafeOps.h"
#include "SidebarClass.h"
#include "GeneralDefinitions.h"
#include <GScreenClass.h>    // RemoveButton(GadgetClass*)
#include <GadgetClass.h>     // full type for GameDelete<T>
#include <vector>

namespace UISafeOps
{

	// ===== existing "add" queue =====
	static std::vector<int>  Queue;
	static std::vector<bool> PendingFlag;
	static size_t            Cursor = 0;
	static int               FrameEnqueued = -1;
	static int               LastProcessed = -1;
	static int               BudgetPerFrame = 3;

	static inline void ensureCapacity(int idx)
	{
		if (idx >= 0 && idx >= static_cast<int>(PendingFlag.size()))
			PendingFlag.resize(static_cast<size_t>(idx) + 1, false);
	}

	void SetPerFrameBudget(int adds) noexcept
	{
		if (adds > 0 && adds < 64) BudgetPerFrame = adds;
	}

	void EnqueueAddCameo(int swIndex) noexcept
	{
		if (swIndex < 0) return;
		ensureCapacity(swIndex);
		if (PendingFlag[swIndex]) return;
		PendingFlag[swIndex] = true;
		Queue.push_back(swIndex);
		FrameEnqueued = Unsorted::CurrentFrame;
	}

	// ===== NEW remove queue (ONE definition only) =====
	static std::vector<::GadgetClass*> RemoveQ;
	static int                         FrameEnqueuedRem = -1;

	void EnqueueRemoveGadget(::GadgetClass* g) noexcept
	{
		if (!g) return;
		RemoveQ.push_back(g);
		FrameEnqueuedRem = Unsorted::CurrentFrame;
	}

	// ===== drain both queues AFTER layout/sort =====
	void ProcessDeferredFromSidebarSort() noexcept
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
