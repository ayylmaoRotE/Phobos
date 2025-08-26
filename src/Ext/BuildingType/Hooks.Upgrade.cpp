// Upgrade hooks — stability-first, perf-safe

#include <Utilities/Macro.h>
#include <BuildingClass.h>
#include <BuildingTypeClass.h>
#include <HouseClass.h>
#include <Utilities/EnumFunctions.h>
#include "Body.h"
#include <Ext/TechnoType/Body.h>
#include <FactoryClass.h>
#include <Ext/House/Body.h>

#pragma region UpgradeBuildings

// same helper as your original (kept public so callers can reuse)
bool BuildingTypeExt::CanUpgrade(BuildingClass* pBuilding, BuildingTypeClass* pUpgradeType, HouseClass* pUpgradeOwner)
{
	auto const pUpgradeExt = BuildingTypeExt::ExtMap.TryFind(pUpgradeType);
	if (pUpgradeExt && EnumFunctions::CanTargetHouse(pUpgradeExt->PowersUp_Owner, pUpgradeOwner, pBuilding->Owner))
	{
		auto const idx = pBuilding->Type->ID;

		// PowersUpBuilding
		if (_stricmp(idx, pUpgradeType->PowersUpBuilding) == 0)
			return true;

		// PowersUp.Buildings
		for (auto const pPowerUpBuilding : pUpgradeExt->PowersUp_Buildings)
		{
			if (_stricmp(idx, pPowerUpBuilding->ID) == 0)
				return true;
		}
	}
	return false;
}

DEFINE_HOOK(0x452678, BuildingClass_CanUpgrade_UpgradeBuildings, 0x8)
{
	enum { Continue = 0x4526A7, ForbidUpgrade = 0x4526B5 };

	GET(BuildingClass*, pBuilding, ECX);
	GET_STACK(BuildingTypeClass*, pUpgrade, 0xC);
	GET(HouseClass*, pUpgradeOwner, EAX);

	if (BuildingTypeExt::CanUpgrade(pBuilding, pUpgrade, pUpgradeOwner))
	{
		R->EAX(pBuilding->Type->PowersUpToLevel);
		return Continue;
	}
	return ForbidUpgrade;
}

DEFINE_HOOK(0x4408EB, BuildingClass_Unlimbo_UpgradeBuildings, 0xA)
{
	enum { Continue = 0x440912, ForbidUpgrade = 0x440926 };

	GET(BuildingClass*, pBuilding, EDI);
	GET(BuildingClass*, pUpgrade, ESI);

	auto* const pUpType = pUpgrade->Type;

	if (BuildingTypeExt::CanUpgrade(pBuilding, pUpType, pUpgrade->Owner))
	{
		R->EBX(pUpType);
		pUpgrade->SetOwningHouse(pBuilding->Owner, false);
		return Continue;
	}
	return ForbidUpgrade;
}

#pragma endregion

#pragma region UpgradesInteraction

static __forceinline int BuildLimitRemaining(const HouseClass* pHouse, const BuildingTypeClass* pItem)
{
	const int limit = pItem->BuildLimit;
	if (limit >= 0)
	{
		return limit - BuildingTypeExt::GetUpgradesAmount(const_cast<BuildingTypeClass*>(pItem), const_cast<HouseClass*>(pHouse));
	}
	// negative means “total ever owned”
	return -limit - pHouse->CountOwnedEver(pItem);
}

static __forceinline int CheckBuildLimit(const HouseClass* pHouse, const BuildingTypeClass* pItem, const bool includeQueued)
{
	enum { NotReached = 1, ReachedPermanently = -1, ReachedTemporarily = 0 };

	const int limit = pItem->BuildLimit;
	const int remaining = BuildLimitRemaining(pHouse, pItem);

	if (limit >= 0 && remaining <= 0)
	{
		// vanilla semantics: if something is queued already, allow one more to finish
		return (includeQueued && FactoryClass::FindByOwnerAndProduct(pHouse, pItem)) ? NotReached : ReachedPermanently;
	}

	return remaining > 0 ? NotReached : ReachedTemporarily;
}

DEFINE_HOOK(0x4F8361, HouseClass_CanBuild_UpgradesInteraction, 0x5)
{
	GET(const HouseClass*, pThis, ECX);
	GET_STACK(const TechnoTypeClass*, pItem, 0x4);
	GET_STACK(const bool, buildLimitOnly, 0x8);
	GET_STACK(const bool, includeInProduction, 0xC);
	GET(const CanBuildResult, resultOfAres, EAX);

	if (const auto pBldType = abstract_cast<const BuildingTypeClass* const>(pItem))
	{
		if (resultOfAres == CanBuildResult::Buildable
			&& !BuildingTypeExt::ExtMap.Find(const_cast<BuildingTypeClass*>(pBldType))->PowersUp_Buildings.empty())
		{
			R->EAX(CheckBuildLimit(pThis, pBldType, includeInProduction));
		}
	}

	if (resultOfAres == CanBuildResult::Buildable)
	{
		R->EAX(HouseExt::BuildLimitGroupCheck(pThis, pItem, buildLimitOnly, includeInProduction));

		if (HouseExt::ReachedBuildLimit(pThis, pItem, true))
		{
			R->EAX(CanBuildResult::TemporarilyUnbuildable);
		}
	}
	return 0;
}

DEFINE_HOOK(0x4F7877, HouseClass_CanBuild_UpgradesInteraction_WithoutAres, 0x5)
{
	Debug::Log("Hook [HouseClass_CanBuild_UpgradesInteraction] disabled\n");

	Patch::Apply_RAW(0x4F8361, // Disable hook HouseClass_CanBuild_UpgradesInteraction
		{ 0xC2, 0x0C, 0x00, 0x6E, 0x7D }
	);

	Patch::Apply_RAW(0x4F7877, // Disable this hook
		{ 0x53, 0x55, 0x8B, 0xE9, 0x56 }
	);

	return 0;
}

#pragma endregion

#pragma region UpgradeAnimLogic

// Always parse full PowerUp anim info, including power flags, if the building can have upgrades.
DEFINE_HOOK(0x464749, BuildingTypeClass_ReadINI_PowerUpAnims, 0x6)
{
	enum { SkipGameCode = 0x46492E };

	GET(BuildingTypeClass*, pThis, EBP);

	auto* const pTypeExt = BuildingTypeExt::ExtMap.Find(pThis);
	auto* const pINI = &CCINIClass::INI_Art;

	int  index = 1;
	char buffer[0x20];

	pTypeExt->HasPowerUpAnim.clear();

	while ((index - 1) < 3)
	{
		auto* const animData = &pThis->BuildingAnim[index - 1];

		sprintf_s(buffer, "PowerUp%01dAnim", index);
		pINI->GetString(pThis->ImageFile, buffer, animData->Anim);
		pTypeExt->HasPowerUpAnim.emplace_back(GeneralUtils::IsValidString(animData->Anim));

		sprintf_s(buffer, "PowerUp%01dDamagedAnim", index);
		pINI->GetString(pThis->ImageFile, buffer, animData->Damaged);

		sprintf_s(buffer, "PowerUp%01dLocXX", index);
		animData->Position.X = pINI->ReadInteger(pThis->ImageFile, buffer, animData->Position.X);

		sprintf_s(buffer, "PowerUp%01dLocYY", index);
		animData->Position.Y = pINI->ReadInteger(pThis->ImageFile, buffer, animData->Position.Y);

		sprintf_s(buffer, "PowerUp%01dLocZZ", index);
		animData->ZAdjust = pINI->ReadInteger(pThis->ImageFile, buffer, animData->ZAdjust);

		sprintf_s(buffer, "PowerUp%01dYSort", index);
		animData->YSort = pINI->ReadInteger(pThis->ImageFile, buffer, animData->YSort);

		sprintf_s(buffer, "PowerUp%01dPowered", index);
		animData->Powered = pINI->ReadBool(pThis->ImageFile, buffer, animData->Powered);

		sprintf_s(buffer, "PowerUp%01dPoweredLight", index);
		animData->PoweredLight = pINI->ReadBool(pThis->ImageFile, buffer, animData->PoweredLight);

		sprintf_s(buffer, "PowerUp%01dPoweredEffect", index);
		animData->PoweredEffect = pINI->ReadBool(pThis->ImageFile, buffer, animData->PoweredEffect);

		sprintf_s(buffer, "PowerUp%01dPoweredSpecial", index);
		animData->PoweredSpecial = pINI->ReadBool(pThis->ImageFile, buffer, animData->PoweredSpecial);

		++index;
	}

	return SkipGameCode;
}

DEFINE_HOOK(0x440988, BuildingClass_Unlimbo_UpgradeAnims, 0x7)
{
	enum { SkipGameCode = 0x4409C7 };

	GET(BuildingClass*, pThis, ESI); // the *upgrade* object
	GET(BuildingClass*, pTarget, EDI); // the building being upgraded

	auto* const pTargetExt = BuildingExt::ExtMap.Find(pTarget);
	auto* const pType = pThis->Type;

	pTargetExt->PoweredUpToLevel = pTarget->UpgradeLevel + 1;
	int animIndex = pTarget->UpgradeLevel;

	if (pType->PowersUpToLevel > 0)
	{
		pTargetExt->PoweredUpToLevel = Math::max(pType->PowersUpToLevel, pTargetExt->PoweredUpToLevel);
		animIndex = pTargetExt->PoweredUpToLevel - 1;
	}

	auto* const animData = &pTarget->Type->BuildingAnim[animIndex];

	// Only inherit image name if there’s no explicit PowerUp anim for this level.
	if (!pTargetExt->TypeExtData->HasPowerUpAnim[animIndex])
	{
		strncpy(animData->Anim, pType->ImageFile, 16u);
	}

	return SkipGameCode;
}

DEFINE_HOOK(0x451630, BuildingClass_CreateUpgradeAnims_AnimIndex, 0x7)
{
	enum { SkipGameCode = 0x451638 };
	GET(BuildingClass*, pThis, EBP);

	const int animIndex = BuildingExt::ExtMap.Find(pThis)->PoweredUpToLevel - 1;
	if (animIndex)
	{
		R->EAX(animIndex);
		return SkipGameCode;
	}
	return 0;
}

// Don’t create upgrade anims if level doesn’t match or power requirements aren’t met.
// (Perf: tight early-outs; identical semantics.)
static __forceinline bool AllowUpgradeAnim(BuildingClass* pBuilding, BuildingAnimSlot anim)
{
	auto* const pType = pBuilding->Type;

	// fast slot+empty check first
	const int slot = static_cast<int>(anim);
	if (pType->Upgrades != 0
		&& anim >= BuildingAnimSlot::Upgrade1 && anim <= BuildingAnimSlot::Upgrade3
		&& !pBuilding->Anims[slot])
	{
		const int idx = BuildingExt::ExtMap.Find(pBuilding)->PoweredUpToLevel - 1;
		if (idx < 0 || slot != idx)
			return false;

		const auto& data = pType->BuildingAnim[slot];

		const bool needsNormal = (pType->Powered && pType->PowerDrain > 0) && (data.PoweredLight || data.PoweredEffect);
		const bool needsSpecial = pType->PoweredSpecial && data.PoweredSpecial;

		if (needsNormal || needsSpecial)
		{
			const Mission m = pBuilding->CurrentMission;
			const bool active = (m != Mission::Construction && m != Mission::Selling);
			if (!(active && pBuilding->IsPowerOnline()))
				return false;
		}
	}
	return true;
}

DEFINE_HOOK(0x45189D, BuildingClass_AnimUpdate_Upgrades, 0x6)
{
	enum { SkipAnim = 0x451B2C };

	GET(BuildingClass*, pThis, ESI);
	GET_STACK(BuildingAnimSlot, anim, STACK_OFFSET(0x34, 0x8));

	if (!AllowUpgradeAnim(pThis, anim))
		return SkipAnim;

	return 0;
}

#pragma endregion
