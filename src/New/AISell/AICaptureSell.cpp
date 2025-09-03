#include "AICaptureSell.h"

#include <InfantryClass.h>
#include <InfantryTypeClass.h>
#include <HouseClass.h>
#include <BuildingTypeClass.h>
#include <ScenarioClass.h>
#include <Unsorted.h>
#include <GeneralDefinitions.h>

#include <Ext/BuildingType/Body.h> // BuildingTypeExt::ExtMap

namespace
{

	inline bool IsEnemyEngineer(TechnoClass* from, HouseClass* owner)
	{
		if (!from || !owner) return false;
		auto* inf = abstract_cast<InfantryClass*>(from);
		if (!inf || !inf->Type || !inf->Type->Engineer) return false;
		return !inf->Owner || !inf->Owner->IsAlliedWith(owner);
	}

	inline bool IsEngineerAdjacent(InfantryClass* inf, AbstractClass* target)
	{
		return inf && target
			? (inf->DistanceFrom(target) <= Unsorted::LeptonsPerCell) // ~door distance
			: false;
	}

	inline bool IsCaptureEligible(BuildingClass* bld)
	{
		if (!bld || !bld->Type) return false;
		const auto* bt = bld->Type;
		return (bt->NeedsEngineer || bt->Capturable);
	}

	inline bool IsHuman(HouseClass* h)
	{
		return h && h->IsControlledByHuman();
	}

	// Start the Selling mission now (no event). Returns true if the mission switched.
	inline bool StartSellingNow(BuildingClass* bld)
	{
		if (!bld) return false;

		if (bld->GetCurrentMission() == Mission::Selling)
		{
			return true; // already selling
		}

		bld->SetTarget(nullptr);
		bld->QueueMission(Mission::Selling, false); // follow stock flow

		return (bld->GetCurrentMission() == Mission::Selling);
	}

} // anon

bool AICaptureSell::TryAtEngineerDoor(BuildingClass* bld, TechnoClass* from)
{
	if (!bld || !bld->Type || !bld->Owner) return false;

	// AI only
	if (IsHuman(bld->Owner)) return false;

	// Must be enemy engineer, capture-eligible target, and adjacent
	if (!IsEnemyEngineer(from, bld->Owner)) return false;
	if (!IsCaptureEligible(bld)) return false;

	auto* inf = abstract_cast<InfantryClass*>(from);
	if (!IsEngineerAdjacent(inf, bld)) return false;

	// If already selling, just block the enter this tick
	if (bld->GetCurrentMission() == Mission::Selling)
	{
		if (inf) { inf->SetDestination(nullptr, false); }
		return true;
	}

	// INI controls
	auto* ext = BuildingTypeExt::ExtMap.Find(bld->Type);
	if (!ext || !(bool)ext->AICaptureSell_Enable) return false;

	if ((bool)ext->AICaptureSell_RespectUnsellable && bld->Type->Unsellable)
	{
		return false;
	}

	// NEW: HP% gate — only trigger when current HP% is strictly below the threshold
	{
		const int maxHP = bld->Type->Strength;     // rules Strength (max HP)
		const int curHP = bld->Health;             // Techno current HP
		const int hpPct = (maxHP > 0) ? (curHP * 100) / maxHP : 0;
		const int thresh = static_cast<int>(ext->AICaptureSell_HealthBelowPercent);
		if (hpPct >= thresh)
		{
			return false; // not low enough yet
		}
	}

	const int chance = static_cast<int>(ext->AICaptureSell_Chance);
	if (chance <= 0) return false;

	// deterministic RNG
	const int roll = ScenarioClass::Instance->Random.RandomRanged(1, 100);
	if (roll > chance)
	{
		return false;
	}

	// start selling; only cancel enter if we actually switched
	if (StartSellingNow(bld))
	{
		if (inf) { inf->SetDestination(nullptr, false); }
		return true;
	}

	return false;
}
