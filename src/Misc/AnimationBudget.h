#pragma once

#include <cstddef>
#include <algorithm>

// Deterministic, budgeted pruning for heavy anim/particle scenes.
// Designed to be called once per frame/tick; no allocations; single-thread safe.
// Intended integration point: end of UI/tactical draw tick.  [ref] :contentReference[oaicite:11]{index=11}

namespace AnimParticleBudget
{

	// Tunables (keep conservative to preserve visuals)
	struct Params
	{
		std::size_t BudgetNormal = 64;
		std::size_t BudgetBoost = 512;
		std::size_t SoftThresh = 800;
		std::size_t HardThresh = 1600;
		unsigned    BoostFramesSoft = 120; // ~2s at 60 fps
		unsigned    BoostFramesHard = 180; // ~3s at 60 fps
	};

	// External list API we touch — forwarded from your AnimExt
	struct AnimListAPI
	{
		// Returns current vector size
		std::size_t(*size)();
		// (i) -> whether the item at i is still valid or can be erased
		bool        (*shouldErase)(std::size_t i);
		// Erase item at i (stable erase not required)
		void        (*eraseAt)(std::size_t i);
	};

	// Stateless service object (holds internal cursor & boost window)
	class Service
	{
	public:
		explicit Service(const Params& p = Params()) : P(p) { }

		// invoke per frame
		void Tick(const AnimListAPI& api);

	private:
		Params   P;
		std::size_t Cursor = 0;
		unsigned BoostFrames = 0;
		unsigned TickCount = 0;
	};
} // namespace AnimParticleBudget
