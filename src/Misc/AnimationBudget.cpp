// Misc/AnimParticleBudget.cpp
#include "AnimationBudget.h"

// This wraps the logic you already prototyped (cursor + budgets), keeping it deterministic.
// It never iterates the whole list in a single tick, smoothing spikes.  [ref] :contentReference[oaicite:12]{index=12}

namespace AnimParticleBudget
{

	void Service::Tick(const AnimListAPI& api)
	{
		// Do light work most frames; heavier when overloaded.
		if (BoostFrames == 0 && ((++TickCount & 0xFFu) != 0))
		{
			return;
		}

		const std::size_t n = api.size ? api.size() : 0;

		// Arm or extend boost window based on pressure.
		if (n > P.HardThresh)
		{
			BoostFrames = P.BoostFramesHard;
		}
		else if (n > P.SoftThresh)
		{
			BoostFrames = std::max<unsigned>(BoostFrames, P.BoostFramesSoft);
		}

		const std::size_t budget = BoostFrames ? P.BudgetBoost : P.BudgetNormal;
		if (BoostFrames) { --BoostFrames; }

		std::size_t visited = 0;
		while (visited < budget && n > 0)
		{
			if (Cursor >= n) { Cursor = 0; }

			// Query once; if item should go, erase and DO NOT advance cursor
			// (so we inspect the element that slid down into [Cursor]).
			if (api.shouldErase && api.shouldErase(Cursor))
			{
				api.eraseAt(Cursor);
				// No ++Cursor here by design.
			}
			else
			{
				++Cursor;
			}
			++visited;
		}
	}

} // namespace AnimParticleBudget
