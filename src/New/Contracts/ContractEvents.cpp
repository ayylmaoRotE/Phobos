#include "ContractBountyManager.h"
#include "ContractEvents.h"
#include <Unsorted.h>

namespace Contracts
{
	void OnMoneyEarned(HouseClass* who, int amount) { Manager::Instance().OnMoney(who, amount); }
	void OnKilled(TechnoClass* victim, HouseClass* killerOwner, bool victimIsBuilding) { Manager::Instance().OnKill(victim, killerOwner, victimIsBuilding); }
	void OnBuilt(BuildingClass* built) { Manager::Instance().OnBuild(built); }
	void OnInfiltrated(HouseClass* infiltrator, BuildingClass* target) { Manager::Instance().OnInfiltration(infiltrator, target); }

	// called once per frame from your NewMessageList draw hook:
	void DrawAndTick()
	{
		auto& M = Manager::Instance();

		// Early shared anchor; 1s reduces catch-up work
		constexpr int kSyncStartFrame = 15 * 1;

		// 1) Capture per-match RNG at/after the anchor (defs not required)
		M.CaptureMatchSeedIfDue(kSyncStartFrame);

		// 2) Normalize defs if available (cheap)
		M.EnsureSeeds();

		// 3) As soon as defs exist AND seed is captured, deterministically sync to the current epoch
		if (!M.HasStarted() && M.HasDefinitions() && M.MatchSeedCaptured)
		{
			M.StartOrSyncFromAnchor(kSyncStartFrame);
		}

		M.OnFrame();
		M.DrawUI();
	}

	void LoadRules(CCINIClass* ini) { Manager::Instance().LoadFromRules(ini); }
	void LoadScenarioOverrides(CCINIClass* mapIni)
	{
		Manager::Instance().LoadFromScenario(mapIni);
	}

}
