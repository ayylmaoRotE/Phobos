#include "Body.h"

#include <ScenarioClass.h>
#include <WarheadTypeClass.h>

#include <Ext/Anim/Body.h>
#include <Ext/AnimType/Body.h>
#include <Ext/WarheadType/Body.h>
#include <Ext/WeaponType/Body.h>
#include <Utilities/Macro.h>

#include <cmath>

namespace AnimLoggingTemp
{
	DWORD UniqueID = 0;
	AnimTypeClass* pType = nullptr;
}

// helpers – return ExtData* (matches your ExtMap containers)
namespace _AnimPerf
{
	__forceinline AnimTypeExt::ExtData* TypeExt(AnimTypeClass* t) { return AnimTypeExt::ExtMap.Find(t); }
	__forceinline AnimExt::ExtData* Ext(AnimClass* a) { return AnimExt::ExtMap.Find(a); }
}

DEFINE_HOOK(0x423B95, AnimClass_AI_HideIfNoOre_Threshold, 0x8)
{
	GET(AnimClass* const, pThis, ESI);
	GET(AnimTypeClass* const, pType, EDX); // keep original register read

	AnimLoggingTemp::UniqueID = pThis->UniqueID;
	AnimLoggingTemp::pType = pThis->Type;

	if (!pType || !pType->HideIfNoOre)
		return 0x423BBF;

	const auto pTypeExt = _AnimPerf::TypeExt(pType);
	const int  threshold = std::abs(pTypeExt->HideIfNoOre_Threshold.Get());

	if (auto* const pCell = pThis->GetCell())
	{
		pThis->Invisible = (pCell->GetContainedTiberiumValue() <= threshold);
	}

	return 0x423BBF;
}

// Nuke Ares' animation damage hook at 0x424538.
DEFINE_PATCH(0x424538, 0x8B, 0x8E, 0xCC, 0x00, 0x00, 0x00);

// New damage logic (deterministic, guarded)
DEFINE_HOOK(0x42453E, AnimClass_AI_Damage, 0x6)
{
	enum { SkipDamage = 0x42465D, Continue = 0x42464C };

	GET(AnimClass*, pThis, ESI);
	if (!pThis || pThis->IsInert)
		return SkipDamage;

	auto* const pType = pThis->Type;
	if (!pType)
		return SkipDamage;

	auto* const pTypeExt = _AnimPerf::TypeExt(pType);
	auto* const pOwnerObj = pThis->OwnerObject;
	const int   delay = pTypeExt->Damage_Delay.Get();
	const bool  isTerrain = pOwnerObj && (pOwnerObj->WhatAmI() == AbstractType::Terrain);
	const int   mul = isTerrain ? 5 : 1;
	const double base = pType->Damage;

	int applied = 0;

	// Apply once per loop
	if (pTypeExt->Damage_ApplyOncePerLoop)
	{
		if (pThis->Animation.Value == std::max(delay - 1, 1))
			applied = static_cast<int>(std::round(base)) * mul;
		else
			return SkipDamage;
	}
	// Fractional/no delay
	else if (delay <= 0 || base < 1.0)
	{
		const double total = mul * base + pThis->Accum;
		if (total >= 1.0)
		{
			applied = static_cast<int>(std::round(total));
			pThis->Accum = total - applied;
		}
		else
		{
			pThis->Accum = total;
			return SkipDamage;
		}
	}
	// Fixed delay ticks
	else
	{
		pThis->Accum += 1.0;
		if (pThis->Accum < delay)
			return SkipDamage;

		applied = static_cast<int>(std::round(base)) * mul;
		pThis->Accum = 0.0;
	}

	if (applied <= 0)
		return SkipDamage;

	// Resolve invoker & owner (guarded)
	auto* const pAnimExt = _AnimPerf::Ext(pThis);
	TechnoClass* pInvoker = nullptr;
	HouseClass* pOwner = pThis->Owner;

	if (pTypeExt->Damage_DealtByInvoker)
	{
		pInvoker = pAnimExt->Invoker;

		if (!pInvoker)
		{
			if (pOwnerObj)
				pInvoker = abstract_cast<TechnoClass*, true>(pOwnerObj);
			else if (pThis->IsBuildingAnim)
				pInvoker = pAnimExt->ParentBuilding;
		}

		if (pAnimExt->InvokerHouse)
			pOwner = pAnimExt->InvokerHouse;

		if (pInvoker)
		{
			if (!pAnimExt->InvokerHouse)
				pOwner = pInvoker->Owner;

			if (pTypeExt->Damage_ApplyFirepowerMult)
				applied = static_cast<int>(applied * TechnoExt::GetCurrentFirepowerMultiplier(pInvoker));
		}
	}

	if (!pOwner)
	{
		if (pOwnerObj)
			pOwner = pOwnerObj->GetOwningHouse();
		else if (pThis->IsBuildingAnim && pAnimExt->ParentBuilding)
			pOwner = pAnimExt->ParentBuilding->Owner;
	}

	const auto coords = pThis->GetCoords();

	if (pTypeExt->Weapon)
	{
		WeaponTypeExt::DetonateAt(pTypeExt->Weapon, coords, pInvoker, applied, pOwner);
	}
	else
	{
		auto* pWH = pType->Warhead;
		if (!pWH)
		{
			pWH = std::strcmp(pType->get_ID(), "INVISO") ? RulesClass::Instance->FlameDamage2
				: RulesClass::Instance->C4Warhead;
		}
		MapClass::DamageArea(coords, applied, pInvoker, pWH, true, pOwner);
	}

	return Continue;
}

DEFINE_HOOK(0x42465D, AnimClass_AI_NullTypeCheck, 0x6)
{
	GET(AnimClass*, pThis, ESI);

	if (!pThis->Type)
	{
		char buffer[28];

		if (AnimLoggingTemp::UniqueID == pThis->UniqueID && AnimLoggingTemp::pType)
			sprintf_s(buffer, sizeof(buffer), " [%s]", AnimLoggingTemp::pType->get_ID());
		else
			sprintf_s(buffer, sizeof(buffer), "");

		auto coords = pThis->Location;
		auto map = pThis->GetMapCoords();

		Debug::FatalErrorAndExit(
			"AnimClass_AI_NullTypeCheck: Animation%s has null type. Active: %d | Inert: %d | Coords: %d,%d,%d | Cell: %d,%d\n",
			buffer, pThis->IsAlive, pThis->IsInert, coords.X, coords.Y, coords.Z, map.X, map.Y
		);
	}

	AnimLoggingTemp::UniqueID = 0;
	AnimLoggingTemp::pType = nullptr;

	return 0;
}

DEFINE_HOOK(0x4242E1, AnimClass_AI_TrailerAnim, 0x5)
{
	enum { SkipGameCode = 0x424322 };

	GET(AnimClass*, pThis, ESI);

	auto* const pType = pThis->Type;
	if (!pType)
		return SkipGameCode;

	// TrailerAnim is an AnimTypeClass*, not an index
	AnimTypeClass* const pTrailerType = pType->TrailerAnim;
	if (!pTrailerType)
		return SkipGameCode; // nothing to create

	const auto coords = pThis->GetCoords();

	//  GameCreate expects AnimTypeClass* here
	AnimClass* const pTrailerAnim = GameCreate<AnimClass>(pTrailerType, coords, 1, 1);
	if (!pTrailerAnim)
		return SkipGameCode;

	auto* const pTrailerAnimExt = AnimExt::ExtMap.Find(pTrailerAnim);
	auto* const pExt = _AnimPerf::Ext(pThis);

	AnimExt::SetAnimOwnerHouseKind(pTrailerAnim, pThis->Owner, nullptr, false, true);
	pTrailerAnimExt->SetInvoker(pExt->Invoker, pExt->InvokerHouse);

	return SkipGameCode;
}

// Deferred creation of attached particle systems for debris anims.
DEFINE_HOOK(0x423939, AnimClass_BounceAI_AttachedSystem, 0x6)
{
	GET(AnimClass*, pThis, EBP);
	_AnimPerf::Ext(pThis)->CreateAttachedSystem();
	return 0;
}

DEFINE_HOOK(0x62E08B, ParticleSystemClass_DTOR_DetachAttachedSystem, 0x7)
{
	GET(ParticleSystemClass*, pParticleSystem, EDI);

	if (pParticleSystem->Owner && pParticleSystem->Owner->WhatAmI() == AbstractType::Anim)
		AnimExt::InvalidateParticleSystemPointers(pParticleSystem);

	return 0;
}

DEFINE_HOOK(0x423CC7, AnimClass_AI_HasExtras_Expired, 0x6)
{
	enum { SkipGameCode = 0x423EFD };

	GET(AnimClass* const, pThis, ESI);
	GET(bool const, heightFlag, EAX);

	if (!pThis || !pThis->Type)
		return SkipGameCode;

	auto* const pType = pThis->Type;
	auto* const pTypeExt = _AnimPerf::TypeExt(pType);

	const auto splashAnims = pTypeExt->SplashAnims.GetElements(RulesClass::Instance->SplashList);
	const int  damage = static_cast<int>(pType->Damage);
	auto* const pOwner = AnimExt::GetOwnerHouse(pThis);

	AnimExt::HandleDebrisImpact(
		pType->ExpireAnim,
		pTypeExt->WakeAnim,
		splashAnims,
		pOwner,
		pType->Warhead,
		damage,
		pThis->GetCell(),
		pThis->Location,
		heightFlag,
		pType->IsMeteor,
		pTypeExt->Warhead_Detonate,
		pTypeExt->ExplodeOnWater,
		pTypeExt->SplashAnims_PickRandom
	);

	return SkipGameCode;
}

DEFINE_HOOK(0x424807, AnimClass_AI_Next, 0x6)
{
	GET(AnimClass*, pThis, ESI);

	auto* const pExt = _AnimPerf::Ext(pThis);
	auto* const pTypeExt = _AnimPerf::TypeExt(pThis->Type);

	if (pExt->AttachedSystem && (pExt->AttachedSystem->Type != pTypeExt->AttachedSystem.Get()))
		pExt->DeleteAttachedSystem();

	if (!pExt->AttachedSystem && pTypeExt->AttachedSystem)
		pExt->CreateAttachedSystem();

	return 0;
}

DEFINE_HOOK(0x424CF1, AnimClass_Start_DetachedReport, 0x6)
{
	GET(AnimClass*, pThis, ESI);

	auto* const pType = pThis->Type;
	if (!pType) return 0;

	auto* const pTypeExt = _AnimPerf::TypeExt(pType);
	if (pTypeExt->DetachedReport >= 0)
		VocClass::PlayAt(pTypeExt->DetachedReport.Get(), pThis->GetCoords());

	return 0;
}

// 0x422CD8 is in an alternate code path only used by anims with ID RING1
DEFINE_HOOK_AGAIN(0x422CD8, AnimClass_DrawIt_XDrawOffset, 0x6)
DEFINE_HOOK(0x423122, AnimClass_DrawIt_XDrawOffset, 0x6)
{
	GET(AnimClass* const, pThis, ESI);
	GET_STACK(Point2D*, pLocation, STACK_OFFSET(0x110, 0x4));

	if (const auto* pTypeExt = AnimTypeExt::ExtMap.TryFind(pThis->Type))
		pLocation->X += pTypeExt->XDrawOffset;

	return 0;
}

#pragma region AttachedAnims

DEFINE_HOOK(0x424CB0, AnimClass_InWhichLayer_AttachedObjectLayer, 0x6)
{
	enum { ReturnValue = 0x424CBF };

	GET(AnimClass*, pThis, ECX);

	if (pThis->OwnerObject)
	{
		const auto* pTypeExt = _AnimPerf::TypeExt(pThis->Type);

		if (pTypeExt->Layer_UseObjectLayer.isset())
		{
			Layer layer = pThis->Type->Layer;

			if (pTypeExt->Layer_UseObjectLayer.Get())
				layer = pThis->OwnerObject->InWhichLayer();

			R->EAX(layer);
			return ReturnValue;
		}
	}
	return 0;
}

DEFINE_HOOK(0x424C3D, AnimClass_AttachTo_AttachedAnimPosition, 0x6)
{
	enum { SkipGameCode = 0x424C76 };

	GET(AnimClass*, pThis, ESI);

	const auto* pTypeExt = _AnimPerf::TypeExt(pThis->Type);

	if (pTypeExt->AttachedAnimPosition != AttachedAnimPosition::Default)
	{
		pThis->SetLocation(CoordStruct::Empty);
		return SkipGameCode;
	}

	return 0;
}


class AnimClassFake final : public AnimClass
{
	CoordStruct* _GetCenterCoords(CoordStruct* pCrd) const;
};

CoordStruct* AnimClassFake::_GetCenterCoords(CoordStruct* pCrd) const
{
	*pCrd = this->Location;

	if (auto* const pObject = this->OwnerObject)
	{
		*pCrd += pObject->GetCoords();

		if (AnimTypeExt::ExtMap.Find(this->Type)->AttachedAnimPosition == AttachedAnimPosition::Ground)
			pCrd->Z = MapClass::Instance.GetCellFloorHeight(*pCrd);
	}

	return pCrd;
}

DEFINE_FUNCTION_JUMP(VTABLE, 0x7E339C, AnimClassFake::_GetCenterCoords);

#pragma endregion

DEFINE_HOOK(0x4236F0, AnimClass_DrawIt_Tiled_Palette, 0x6)
{
	GET(AnimClass*, pThis, ESI);

	R->EDX(_AnimPerf::TypeExt(pThis->Type)->Palette.GetOrDefaultConvert(FileSystem::ANIM_PAL));

	return 0x4236F6;
}

DEFINE_HOOK(0x423365, AnimClass_DrawIt_ExtraShadow, 0x8)
{
	enum { DrawExtraShadow = 0x42336D, SkipExtraShadow = 0x4233EE };

	GET(AnimClass*, pThis, ESI);

	if (pThis->HasExtras)
	{
		if (!_AnimPerf::TypeExt(pThis->Type)->ExtraShadow)
			return SkipExtraShadow;

		return DrawExtraShadow;
	}

	return SkipExtraShadow;
}

// Apply cell lighting on UseNormalLight=no MakeInfantry anims.
DEFINE_HOOK(0x4232BF, AnimClass_DrawIt_MakeInfantry, 0x6)
{
	enum { SkipGameCode = 0x4232C5 };

	GET(AnimClass*, pThis, ESI);

	if (pThis->Type->MakeInfantry != -1)
	{
		if (auto* const pCell = pThis->GetCell())
		{
			R->EAX(pCell->Intensity_Normal);
			return SkipGameCode;
		}
	}

	return 0;
}

DEFINE_HOOK(0x423061, AnimClass_DrawIt_Visibility, 0x6)
{
	enum { SkipDrawing = 0x4238A3 };

	GET(AnimClass* const, pThis, ESI);

	const auto* pTypeExt = _AnimPerf::TypeExt(pThis->Type);

	if (!pTypeExt->RestrictVisibilityIfCloaked && pTypeExt->VisibleTo == AffectedHouse::All)
		return 0;

	TechnoClass* pTechno = abstract_cast<TechnoClass*>(pThis->OwnerObject);
	HouseClass* const pCurrentHouse = HouseClass::CurrentPlayer;

	if (!pTechno)
	{
		const auto* pExt = _AnimPerf::Ext(pThis);

		if (pExt->IsTechnoTrailerAnim)
			pTechno = pExt->Invoker;
	}

	if (pTypeExt->RestrictVisibilityIfCloaked && !HouseClass::IsCurrentPlayerObserver()
		&& pTechno && (pTechno->CloakState == CloakState::Cloaked || pTechno->CloakState == CloakState::Cloaking)
		&& !pTechno->Owner->IsAlliedWith(pCurrentHouse)
		&& !pTechno->GetCell()->Sensors_InclHouse(pCurrentHouse->ArrayIndex))
	{
		return SkipDrawing;
	}

	HouseClass* pOwner = pThis->OwnerObject ? pThis->OwnerObject->GetOwningHouse() : pThis->Owner;

	if (pTypeExt->VisibleTo_ConsiderInvokerAsOwner)
	{
		const auto* pExt = _AnimPerf::Ext(pThis);

		if (pExt->Invoker)
			pOwner = pExt->Invoker->Owner;
		else if (pExt->InvokerHouse)
			pOwner = pExt->InvokerHouse;
	}

	if (!HouseClass::IsCurrentPlayerObserver() && !EnumFunctions::CanTargetHouse(pTypeExt->VisibleTo, pCurrentHouse, pOwner))
		return SkipDrawing;

	return 0;
}

#pragma region AltPalette

// Fix AltPalette anims not using owner color scheme.
DEFINE_HOOK(0x4232E2, AnimClass_DrawIt_AltPalette, 0x6)
{
	enum { SkipGameCode = 0x4232EA };

	GET(AnimClass*, pThis, ESI);

	int schemeIndex = pThis->Owner
		? (pThis->Owner->ColorSchemeIndex - 1)
		: RulesExt::Global()->AnimRemapDefaultColorScheme;

	schemeIndex += _AnimPerf::TypeExt(pThis->Type)->AltPalette_ApplyLighting ? 1 : 0;

	const auto scheme = ColorScheme::Array[schemeIndex];

	R->ECX(scheme);
	return SkipGameCode;
}

// Set ShadeCount to 53 to initialize the palette fully shaded - this is required to make it not draw over shroud for some reason.
DEFINE_HOOK(0x68C4C4, GenerateColorSpread_ShadeCountSet, 0x5)
{
	GET(const int, shadeCount, EDX);

	if (shadeCount == 1)
		R->EDX(53);

	return 0;
}

#pragma endregion

DEFINE_HOOK(0x425174, AnimClass_Detach_Cloak, 0x6)
{
	enum { SkipDetaching = 0x4251A3 };

	GET(AnimClass*, pThis, ESI);
	GET(AbstractClass*, pTarget, EDI);

	if (const auto* pTypeExt = AnimTypeExt::ExtMap.TryFind(pThis->Type))
	{
		if (!pTypeExt->DetachOnCloak)
		{
			if (const auto pTechno = abstract_cast<TechnoClass*>(pTarget))
			{
				if (TechnoExt::ExtMap.Find(pTechno)->IsDetachingForCloak)
					return SkipDetaching;
			}
		}
	}

	return 0;
}

#pragma region ScorchFlamer

// Disable Ares' implementation.
DEFINE_PATCH(0x42511B, 0x5F, 0x5E, 0x5D, 0x5B, 0x83, 0xC4, 0x20);
DEFINE_PATCH(0x4250C9, 0x5F, 0x5E, 0x5D, 0x5B, 0x83, 0xC4, 0x20);
DEFINE_PATCH(0x42513F, 0x5F, 0x5E, 0x5D, 0x5B, 0x83, 0xC4, 0x20);

DEFINE_HOOK(0x425060, AnimClass_Expire_ScorchFlamer, 0x6)
{
	GET(AnimClass*, pThis, ESI);

	if (auto* const pType = pThis->Type)
	{
		if (pType->Flamer || pType->Scorch)
			AnimExt::SpawnFireAnims(pThis);
	}

	return 0;
}

#pragma endregion

DEFINE_HOOK(0x4250E1, AnimClass_Middle_CraterDestroyTiberium, 0x6)
{
	enum { SkipDestroyTiberium = 0x4250EC };
	GET(AnimTypeClass*, pType, EDX);
	return AnimTypeExt::ExtMap.Find(pType)->Crater_DestroyTiberium.Get(RulesExt::Global()->AnimCraterDestroyTiberium) ? 0 : SkipDestroyTiberium;
}
