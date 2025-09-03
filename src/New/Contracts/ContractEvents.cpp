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
		constexpr int kSyncStartFrame = 15 * 1;

		// 1) capture match seed at/after shared frame
		M.CaptureMatchSeedIfDue(kSyncStartFrame);

		// 2) normalize defs if they exist
		M.EnsureSeeds();

		// 3) once defs+seed exist, sync to current epoch from shared anchor
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
