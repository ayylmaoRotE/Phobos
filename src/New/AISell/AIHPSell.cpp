#include "New/AISell/AIHPSell.h"

#include <BuildingClass.h>
#include <HouseClass.h>
#include <RulesClass.h>
#include <ScenarioClass.h>
#include <GeneralDefinitions.h>

#include <Ext/Rules/Body.h>   // RulesExt::Global()

namespace
{

	// crossed from >= threshold to < threshold? (no division)
	inline bool CrossedBelowThreshold_FirstTime(int prevHP, int curHP, int maxHP, int thresholdPct)
	{
		if (maxHP <= 0) return false;
		const long long curScaled = (long long)curHP * 100;
		const long long prevScaled = (long long)prevHP * 100;
		const long long gate = (long long)maxHP * thresholdPct;
		return (curScaled < gate) && (prevScaled >= gate);
	}

	inline bool StartSellingNow(BuildingClass* bld)
	{
		if (!bld) return false;
		if (bld->GetCurrentMission() == Mission::Selling) return true;
		bld->SetTarget(nullptr);
		bld->QueueMission(Mission::Selling, false);
		return (bld->GetCurrentMission() == Mission::Selling);
	}

	// Core evaluator used by both public entry points.
	// prevHP/curHP are for this tick (pre vs post apply).
	static void EvaluateWithHP(BuildingClass* bld, int prevHP, int curHP, int appliedDamage)
	{
		if (!bld || !bld->Owner || !bld->Type || appliedDamage <= 0) return;

		auto* rulesEx = RulesExt::Global();
		if (!rulesEx || !rulesEx->AILowHPSell_Enable) return;

		if (rulesEx->AILowHPSell_OnlyAI && bld->Owner->IsControlledByHuman()) return;
		if (rulesEx->AILowHPSell_RespectUnsellable && bld->Type->Unsellable)   return;

		const int maxHP = bld->Type->Strength;
		if (curHP <= 0) return;

		const int thr = Math::clamp((int)rulesEx->AILowHPSell_ThresholdPercent, 1, 100);
		if (!CrossedBelowThreshold_FirstTime(prevHP, curHP, maxHP, thr)) return;

		const int chance = Math::clamp((int)rulesEx->AILowHPSell_Chance, 0, 100);
		if (chance <= 0) return;

		const int roll = ScenarioClass::Instance->Random.RandomRanged(1, 100);
		if (roll > chance) return;

		(void)StartSellingNow(bld);
	}

} // anon

// AFTER HP already subtracted
void AILowHPSell::ConsiderOnDamage(BuildingClass* bld, int appliedDamage)
{
	if (!bld) return;
	const int curHP = bld->Health;            // post-apply
	const int prevHP = curHP + appliedDamage;  // reconstruct pre-apply
	EvaluateWithHP(bld, prevHP, curHP, appliedDamage);
}

// BEFORE HP is subtracted (pre-apply)
void AILowHPSell::ConsiderOnDamage_PreApply(BuildingClass* bld, int prevHP, int appliedDamage)
{
	if (!bld) return;
	int curHP = prevHP - appliedDamage;        // simulate post-apply
	if (curHP < 0) curHP = 0;
	EvaluateWithHP(bld, prevHP, curHP, appliedDamage);
}
