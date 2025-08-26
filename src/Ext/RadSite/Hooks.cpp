#include "Body.h"

#include <BulletClass.h>
#include <HouseClass.h>
#include <InfantryClass.h>
#include <WarheadTypeClass.h>
#include <ScenarioClass.h>

#include <Ext/BuildingType/Body.h>
#include <Ext/Bullet/Body.h>
#include <Ext/Rules/Body.h>
#include <Ext/Techno/Body.h>
#include <Ext/WarheadType/Body.h>
#include <Ext/Cell/Body.h>

#include <Utilities/Macro.h>

#include <algorithm>
#include <numeric>
#include <unordered_map>
#include <utility>
#include <vector>

/*
	Custom Radiations
	Worked out from old uncommented Ares RadSite Hook , adding some more hook
	and rewriting some in order to make this working perfecly
	Credit : Ares Team , for unused/uncommented source of Hook.RadSite
						,RulesData_LoadBeforeTypeData Hook
			Alex-B : GetRadSiteAt ,Helper that used at FootClass_AI & BuildingClass_AI
					Radiate , Uncommented
			me(Otamaa) adding some more stuffs and rewriting hook that cause crash
*/

// ------------------------------------------------------------
// Helper scratch buffers (reused per call to avoid allocations)
// ------------------------------------------------------------
namespace RadTmp
{
	using SitePair = std::pair<RadSiteExt::ExtData*, int>; // (site ext, level)
	using SiteVec = std::vector<SitePair>;
	using TypeBucket = std::pair<RadTypeClass*, SiteVec>;    // (type, sites)
	using TypeMap = std::vector<TypeBucket>;

	static __forceinline TypeMap& GetTypeMap()
	{
		static TypeMap buf;
		return buf;
	}
}

// ------------------------------------------------------------
// Bullet radiation spawning
// ------------------------------------------------------------
DEFINE_HOOK(0x469150, BulletClass_Detonate_ApplyRadiation, 0x5)
{
	GET(BulletClass* const, pThis, ESI);
	GET_BASE(CoordStruct const* const, pCoords, 0x8);

	const auto pWeapon = pThis->GetWeaponType();

	if (pWeapon && pWeapon->RadLevel > 0 && MapClass::Instance.IsWithinUsableArea((*pCoords)))
	{
		const auto pExt = BulletExt::ExtMap.Find(pThis);
		const auto pWH = pThis->WH;
		const auto cell = CellClass::Coord2Cell(*pCoords);
		const auto spread = static_cast<int>(pWH->CellSpread);

		pExt->ApplyRadiationToCell(cell, spread, pWeapon->RadLevel);
	}

	return 0x46920B;
}
#ifndef __clang__
//unused function , safeguard
DEFINE_HOOK(0x46ADE0, BulletClass_ApplyRadiation_Unused, 0x5)
{
	Debug::Log(__FUNCTION__ " called !  You are not supposed to be here!\n");
	return 0x46AE5E;
}
#endif

// ------------------------------------------------------------
// Desolator deploy checks (unchanged logic)
// ------------------------------------------------------------
DEFINE_HOOK(0x5213B4, InfantryClass_AIDeployment_CheckRad, 0x7)
{
	enum { FireCheck = 0x5213F4, SetMissionRate = 0x521484 };

	GET(InfantryClass*, pInfantry, ESI);
	GET(const int, weaponRadLevel, EBX);
	const auto pCell = pInfantry->GetCell();
	const auto pCellExt = CellExt::ExtMap.Find(pCell);
	int radLevel = 0;

	if (!pCellExt->RadSites.empty())
	{
		if (const auto pWeapon = pInfantry->GetDeployWeapon()->WeaponType)
		{
			const auto pWeaponExt = WeaponTypeExt::ExtMap.Find(pWeapon);
			const auto pRadType = pWeaponExt->RadType;
			const float cellSpread = pWeapon->Warhead->CellSpread;

			for (const auto radSite : pCellExt->RadSites)
			{
				if (radSite->Spread == static_cast<int>(cellSpread) && RadSiteExt::ExtMap.Find(radSite)->Type == pRadType)
				{
					radLevel = radSite->GetRadLevel();
					break;
				}
			}
		}
	}

	return (!radLevel || (radLevel < weaponRadLevel / 3))
		? FireCheck : SetMissionRate;
}

// Fix for desolator unable to fire his deploy weapon when cloaked (unchanged)
DEFINE_HOOK(0x521478, InfantryClass_AIDeployment_FireNotOKCloakFix, 0x4)
{
	GET(InfantryClass* const, pThis, ESI);

	const auto pWeapon = pThis->GetDeployWeapon()->WeaponType;
	AbstractClass* pTarget = nullptr; //default WWP nullptr

	if (pWeapon
		&& pWeapon->DecloakToFire
		&& (pThis->CloakState == CloakState::Cloaked || pThis->CloakState == CloakState::Cloaking))
	{
		// stop cloaking immediately so DeployFire can proceed
		const int nDeployFrame = pThis->Type->Sequence->GetSequence(Sequence::DeployedFire).CountFrames;
		pThis->CloakDelayTimer.Start(nDeployFrame);

		pTarget = MapClass::Instance.TryGetCellAt(pThis->GetCoords());
	}

	pThis->SetTarget(pTarget);
	return 0x521484;
}

// ------------------------------------------------------------
// Building radiation application (optimized containers + skip-sort)
// ------------------------------------------------------------
DEFINE_HOOK(0x43FB23, BuildingClass_AI_Radiation, 0x5)
{
	GET(BuildingClass* const, pBuilding, ECX);

	if (pBuilding->Type->ImmuneToRadiation || pBuilding->InLimbo || pBuilding->BeingWarpedOut || pBuilding->TemporalTargetingMe)
		return 0;

	// Global application delay gate
	if (RulesExt::Global()->UseGlobalRadApplicationDelay)
	{
		const int delay = RulesExt::Global()->RadApplicationDelay_Building;
		if (delay == 0 || Unsorted::CurrentFrame % delay)
			return 0;
	}

	const auto buildingCoords = pBuilding->GetMapCoords();

	// NOTE: vanilla code built this every iteration; we still do, but reuse capacity
	auto& typeMap = RadTmp::GetTypeMap();
	typeMap.clear();
	typeMap.reserve(RadTypeClass::Array.size());

	// Count-limiter bookkeeping (kept as-is: behavior unchanged, still not incrementing later)
	std::unordered_map<RadSiteClass*, int> damageCounts;

	for (auto pFoundation = pBuilding->GetFoundationData(false);
		 *pFoundation != CellStruct { 0x7FFF, 0x7FFF }; ++pFoundation)
	{
		const auto nCurrentCoord = buildingCoords + *pFoundation;
		const auto pCell = MapClass::Instance.TryGetCellAt(nCurrentCoord);
		if (!pCell)
			continue;

		const auto pCellExt = CellExt::ExtMap.Find(pCell);
		const size_t approxSites = pCellExt->RadLevels.size();

		for (const auto& [pRadSite, radLevel] : pCellExt->RadLevels)
		{
			if (radLevel <= 0)
				continue;

			// Resolve once; used later during damage
			const auto pRadExt = RadSiteExt::ExtMap.Find(pRadSite);
			const auto pRadType = pRadExt->Type;

			// Per-site limits (kept identical)
			const int maxDamageCount = pRadType->GetBuildingDamageMaxCount();
			if (maxDamageCount > 0 && damageCounts[pRadSite] >= maxDamageCount)
				continue;

			if (!pRadType->GetWarhead())
				continue;

			// Per-type delay (when not using global)
			if (!RulesExt::Global()->UseGlobalRadApplicationDelay)
			{
				const int delay = pRadType->GetBuildingApplicationDelay();
				if (delay == 0 || Unsorted::CurrentFrame % delay)
					continue;
			}

			// Find/create bucket for this type
			auto it = std::find_if(typeMap.begin(), typeMap.end(),
				[pRadType](const RadTmp::TypeBucket& b) { return b.first == pRadType; });

			if (it == typeMap.end())
			{
				RadTmp::SiteVec sites;
				sites.reserve(approxSites);
				sites.emplace_back(pRadExt, radLevel);
				typeMap.emplace_back(pRadType, std::move(sites));
			}
			else
			{
				it->second.emplace_back(pRadExt, radLevel);
			}
		}
	}

	// Process per-type sites
	for (auto& [pRadType, sites] : typeMap)
	{
		const int radLevelMax = pRadType->GetLevelMax();

		// If sum never reaches cap, we can skip sorting altogether
		int sum = 0;
		for (const auto& s : sites) sum += s.second;
		if (sum > radLevelMax)
		{
			std::stable_sort(sites.begin(), sites.end(),
				[](const RadTmp::SitePair& a, const RadTmp::SitePair& b) { return a.second > b.second; });
		}

		int radLevelSum = 0;
		for (const auto& [pSiteExt, radLevel] : sites)
		{
			const int remain = radLevelMax - radLevelSum;
			int damage = static_cast<int>(std::min(radLevel, remain) * pRadType->GetLevelFactor());

			// same check as before, but with pre-resolved ext
			if (pBuilding->IsAlive && !pSiteExt->ApplyRadiationDamage(pBuilding, damage))
				return 0;

			if (radLevel >= remain)
				break;

			radLevelSum += radLevel;
		}
	}

	return 0;
}

// ------------------------------------------------------------
// Foot radiation application (optimized containers + skip-sort)
// ------------------------------------------------------------
DEFINE_HOOK(0x4DA59F, FootClass_AI_Radiation, 0x5)
{
	enum { Continue = 0x4DA63B, ReturnFromFunction = 0x4DAF00 };

	GET(FootClass* const, pFoot, ESI);

	const bool useGlobalDelay = RulesExt::Global()->UseGlobalRadApplicationDelay;

	if (pFoot->IsInPlayfield && !pFoot->TemporalTargetingMe
		&& (!useGlobalDelay || Unsorted::CurrentFrame % RulesClass::Instance->RadApplicationDelay == 0))
	{
		const auto pCell = pFoot->GetCell();
		const auto pCellExt = CellExt::ExtMap.Find(pCell);

		// Reuse buffers
		auto& typeMap = RadTmp::GetTypeMap();
		typeMap.clear();
		typeMap.reserve(RadTypeClass::Array.size());

		const size_t approxSites = pCellExt->RadLevels.size();

		// Group per rad type; resolve Ext once per site
		for (const auto& [pRadSite, radLevel] : pCellExt->RadLevels)
		{
			if (radLevel <= 0)
				continue;

			const auto pRadExt = RadSiteExt::ExtMap.Find(pRadSite);
			const auto pRadType = pRadExt->Type;

			if (!pRadType->GetWarhead())
				continue;

			if (!useGlobalDelay)
			{
				const int delay = pRadType->GetApplicationDelay();
				if (delay == 0 || Unsorted::CurrentFrame % delay)
					continue;
			}

			auto it = std::find_if(typeMap.begin(), typeMap.end(),
				[pRadType](const RadTmp::TypeBucket& b) { return b.first == pRadType; });

			if (it == typeMap.end())
			{
				RadTmp::SiteVec sites;
				sites.reserve(approxSites);
				sites.emplace_back(pRadExt, radLevel);
				typeMap.emplace_back(pRadType, std::move(sites));
			}
			else
			{
				it->second.emplace_back(pRadExt, radLevel);
			}
		}

		// For each type, optionally sort by strongest level first
		for (auto& [pRadType, sites] : typeMap)
		{
			const int radLevelMax = pRadType->GetLevelMax();

			// If aggregate ≤ cap, order does not affect output (no early cap), skip sort
			int sum = 0;
			for (const auto& s : sites) sum += s.second;
			if (sum > radLevelMax)
			{
				std::stable_sort(sites.begin(), sites.end(),
					[](const RadTmp::SitePair& a, const RadTmp::SitePair& b) { return a.second > b.second; });
			}

			int radLevelSum = 0;

			for (const auto& [pSiteExt, radLevel] : sites)
			{
				const int remain = radLevelMax - radLevelSum;
				int damage = static_cast<int>(std::min(radLevel, remain) * pRadType->GetLevelFactor());

				// Keep original liveness/sinking gate
				if ((pFoot->IsAlive || !pFoot->IsSinking) && !pSiteExt->ApplyRadiationDamage(pFoot, damage))
					return ReturnFromFunction;

				if (radLevel >= remain)
					break;

				radLevelSum += radLevel;
			}
		}
	}

	return pFoot->IsAlive ? Continue : ReturnFromFunction;
}

// ------------------------------------------------------------
// Inline helpers for property reads (unchanged)
// ------------------------------------------------------------
#define GET_RADSITE(reg, value)\
	GET(RadSiteClass* const, pThis, reg);\
	RadSiteExt::ExtData* pExt = RadSiteExt::ExtMap.Find(pThis);\
	auto output = pExt->Type-> value ;

/*
//All part of 0x65B580 Hooks is here
DEFINE_HOOK(65B593, RadSiteClass_Activate_Delay, 6)
{
	GET(RadSiteClass* const, pThis, ECX);
	const auto pExt = RadSiteExt::ExtMap.Find(pThis);

	const auto currentLevel = pThis->GetRadLevel();
	auto levelDelay = pExt->Type->GetLevelDelay();
	auto lightDelay = pExt->Type->GetLightDelay();

	if (currentLevel < levelDelay)
	{
		levelDelay = currentLevel;
		lightDelay = currentLevel;
	}

	R->ECX(levelDelay);
	R->EAX(lightDelay);

	return 0x65B59F;
}

DEFINE_HOOK(65B5CE, RadSiteClass_Activate_Color, 6)
{
	GET_RADSITE(ESI, GetColor());

	R->EAX(0);
	R->EDX(0);
	R->EBX(0);

	R->DL(output.G);
	R->EBP(R->EDX());

	R->BL(output.B);
	R->AL(output.R);

	// point out the missing register - Otamaa
	R->EDI(pThis);

	return 0x65B604;
}

DEFINE_HOOK(0x65B63E, RadSiteClass_Activate_LightFactor, 0x6)
{
	GET_RADSITE(ESI, GetLightFactor());

	__asm fmul output;

	return 0x65B644;
}

DEFINE_HOOK_AGAIN(0x65B6A0, RadSiteClass_Activate_TintFactor, 0x6)
DEFINE_HOOK_AGAIN(0x65B6CA, RadSiteClass_Activate_TintFactor, 0x6)
DEFINE_HOOK(0x65B6F2, RadSiteClass_Activate_TintFactor, 0x6)
{
	GET_RADSITE(ESI, GetTintFactor());

	__asm fmul output;

	return R->Origin() + 6;
}
*/

// ------------------------------------------------------------
// RadSite timers / updates (unchanged)
// ------------------------------------------------------------
DEFINE_HOOK(0x65B843, RadSiteClass_AI_LevelDelay, 0x6)
{
	GET_RADSITE(ESI, GetLevelDelay());
	R->ECX(output);
	return 0x65B849;
}

DEFINE_HOOK(0x65B8B9, RadSiteClass_AI_LightDelay, 0x6)
{
	GET_RADSITE(ESI, GetLightDelay());
	R->ECX(output);
	return 0x65B8BF;
}

// Additional Hook below
DEFINE_HOOK(0x65BB67, RadSite_Deactivate, 0x6)
{
	GET_RADSITE(ECX, GetLevelDelay());
	GET(const int, val, EAX);

	R->EAX(val / output);
	R->EDX(val % output);

	return 0x65BB6D;
}

DEFINE_HOOK_AGAIN(0x65BE01, RadSiteClass_UpdateLevel, 0x6)// RadSiteClass_DecreaseRadiation_Decrease
DEFINE_HOOK_AGAIN(0x65BC6E, RadSiteClass_UpdateLevel, 0x6)// RadSiteClass_Deactivate_Decrease
DEFINE_HOOK(0x65BAC1, RadSiteClass_UpdateLevel, 0x8)// RadSiteClass_Radiate_Increase
{
	enum { SkipGameCode = 0x65BB11, SkipGameCode2 = 0x65BCBD, SkipGameCode3 = 0x65BE4C };

	GET(RadSiteClass*, pThis, EDX);
	GET(const int, distance, EAX);
	const int max = pThis->SpreadInLeptons;

	if (distance <= max)
	{
		CellStruct* cell = nullptr;

		if (R->Origin() == 0x65BAC1)
			cell = R->lea_Stack<CellStruct*>(STACK_OFFSET(0x60, -0x4C));
		else if (R->Origin() == 0x65BC6E)
			cell = R->lea_Stack<CellStruct*>(STACK_OFFSET(0x70, -0x5C));
		else
			cell = R->lea_Stack<CellStruct*>(STACK_OFFSET(0x60, -0x50));

		if (const auto pCellExt = CellExt::ExtMap.TryFind(MapClass::Instance.TryGetCellAt(*cell)))
		{
			auto& radLevels = pCellExt->RadLevels;

			const auto it = std::find_if(radLevels.begin(), radLevels.end(), [pThis](CellExt::RadLevel const& item) { return item.Rad == pThis; });

			if (R->Origin() == 0x65BAC1)
			{
				const int level = static_cast<int>(static_cast<double>(max - distance) / max * pThis->RadLevel);

				if (it != radLevels.end())
					it->Level = std::min(it->Level + level, RadSiteExt::ExtMap.Find(pThis)->Type->GetLevelMax());
				else
					radLevels.emplace_back(pThis, level);
			}
			else if (R->Origin() == 0x65BC6E)
			{
				if (it != radLevels.end())
				{
					GET_STACK(const int, stepCount, STACK_OFFSET(0x70, -0x30));
					const int level = static_cast<int>(static_cast<double>(max - distance) / max * pThis->RadLevel / pThis->LevelSteps * stepCount);
					it->Level = std::max(it->Level - std::max(level, 0), 0);
				}
			}
			else
			{
				if (it != radLevels.end())
				{
					const int stepCount = pThis->RadTimeLeft / RadSiteExt::ExtMap.Find(pThis)->Type->GetLevelDelay();
					const int level = static_cast<int>(static_cast<double>(max - distance) / max * pThis->RadLevel / pThis->LevelSteps * stepCount);
					it->Level = std::max(level, 0);
				}
			}
		}
	}

	if (R->Origin() == 0x65BAC1)
		return SkipGameCode;
	else if (R->Origin() == 0x65BC6E)
		return SkipGameCode2;
	else
		return SkipGameCode3;
}
