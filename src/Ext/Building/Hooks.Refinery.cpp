#include "Body.h"
#include <unordered_map>

// Per-event snapshots (no interleaving races). We also reserve once (on first use)
// to avoid mid-game rehashes that can stutter.
namespace IncomeBefore
{
	static std::unordered_map<const BuildingClass*, long> HarvesterUnloadByDock;
	static std::unordered_map<const TechnoClass*, long>   SlaveReturnByMiner;

	static __forceinline void ReserveOnce()
	{
		static bool inited = false;
		if (!inited)
		{
			HarvesterUnloadByDock.reserve(64);
			SlaveReturnByMiner.reserve(64);
			inited = true;
		}
	}
}

// Unload more than once per ore dump if the harvester contains more than 1 tiberium type
DEFINE_HOOK(0x73E3DB, UnitClass_Mission_Unload_NoteBalanceBefore, 0x6)
{
	GET(HouseClass* const, pHouse, EBX); // house of the refinery
	GET(BuildingClass* const, pDock, EDI); // the dock building at this site (vanilla)

	IncomeBefore::ReserveOnce();
	const long before = pHouse->Available_Money(); // includes silos (vanilla)
	IncomeBefore::HarvesterUnloadByDock[pDock] = before;
	return 0;
}

DEFINE_HOOK(0x73E4D0, UnitClass_Mission_Unload_CheckBalanceAfter, 0xA)
{
	GET(HouseClass* const, pHouse, EBX);
	GET(BuildingClass* const, pDock, EDI);

	if (auto it = IncomeBefore::HarvesterUnloadByDock.find(pDock);
		it != IncomeBefore::HarvesterUnloadByDock.end())
	{
		const long delta = pHouse->Available_Money() - it->second;
		IncomeBefore::HarvesterUnloadByDock.erase(it);

		if (auto const pBldExt = BuildingExt::ExtMap.TryFind(pDock))
		{
			pBldExt->AccumulatedIncome += delta;
		}
	}
	// else: missed 'before' (rare) → no-op to avoid double count (matches safe legacy behavior)
	return 0;
}

DEFINE_HOOK(0x522D50, InfantryClass_SlaveGiveMoney_RecordBalanceBefore, 0x5)
{
	GET_STACK(TechnoClass* const, pSlaveMiner, 0x4);
	IncomeBefore::ReserveOnce();
	IncomeBefore::SlaveReturnByMiner[pSlaveMiner] = pSlaveMiner->Owner->Available_Money();
	return 0;
}

DEFINE_HOOK(0x522E4F, InfantryClass_SlaveGiveMoney_CheckBalanceAfter, 0x6)
{
	GET_STACK(TechnoClass* const, pSlaveMiner, STACK_OFFSET(0x18, 0x4));

	long money = 0;
	if (auto it = IncomeBefore::SlaveReturnByMiner.find(pSlaveMiner);
		it != IncomeBefore::SlaveReturnByMiner.end())
	{
		money = pSlaveMiner->Owner->Available_Money() - it->second;
		IncomeBefore::SlaveReturnByMiner.erase(it);
	}
	// else: no 'before' → no-op (safe)

	if (money != 0)
	{
		if (auto const pBld = abstract_cast<BuildingClass*>(pSlaveMiner))
		{
			BuildingExt::ExtMap.Find(pBld)->AccumulatedIncome += money;
		}
		else if (auto const pBldTypeExt =
		 BuildingTypeExt::ExtMap.TryFind(pSlaveMiner->GetTechnoType()->DeploysInto))
		{
			if (pBldTypeExt->DisplayIncome.Get(RulesExt::Global()->DisplayIncome.Get()))
			{
				FlyingStrings::AddMoneyString(
					money, pSlaveMiner->Owner,
					RulesExt::Global()->DisplayIncome_Houses.Get(),
					pSlaveMiner->Location);
			}
		}
	}
	return 0;
}

// cosmetic anim toggles remain unchanged
DEFINE_HOOK(0x445FE4, BuildingClass_Place_RefineryActiveAnim, 0x6)
{
	GET(BuildingTypeClass*, pType, ESI);
	return BuildingTypeExt::ExtMap.Find(pType)->Refinery_UseNormalActiveAnim ? 0x446183 : 0;
}

DEFINE_HOOK(0x450DAA, BuildingClass_UpdateAnimations_RefineryActiveAnim, 0x6)
{
	GET(BuildingTypeClass*, pType, EDX);
	return BuildingTypeExt::ExtMap.Find(pType)->Refinery_UseNormalActiveAnim ? 0x450F9E : 0;
}
