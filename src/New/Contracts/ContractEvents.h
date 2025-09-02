#pragma once
#include <HouseClass.h>
#include <TechnoClass.h>
#include <BuildingClass.h>

namespace Contracts
{
	void OnMoneyEarned(HouseClass* who, int amount);
	void OnKilled(TechnoClass* victim, HouseClass* killerOwner, bool victimIsBuilding);
	void OnBuilt(BuildingClass* built);
	void OnInfiltrated(HouseClass* infiltrator, BuildingClass* target);
	void DrawAndTick();
	void LoadRules(CCINIClass* ini);
	void LoadScenarioOverrides(CCINIClass* mapIni);
	void StartContract();
}
#pragma once
