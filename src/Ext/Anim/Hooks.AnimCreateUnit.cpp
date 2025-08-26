// Anim-to--Unit
// Author: Otamaa, revisions by Starkku

#include "Body.h"

#include <BulletClass.h>
#include <HouseClass.h>
#include <ScenarioClass.h>

#include <Ext/Bullet/Body.h>
#include <Ext/TechnoType/Body.h>
#include <Ext/Techno/Body.h>
#include <Ext/AnimType/Body.h>

DEFINE_HOOK(0x737F6D, UnitClass_TakeDamage_Destroy, 0x7)
{
	GET(UnitClass* const, pThis, ESI);
	REF_STACK(args_ReceiveDamage const, Receivedamageargs, STACK_OFFSET(0x44, 0x4));

	R->ECX(R->ESI());
	TechnoExt::ExtMap.Find(pThis)->ReceiveDamage = true;
	AnimTypeExt::ProcessDestroyAnims(pThis, Receivedamageargs.Attacker);
	pThis->Destroy();                  // ← always call, no guard
	return 0x737F74;
}


DEFINE_HOOK(0x738807, UnitClass_Destroy_DestroyAnim, 0x8)
{
	GET(UnitClass* const, pThis, ESI);

	auto const pExt = TechnoExt::ExtMap.Find(pThis);

	if (!pExt->ReceiveDamage)
		AnimTypeExt::ProcessDestroyAnims(pThis);

	return 0x73887E;
}

// Performance tweak, mark once instead of every frame.
// DEFINE_HOOK(0x423BC8, AnimClass_AI_CreateUnit_MarkOccupationBits, 0x6)
DEFINE_HOOK(0x423BC8, AnimClass_AI_CreateUnit_MarkOccupationBits, 0x6)
{
	GET(AnimClass* const, pThis, ESI);

	const auto pTypeExt = AnimTypeExt::ExtMap.Find(pThis->Type);
	if (pTypeExt->CreateUnitType)
	{
		if (auto* const pCell = pThis->GetCell())
		{
			pThis->MarkAllOccupationBits(pCell->GetCoordsWithBridge());
		}
		// else: no cell yet → do nothing (matches pre-change behavior)
	}
	return 0;
}
DEFINE_HOOK(0x424932, AnimClass_AI_CreateUnit_ActualEffects, 0x6)
{
	GET(AnimClass* const, pThis, ESI);

	auto const pType = pThis->Type;
	auto const pTypeExt = AnimTypeExt::ExtMap.Find(pType);

	if (auto const pCreateUnit = pTypeExt->CreateUnitType.get())
	{
		auto const pUnitType = pCreateUnit->Type;
		auto const pExt = AnimExt::ExtMap.Find(pThis);

		// unmark safely
		if (auto* const pCell = pThis->GetCell())
		{
			pThis->UnmarkAllOccupationBits(pCell->GetCoordsWithBridge());
		}

		// Facing
		const DirType facing = pCreateUnit->RandomFacing
			? static_cast<DirType>(ScenarioClass::Instance->Random.RandomRanged(0, 255))
			: pCreateUnit->Facing;

		// Primary/secondary facings – preserve death/turret inheritance
		const DirType primaryFacing = (pCreateUnit->InheritDeathFacings && pExt->FromDeathUnit)
			? pExt->DeathUnitFacing
			: facing;

		DirType secondaryDir {};        // stable storage for pointer
		DirType* secondaryFacing = nullptr;

		if (pUnitType->WhatAmI() == AbstractType::UnitType
			&& pUnitType->Turret
			&& pExt->FromDeathUnit
			&& pExt->DeathUnitHasTurret
			&& pCreateUnit->InheritTurretFacings)
		{
			secondaryDir = pExt->DeathUnitTurretFacing.GetDir();
			secondaryFacing = &secondaryDir;
			Debug::Log("CreateUnit: Using stored turret facing %d from anim [%s]\n",
				pExt->DeathUnitTurretFacing.GetFacing<256>(), pType->get_ID());
		}

		TechnoTypeExt::CreateUnit(
			pCreateUnit,
			primaryFacing,
			secondaryFacing,
			pThis->Location,
			pThis->Owner,
			pExt->Invoker,
			pExt->InvokerHouse
		);
	}

	return (pType->MakeInfantry != -1) ? 0x42493E : 0x424B31;
}
