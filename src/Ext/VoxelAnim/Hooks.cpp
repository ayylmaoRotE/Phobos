#include "Body.h"

#include <ScenarioClass.h>

#include <Ext/Anim/Body.h>
#include <Ext/AnimType/Body.h>
#include <Ext/VoxelAnimType/Body.h>
#include <Ext/WarheadType/Body.h>

DEFINE_HOOK(0x74A70E, VoxelAnimClass_AI_Additional, 0xC)
{
	GET(VoxelAnimClass* const, pThis, EBX);

	const auto* const pThisExt = VoxelAnimExt::ExtMap.Find(pThis);
	if (!pThisExt || pThisExt->LaserTrails.empty())
		return 0;

	const CoordStruct location = pThis->GetCoords();
	const bool visible = pThis->IsVisible;

	for (const auto& pTrail : pThisExt->LaserTrails)
	{
		if (!pTrail->LastLocation.isset())
			pTrail->LastLocation = location;

		pTrail->Visible = visible;
		pTrail->Update(location);
	}
	return 0;
}

DEFINE_HOOK(0x74A027, VoxelAnimClass_AI_Expired, 0x6)
{
	enum { SkipGameCode = 0x74A22A };

	GET(VoxelAnimClass* const, pThis, EBX);
	GET(const int, flag, EAX);

	const bool heightFlag = flag & 0xFF;
	auto const pType = pThis->Type;
	auto const pTypeExt = VoxelAnimTypeExt::ExtMap.Find(pType);
	auto const splashAnims = pTypeExt->SplashAnims.GetElements(RulesClass::Instance->SplashList);

	AnimExt::HandleDebrisImpact(pType->ExpireAnim, pTypeExt->WakeAnim, splashAnims, pThis->OwnerHouse, pType->Warhead, pType->Damage,
		pThis->GetCell(), pThis->Location, heightFlag, pType->IsMeteor, pTypeExt->Warhead_Detonate, pTypeExt->ExplodeOnWater, pTypeExt->SplashAnims_PickRandom);

	return SkipGameCode;
}

DEFINE_HOOK(0x74A70E, VoxelAnimClass_AI_Trailer, 0x6)
{
	enum { SkipGameCode = 0x74A7AB };
	GET(VoxelAnimClass* const, pThis, EBX);

	auto* const pType = pThis->Type;
	if (const auto pAnimType = pType ? pType->TrailerAnim : nullptr)
	{
		auto* const pExt = VoxelAnimExt::ExtMap.Find(pThis);
		if (pExt && pExt->TrailerSpawnTimer.Expired())
		{
			const auto delay =
				VoxelAnimTypeExt::ExtMap.Find(pType)->Trailer_SpawnDelay.Get();
			pExt->TrailerSpawnTimer.Start(delay);

			if (auto* const pTrailerAnim =
					GameCreate<AnimClass>(pAnimType, pThis->Location, 1, 1))
			{
				AnimExt::SetAnimOwnerHouseKind(
					pTrailerAnim, pThis->OwnerHouse, nullptr, false, true);
			}
			// if creation fails, we simply try again after 'delay' — matches old cadence
		}
	}
	return SkipGameCode;
}
