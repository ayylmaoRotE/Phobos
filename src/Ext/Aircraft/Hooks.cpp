// Hybrid_Aircraft_Missions.cpp
#include "Body.h"

#include <AircraftClass.h>
#include <EventClass.h>
#include <FlyLocomotionClass.h>

#include <Ext/Aircraft/Body.h>
#include <Ext/Techno/Body.h>
#include <Ext/Anim/Body.h>
#include <Ext/WeaponType/Body.h>
#include <Ext/BulletType/Body.h>
#include <Utilities/Macro.h>


#pragma region Mission_Attack

// ----- DEBUG (safe to compile out) -----
#ifndef ACDBG
#define ACDBG 1
#endif

#if ACDBG
static const char* MissionStr(Mission m)
{
	switch (m)
	{
	case Mission::None: return "None";
	case Mission::Sleep: return "Sleep";
	case Mission::Move: return "Move";
	case Mission::Attack: return "Attack";
	case Mission::Area_Guard: return "Area_Guard";
	case Mission::Return: return "Return";
	default: return "Other";
	}
}
#define DBG(msg, ...) Debug::Log("[AC] " msg "\n", __VA_ARGS__)
#else
#define DBG(...) do{}while(0)
#endif

static __forceinline bool AC_IsHeadingToDock(const AircraftClass* ac) noexcept
{
	if (!ac || !ac->Type || !ac->Type->AirportBound) return false;
	if (ac->Target) return false;
	if (ac->DockNowHeadingTo) return true;
	auto* const dest = ac->Destination;
	return dest && (dest == ac->DockNowHeadingTo || dest == ac->SpawnOwner);
}

// Clamp a point to a valid cell by walking it toward the destination.
static __forceinline bool ClampToMapLine(CoordStruct& p, const CoordStruct& toward, int stepLeptons)
{
	// Fast bail: already valid
	if (MapClass::Instance.TryGetCellAt(p)) return true;

	// Walk toward 'toward' point in small steps until we hit the map
	const auto dx = toward.X - p.X;
	const auto dy = toward.Y - p.Y;
	const double len = std::max(1.0, std::sqrt((double)dx * dx + (double)dy * dy));
	const double ux = dx / len, uy = dy / len;

	for (int i = 0; i < 64; ++i)
	{ // 64 small nudges is plenty
		p.X += (int)std::round(ux * stepLeptons);
		p.Y += (int)std::round(uy * stepLeptons);
		if (MapClass::Instance.TryGetCellAt(p)) return true;
	}
	return false;
}



static __forceinline bool IsHeadingToDock(const AircraftClass* pAC) noexcept
{
	if (!pAC || !pAC->Type || !pAC->Type->AirportBound) return false;
	if (pAC->Target) return false;
	if (pAC->DockNowHeadingTo) return true;
	auto* const dest = pAC->Destination;
	return dest && (dest == pAC->DockNowHeadingTo || dest == pAC->SpawnOwner);
}

// NEW: Only commit to landing when orders are actually finished.
static __forceinline bool HasBlockingOrders(const AircraftClass* pAC) noexcept
{
	if (!pAC) return false;

	// Still executing an attack pass / burst
	if (pAC->IsLocked) return true;

	// Active attack
	if (pAC->CurrentMission == Mission::Attack) return true;

	// Queued player Attack must complete first
	if (pAC->QueuedMission == Mission::Attack) return true;

	// Queued Move to a non-dock destination must complete first
	if (pAC->QueuedMission == Mission::Move)
	{
		auto* dest = pAC->Destination;
		if (!dest || (dest != pAC->DockNowHeadingTo && dest != pAC->SpawnOwner))
			return true;
	}
	return false;
}


// robust strafing-steps handler (bounds+null checks + cache)
DEFINE_HOOK(0x417FF1, AircraftClass_Mission_Attack_StrafeShots, 0x6)
{
	GET(AircraftClass*, pThis, ESI);
	if (!pThis) return 0;

	auto const pExt = TechnoExt::ExtMap.Find(pThis);
	auto const pType = pThis->Type;

	int weaponIndex = pExt->CurrentAircraftWeaponIndex;
	if (weaponIndex < 0)
	{
		weaponIndex = pThis->SelectWeapon(pThis->Target);
		pExt->CurrentAircraftWeaponIndex = weaponIndex;
	}
	if (weaponIndex < 0 || !pType || weaponIndex >= pType->WeaponCount)
	{
		pExt->Strafe_BombsDroppedThisRound = 0;
		return 0;
	}

	auto const pWTSlot = pThis->GetWeapon(weaponIndex);
	if (!pWTSlot || !pWTSlot->WeaponType)
	{
		pExt->Strafe_BombsDroppedThisRound = 0;
		return 0;
	}

	auto const pWTExt = WeaponTypeExt::ExtMap.Find(pWTSlot->WeaponType);
	auto const state = static_cast<AirAttackStatus>(pThis->MissionStatus);

	// reset when not in strafing phases
	if (state < AirAttackStatus::FireAtTarget2_Strafe || state > AirAttackStatus::FireAtTarget5_Strafe)
	{
		pExt->Strafe_BombsDroppedThisRound = 0;
		return 0;
	}

	const int strafingShots = pWTExt->Strafing_Shots.Get(5);
	if (strafingShots > 5 && state == AirAttackStatus::FireAtTarget3_Strafe)
	{
		const int remaining = strafingShots - 3 - pExt->Strafe_BombsDroppedThisRound;
		if (remaining > 0)
			pThis->MissionStatus = (int)AirAttackStatus::FireAtTarget2_Strafe;
	}
	return 0;
}

// Fire location for strafing vs air targets (safe early-outs)
DEFINE_HOOK(0x4197F3, AircraftClass_GetFireLocation_Strafing, 0x5)
{
	GET(AircraftClass*, pThis, EDI);
	GET(AbstractClass*, pTarget, EAX);

	auto* const pObj = abstract_cast<ObjectClass*>(pTarget);
	if (!pObj || !pObj->IsInAir())
	{
		return 0;
	}

	int weaponIndex = TechnoExt::ExtMap.Find(pThis)->CurrentAircraftWeaponIndex;
	if (weaponIndex < 0)
	{
		weaponIndex = pThis->SelectWeapon(pTarget);
	}
	if (weaponIndex < 0)
	{
		return 0;
	}

	const auto err = pThis->GetFireError(pTarget, weaponIndex, false);
	if (err == FireError::ILLEGAL || err == FireError::CANT)
	{
		return 0;
	}

	// SAFETY: coords may be off-map in rare frames; tolerate nullptr
	auto* const cell = MapClass::Instance.TryGetCellAt(pObj->GetCoords());
	R->EAX(cell); // caller must accept null here (vanilla does)
	return 0;
}

// Use cache when possible; write-back when selecting (perf-safe)
long __stdcall AircraftClass_IFlyControl_IsStrafe(IFlyControl const* ifly)
{
	__assume(ifly != nullptr);

	auto const pThis = static_cast<AircraftClass const*>(ifly);
	auto const pExt = TechnoExt::ExtMap.Find(pThis);

	WeaponTypeClass* pWT = nullptr;

	if (pExt->CurrentAircraftWeaponIndex >= 0)
	{
		pWT = pThis->GetWeapon(pExt->CurrentAircraftWeaponIndex)->WeaponType;
	}
	else if (pThis->Target)
	{
		int idx = pThis->SelectWeapon(pThis->Target);
		if (idx >= 0)
			TechnoExt::ExtMap.Find(const_cast<AircraftClass*>(pThis))->CurrentAircraftWeaponIndex = idx; // write-back
		pWT = (idx >= 0) ? pThis->GetWeapon(idx)->WeaponType : nullptr;
	}
	else if (pExt->LastWeaponType)
	{
		pWT = pExt->LastWeaponType;
	}
	else
	{
		pWT = pThis->GetWeapon(0)->WeaponType;
	}

	if (pWT)
	{
		auto const pWTExt = WeaponTypeExt::ExtMap.Find(pWT);
		auto const pBT = pWT->Projectile;
		return pWTExt->Strafing.Get(pBT->ROT <= 1 && !pBT->Inviso && !BulletTypeExt::ExtMap.Find(pBT)->TrajectoryType);
	}
	return false;
}
DEFINE_FUNCTION_JUMP(VTABLE, 0x7E2268, AircraftClass_IFlyControl_IsStrafe);

DEFINE_HOOK(0x418403, AircraftClass_Mission_Attack_FireAtTarget_BurstFix, 0x8)
{
	GET(AircraftClass*, pThis, ESI);
	pThis->ShouldLoseAmmo = true;
	AircraftExt::FireWeapon(pThis, pThis->Target);
	return 0x418478;
}
DEFINE_HOOK(0x4186B6, AircraftClass_Mission_Attack_FireAtTarget2_BurstFix, 0x8)
{
	GET(AircraftClass*, pThis, ESI);
	AircraftExt::FireWeapon(pThis, pThis->Target);
	return 0x4186D7;
}
DEFINE_HOOK(0x418805, AircraftClass_Mission_Attack_FireAtTarget2Strafe_BurstFix, 0x8)
{
	GET(AircraftClass*, pThis, ESI);
	AircraftExt::FireWeapon(pThis, pThis->Target);
	return 0x418826;
}
DEFINE_HOOK(0x418914, AircraftClass_Mission_Attack_FireAtTarget3Strafe_BurstFix, 0x8)
{
	GET(AircraftClass*, pThis, ESI);
	AircraftExt::FireWeapon(pThis, pThis->Target);
	return 0x418935;
}
DEFINE_HOOK(0x418A23, AircraftClass_Mission_Attack_FireAtTarget4Strafe_BurstFix, 0x8)
{
	GET(AircraftClass*, pThis, ESI);
	AircraftExt::FireWeapon(pThis, pThis->Target);
	return 0x418A44;
}
DEFINE_HOOK(0x418B1F, AircraftClass_Mission_Attack_FireAtTarget5Strafe_BurstFix, 0x8)
{
	GET(AircraftClass*, pThis, ESI);
	AircraftExt::FireWeapon(pThis, pThis->Target);
	return 0x418B40;
}

int __fastcall AircraftClass_SelectWeapon_Wrapper(AircraftClass* pThis, void* _, AbstractClass* pTarget)
{
	auto const pExt = TechnoExt::ExtMap.Find(pThis);

	// Re-evaluate weapon selection only if not mid-strafing run before firing.
	if (!pExt->Strafe_BombsDroppedThisRound)
		pExt->CurrentAircraftWeaponIndex = Math::max(pThis->SelectWeapon(pTarget), 0);

	return pExt->CurrentAircraftWeaponIndex;
}

DEFINE_FUNCTION_JUMP(CALL6, 0x41831E, AircraftClass_SelectWeapon_Wrapper);
DEFINE_FUNCTION_JUMP(CALL6, 0x4185F5, AircraftClass_SelectWeapon_Wrapper);
DEFINE_FUNCTION_JUMP(CALL6, 0x4187C4, AircraftClass_SelectWeapon_Wrapper);
DEFINE_FUNCTION_JUMP(CALL6, 0x4188D3, AircraftClass_SelectWeapon_Wrapper);
DEFINE_FUNCTION_JUMP(CALL6, 0x4189E2, AircraftClass_SelectWeapon_Wrapper);
DEFINE_FUNCTION_JUMP(CALL6, 0x418AF1, AircraftClass_SelectWeapon_Wrapper);

DEFINE_HOOK_AGAIN(0x41874E, AircraftClass_Mission_Attack_StrafingDestinationFix, 0x6)
DEFINE_HOOK(0x418544, AircraftClass_Mission_Attack_StrafingDestinationFix, 0x6)
{
	GET(FireError, fireError, EAX);
	GET(AircraftClass*, pThis, ESI);

	// The aircraft managed by the spawn manager will not update destination after changing target
	if (fireError == FireError::RANGE && pThis->Is_Strafe())
		pThis->SetDestination(pThis->Target, true);

	return 0;
}

#pragma region After_Shot_Delays

static int GetDelay(AircraftClass* pThis, bool isLastShot)
{
	auto const pExt = TechnoExt::ExtMap.Find(pThis);
	auto const pWeapon = pThis->GetWeapon(pExt->CurrentAircraftWeaponIndex)->WeaponType;
	auto const pWeaponExt = WeaponTypeExt::ExtMap.Find(pWeapon);
	int delay = pWeapon->ROF;

	if (isLastShot || pExt->Strafe_BombsDroppedThisRound == pWeaponExt->Strafing_Shots.Get(5) || (pWeaponExt->Strafing_UseAmmoPerShot && !pThis->Ammo))
	{
		pExt->Strafe_TargetCell = nullptr;
		pThis->MissionStatus = (int)AirAttackStatus::FlyToPosition;
		delay = pWeaponExt->Strafing_EndDelay.Get((pWeapon->Range + (Unsorted::LeptonsPerCell * 4)) / pThis->Type->Speed);
	}

	return delay;
}

DEFINE_HOOK(0x4184CC, AircraftClass_Mission_Attack_Delay1A, 0x6)
{
	GET(AircraftClass*, pThis, ESI);

	auto const pExt = TechnoExt::ExtMap.Find(pThis);

	if (WeaponTypeExt::ExtMap.Find(pThis->GetWeapon(pExt->CurrentAircraftWeaponIndex)->WeaponType)->Strafing_TargetCell)
		pExt->Strafe_TargetCell = MapClass::Instance.GetCellAt(pThis->Target->GetCoords());
	pThis->IsLocked = true;
	pThis->MissionStatus = (int)AirAttackStatus::FireAtTarget2_Strafe;
	R->EAX(GetDelay(pThis, false));
	return 0x4184F1;
}
DEFINE_HOOK(0x418506, AircraftClass_Mission_Attack_Delay1B, 0x6)
{
	GET(AircraftClass*, pThis, ESI);
	pThis->IsLocked = true;
	pThis->MissionStatus = pThis->Ammo > 0 ? (int)AirAttackStatus::PickAttackLocation : (int)AirAttackStatus::ReturnToBase;
	R->EAX(GetDelay(pThis, false));
	return 0x418539;
}
DEFINE_HOOK(0x418883, AircraftClass_Mission_Attack_Delay2, 0x6)
{
	GET(AircraftClass*, pThis, ESI);
	pThis->MissionStatus = (int)AirAttackStatus::FireAtTarget3_Strafe;
	R->EAX(GetDelay(pThis, false));
	return 0x4188A1;
}
DEFINE_HOOK(0x418992, AircraftClass_Mission_Attack_Delay3, 0x6)
{
	GET(AircraftClass*, pThis, ESI);
	pThis->MissionStatus = (int)AirAttackStatus::FireAtTarget4_Strafe;
	R->EAX(GetDelay(pThis, false));
	return 0x4189B0;
}
DEFINE_HOOK(0x418AA1, AircraftClass_Mission_Attack_Delay4, 0x6)
{
	GET(AircraftClass*, pThis, ESI);
	pThis->MissionStatus = (int)AirAttackStatus::FireAtTarget5_Strafe;
	R->EAX(GetDelay(pThis, false));
	return 0x418ABF;
}
DEFINE_HOOK(0x418B8A, AircraftClass_Mission_Attack_Delay5, 0x6)
{
	GET(AircraftClass*, pThis, ESI);
	R->EAX(GetDelay(pThis, true));
	return 0x418BBA;
}

#pragma endregion

// Fix for OpenTopped aircraft circling issue when landing with passengers
DEFINE_HOOK(0x417A2E, AircraftClass_EnterIdleMode_Opentopped, 0x5)
{
	GET(AircraftClass*, pThis, ESI);
	R->EDI(2);
	const bool willJump = !pThis->Spawned && pThis->Type->OpenTopped &&
		(pThis->QueuedMission != Mission::Attack) && !pThis->Target;
	DBG("EnterIdle(OpenTopped): jump=%d Ammo=%d Cur=%s Queued=%s",
		willJump ? 1 : 0, pThis->Ammo, MissionStr(pThis->CurrentMission), MissionStr(pThis->QueuedMission));
	return willJump ? 0x417944 : 0x417AD4;
}

// Critical for OpenTopped aircraft weapon index management
void __fastcall AircraftClass_SetTarget_Wrapper(AircraftClass* pThis, void* _, AbstractClass* pTarget)
{
	pThis->TechnoClass::SetTarget(pTarget);
	auto* const pExt = TechnoExt::ExtMap.Find(pThis);
	pExt->CurrentAircraftWeaponIndex = -1;

	// when target clears, unlock strafing and reset transient strafing state
	if (!pTarget)
	{
		pThis->IsLocked = false;
		pExt->Strafe_BombsDroppedThisRound = 0;
		pExt->Strafe_TargetCell = nullptr;
	}
}
DEFINE_FUNCTION_JUMP(VTABLE, 0x7E266C, AircraftClass_SetTarget_Wrapper);

#pragma region StrafeCell

DEFINE_HOOK_AGAIN(0x4188AC, AircraftClass_Mission_Attack_StrafeCell, 0x6)
DEFINE_HOOK_AGAIN(0x4189BB, AircraftClass_Mission_Attack_StrafeCell, 0x6)
DEFINE_HOOK_AGAIN(0x418ACA, AircraftClass_Mission_Attack_StrafeCell, 0x6)
DEFINE_HOOK(0x41879D, AircraftClass_Mission_Attack_StrafeCell, 0x6)
{
	enum { CannotFireNow = 0x418BC5, SkipGameCode = 0x418BBA };

	GET(AircraftClass*, pThis, ESI);

	const auto pExt = TechnoExt::ExtMap.Find(pThis);

	if (const auto pTargetCell = pExt->Strafe_TargetCell)
	{
		switch (pThis->GetFireError(pTargetCell, pExt->CurrentAircraftWeaponIndex, true))
		{
		case FireError::OK:
		case FireError::FACING:
		case FireError::CLOAKED:
		case FireError::RANGE:
			break;
		default:
			return CannotFireNow;
		}

		AircraftExt::FireWeapon(pThis, pTargetCell);

		if (pExt->TypeExtData->FiringForceScatter)
			pTargetCell->ScatterContent(pThis->Location, true, false, false);

		pThis->SetDestination(pTargetCell, true);
		pThis->MissionStatus++;

		R->EAX(GetDelay(pThis, pThis->MissionStatus > static_cast<int>(AirAttackStatus::FireAtTarget5_Strafe)));
		return SkipGameCode;
	}

	return 0;
}

#pragma endregion

#pragma region ScatterCell

DEFINE_HOOK_AGAIN(0x41882C, AircraftClass_MissionAttack_ScatterCell1, 0x6)
DEFINE_HOOK_AGAIN(0x41893B, AircraftClass_MissionAttack_ScatterCell1, 0x6)
DEFINE_HOOK_AGAIN(0x418A4A, AircraftClass_MissionAttack_ScatterCell1, 0x6)
DEFINE_HOOK_AGAIN(0x418B46, AircraftClass_MissionAttack_ScatterCell1, 0x6)
DEFINE_HOOK(0x41847E, AircraftClass_MissionAttack_ScatterCell1, 0x6)
{
	GET(AircraftClass*, pThis, ESI);
	return TechnoTypeExt::ExtMap.Find(pThis->Type)->FiringForceScatter ? 0 : (R->Origin() + 0x44);
}
DEFINE_HOOK(0x4186DD, AircraftClass_MissionAttack_ScatterCell2, 0x5)
{
	GET(AircraftClass*, pThis, ESI);
	return TechnoTypeExt::ExtMap.Find(pThis->Type)->FiringForceScatter ? 0 : (R->Origin() + 0x43);
}

#pragma endregion

DEFINE_HOOK(0x414F10, AircraftClass_AI_Trailer, 0x5)
{
	enum { SkipGameCode = 0x414F47 };
	GET(AircraftClass*, pThis, ESI);
	REF_STACK(const CoordStruct, coords, STACK_OFFSET(0x40, -0xC));

	auto const pTrailerAnim = GameCreate<AnimClass>(pThis->Type->Trailer, coords, 1, 1);
	auto const pTrailerAnimExt = AnimExt::ExtMap.Find(pTrailerAnim);
	AnimExt::SetAnimOwnerHouseKind(pTrailerAnim, pThis->Owner, nullptr, false, true);
	pTrailerAnimExt->SetInvoker(pThis);
	pTrailerAnimExt->IsTechnoTrailerAnim = true;
	return SkipGameCode;
}

DEFINE_HOOK(0x414C0B, AircraftClass_ChronoSparkleDelay, 0x5)
{
	R->ECX(RulesExt::Global()->ChronoSparkleDisplayDelay);
	return 0x414C10;
}

#pragma region LandingDir

DEFINE_HOOK(0x4CF31C, FlyLocomotionClass_FlightUpdate_LandingDir, 0x9)
{
	enum { SkipGameCode = 0x4CF3D0, SetSecondaryFacing = 0x4CF351 };

	GET(FootClass** const, pFootPtr, ESI);
	GET_STACK(IFlyControl* const, iFly, STACK_OFFSET(0x48, -0x38));
	REF_STACK(unsigned int, dir, STACK_OFFSET(0x48, 0x8));

	const auto pFoot = *pFootPtr;
	dir = 0;

	if (!iFly) return SetSecondaryFacing;
	if (iFly->Is_Locked()) return SkipGameCode;

	if (const auto pAircraft = abstract_cast<AircraftClass*, true>(pFoot))
		dir = DirStruct(AircraftExt::GetLandingDir(pAircraft)).Raw;
	else
		dir = (iFly->Landing_Direction() << 13);

	return SetSecondaryFacing;
}

namespace SeparateAircraftTemp { BuildingClass* pBuilding = nullptr; }

DEFINE_HOOK(0x446F57, BuildingClass_GrandOpening_PoseDir_SetContext, 0x6)
{
	GET(BuildingClass*, pThis, EBP);
	SeparateAircraftTemp::pBuilding = pThis;
	return 0;
}

DirType __fastcall AircraftClass_PoseDir_Wrapper(AircraftClass* pThis)
{
	return AircraftExt::GetLandingDir(pThis, SeparateAircraftTemp::pBuilding);
}
DEFINE_FUNCTION_JUMP(CALL, 0x446F67, AircraftClass_PoseDir_Wrapper);

DEFINE_HOOK(0x443FC7, BuildingClass_ExitObject_PoseDir1, 0x8)
{
	GET(BuildingClass*, pThis, ESI);
	GET(AircraftClass*, pAircraft, EBP);
	R->EAX(AircraftExt::GetLandingDir(pAircraft, pThis));
	return 0;
}

DEFINE_HOOK(0x44402E, BuildingClass_ExitObject_PoseDir2, 0x5)
{
	GET(BuildingClass*, pThis, ESI);
	GET(AircraftClass*, pAircraft, EBP);

	auto const dir = DirStruct(AircraftExt::GetLandingDir(pAircraft, pThis));
	if (RulesExt::Global()->ExtendedAircraftMissions)
		pAircraft->PrimaryFacing.SetCurrent(dir);
	pAircraft->SecondaryFacing.SetCurrent(dir);
	return 0;
}

#pragma endregion

DEFINE_HOOK(0x415EEE, AircraftClass_Fire_KickOutPassengers, 0x6)
{
	enum { SkipKickOutPassengers = 0x415F08 };
	GET(AircraftClass*, pThis, EDI);
	GET_BASE(const int, weaponIdx, 0xC);

	auto const slot = pThis->GetWeapon(weaponIdx);
	if (!slot) return 0;

	auto const pWeapon = slot->WeaponType;
	if (!pWeapon) return 0;

	if (!WeaponTypeExt::ExtMap.Find(pWeapon)->KickOutPassengers)
		return SkipKickOutPassengers;

	return 0;
}

#pragma region ExtendedAircraftMissions

bool __fastcall AircraftTypeClass_CanUseWaypoint(AircraftTypeClass* /*pThis*/)
{
	return RulesExt::Global()->ExtendedAircraftMissions.Get();
}
DEFINE_FUNCTION_JUMP(VTABLE, 0x7E2908, AircraftTypeClass_CanUseWaypoint)

// CHANGED: Commit to landing only when heading to dock AND not blocked by orders.
DEFINE_HOOK_AGAIN(0x4168C7, AircraftClass_Mission_Move_SmoothMoving, 0x5)
DEFINE_HOOK(0x416A0A, AircraftClass_Mission_Move_SmoothMoving, 0x5)
{
	enum { EnterIdleAndReturn = 0x416AC0, ContinueMoving1 = 0x416908, ContinueMoving2 = 0x416A47 };

	GET(AircraftClass* const, pThis, ESI);
	GET(CoordStruct const* const, pCoords, EAX);

	if (pThis->Team || pThis->Airstrike || pThis->Spawned) return 0;
	const auto pType = pThis->Type;
	if (!pType || !pType->AirportBound) return 0;

	const bool hasNoTarget = (pThis->Target == nullptr);
	const bool hasLandingIntent =
		pThis->Destination && (pThis->DockNowHeadingTo == pThis->Destination || pThis->SpawnOwner == pThis->Destination);

	// ONLY commit once we're genuinely heading to dock and not fighting/following a target
	if (hasNoTarget && hasLandingIntent)
	{
		pThis->IsLocked = false;              // don’t keep strafing lock
		if (!pThis->TryNextPlanningTokenNode())
			pThis->EnterIdleMode(false, true);

		DBG("SmoothMoving: commit to landing (Ammo=%d, Cur=%s, Queued=%s)",
			pThis->Ammo, MissionStr(pThis->CurrentMission), MissionStr(pThis->QueuedMission));
		return EnterIdleAndReturn;
	}

	const bool extended = RulesExt::Global()->ExtendedAircraftMissions;
	if (!TechnoTypeExt::ExtMap.Find(pType)->ExtendedAircraftMissions_SmoothMoving.Get(extended))
		return 0;

	const int distance = static_cast<int>(Point2D { pCoords->X - pThis->Location.X, pCoords->Y - pThis->Location.Y }.Magnitude());
	const double rotRadian = std::abs(pThis->PrimaryFacing.ROT.Raw * (Math::TwoPi / 65536));
	const int turningRadius = (rotRadian > 1e-10) ? static_cast<int>(pType->Speed / rotRadian) : 0;

	if (distance > std::max(pType->SlowdownDistance / 2, turningRadius))
		return (R->Origin() == 0x4168C7 ? ContinueMoving1 : ContinueMoving2);

	if (!extended || !pThis->TryNextPlanningTokenNode())
		pThis->EnterIdleMode(false, true);

	return EnterIdleAndReturn;
}

DEFINE_HOOK(0x4DDD66, FootClass_IsLandZoneClear_ReplaceHardcode, 0x6)
{
	enum { SkipGameCode = 0x4DDD8A };
	GET(FootClass* const, pThis, EBP);
	GET_STACK(CellStruct, cell, STACK_OFFSET(0x20, 0x4));

	const auto pType = pThis->GetTechnoType();
	// **Unconditional true** like your last stable version
	R->AL(MapClass::Instance.GetCellAt(cell)->IsClearToMove(
		pType->SpeedType, false, false, -1, pType->MovementZone, -1, true));
	return SkipGameCode;
}

// Skip duplicated aircraft check
DEFINE_PATCH(0x4CF033, 0x8B, 0x06, 0xEB, 0x18); // mov eax, [esi] ; jmp short loc_4CF04F ;

// CHANGED: Don’t override steering once landing has begun / pad latched
DEFINE_HOOK(0x4CF190, FlyLocomotionClass_FlightUpdate_SetPrimaryFacing, 0x6)
{
	enum { SkipGameCode = 0x4CF29A };
	GET(IFlyControl* const, iFly, EAX);

	if (!iFly || !iFly->Is_Locked())
	{
		GET(FootClass** const, pFootPtr, ESI);
		REF_STACK(CoordStruct, destination, STACK_OFFSET(0x48, 0x8));
		// same offset you already use in SetFlightLevel:
		GET_STACK(FlyLocomotionClass* const, pFly, STACK_OFFSET(0x48, -0x28));

		auto* const pFoot = *pFootPtr;
		auto* const pAircraft = abstract_cast<AircraftClass*, true>(pFoot);

		// If landing spline active OR pad latched / heading to dock -> let vanilla steer
		const bool headingToDock = pAircraft && (pAircraft->DockNowHeadingTo ||
								(pAircraft->Destination && (pAircraft->Destination == pAircraft->DockNowHeadingTo ||
									pAircraft->Destination == pAircraft->SpawnOwner)));
		if ((pFly && pFly->IsLanding) || headingToDock)
		{
			DBG("SetPrimaryFacing: yield (IsLanding=%d, DockLatched=%d)",
				pFly ? (int)pFly->IsLanding : 0, pAircraft && pAircraft->DockNowHeadingTo ? 1 : 0);
			return 0; // run original code
		}

		auto horizontalDistance = [&destination](const CoordStruct& loc)
			{
				const auto d = Point2D { loc.X, loc.Y } - Point2D { destination.X, destination.Y };
				return static_cast<int>(d.Magnitude());
			};

		if (!pAircraft || !TechnoTypeExt::ExtMap.Find(pAircraft->Type)->ExtendedAircraftMissions_RearApproach
							.Get(RulesExt::Global()->ExtendedAircraftMissions))
		{
			const auto foot = pFoot->GetCoords();
			const auto desired = DirStruct(Math::atan2(foot.Y - destination.Y, destination.X - foot.X));
			if (!iFly || !iFly->Is_Strafe() || horizontalDistance(foot) > 768
				|| std::abs((short)(desired.Raw) - (short)(pFoot->PrimaryFacing.Current().Raw)) <= 8192)
			{
				pFoot->PrimaryFacing.SetDesired(desired);
			}
		}
		else
		{
			const auto footCoords = pAircraft->GetCoords();
			const DirStruct landingDir = DirStruct(AircraftExt::GetLandingDir(pAircraft));

			if (pAircraft->Destination && (pAircraft->DockNowHeadingTo == pAircraft->Destination || pAircraft->SpawnOwner == pAircraft->Destination))
			{
				const auto pType = pAircraft->Type;
				const auto rotRadian = std::abs(pAircraft->PrimaryFacing.ROT.Raw * (Math::TwoPi / 65536));
				const auto turningRadius = rotRadian > 1e-10 ? static_cast<int>(pType->Speed / rotRadian) : 0;
				const auto cellCounts = Math::max((pType->SlowdownDistance / Unsorted::LeptonsPerCell), (turningRadius / 128));

				const auto currentDir = DirStruct(Math::atan2(footCoords.Y - destination.Y, destination.X - footCoords.X));
				const auto difference = static_cast<short>(static_cast<short>(currentDir.Raw) - static_cast<short>(landingDir.Raw));

				const auto landingFace = landingDir.GetFacing<8>(4);
				auto cellOffset = Unsorted::AdjacentCoord[landingFace];

				if (std::abs(difference) >= 12288) // 3/16 of full circle
					cellOffset = (cellOffset + Unsorted::AdjacentCoord[((difference > 0) ? (landingFace + 2) : (landingFace - 2)) & 7]) * cellCounts;
				else
					cellOffset *= Math::min(cellCounts, ((landingFace & 1) ? (horizontalDistance(footCoords) / 724) : (horizontalDistance(footCoords) / 512)));

				destination.X += cellOffset.X;
				destination.Y += cellOffset.Y;
			}

			const auto foot = pAircraft->GetCoords();

			const auto foot2 = pAircraft->GetCoords();
			if (foot2.Y != destination.Y || foot2.X != destination.X)
				pAircraft->PrimaryFacing.SetDesired(DirStruct(Math::atan2(foot2.Y - destination.Y, destination.X - foot2.X)));
			else
				pAircraft->PrimaryFacing.SetDesired(landingDir);
		}
	}
	return SkipGameCode;
}

// CHANGED: Don’t override altitude once landing has begun / pad latched
DEFINE_HOOK(0x4CF3D0, FlyLocomotionClass_FlightUpdate_SetFlightLevel, 0x7)
{
	GET(FootClass** const, pFootPtr, ESI);
	const auto pAircraft = abstract_cast<AircraftClass*, true>(*pFootPtr);
	if (!pAircraft) { return 0; }

	// NEW: if pad is latched, let vanilla manage vertical profile
	if (pAircraft->DockNowHeadingTo)
	{
		return 0;
	}

	const auto pType = pAircraft->Type;
	if (pType->HunterSeeker) { return 0; }

	if (!TechnoTypeExt::ExtMap.Find(pType)->ExtendedAircraftMissions_EarlyDescend
			.Get(RulesExt::Global()->ExtendedAircraftMissions))
	{
		return 0;
	}

	enum { SkipGameCode = 0x4CF4D2 };

	GET_STACK(FlyLocomotionClass* const, pThis, STACK_OFFSET(0x48, -0x28));
	GET(const int, distance, EBX);

	// restore EBP as the original code expects
	R->EBP(pThis);

	// Early-elevate stage: use destination floor height – only if destination cell is valid
	if (pThis->IsElevating && distance < 768)
	{
		if (MapClass::Instance.TryGetCellAt(pThis->MovingDestination))
		{
			const auto floorHeight = MapClass::Instance.GetCellFloorHeight(pThis->MovingDestination);
			pThis->FlightLevel = pThis->MovingDestination.Z - floorHeight;

			// Bridge correction – only if current location cell exists
			if (const auto* locCell = MapClass::Instance.TryGetCellAt(pAircraft->Location))
			{
				if (locCell->ContainsBridge() && pThis->FlightLevel >= CellClass::BridgeHeight)
				{
					pThis->FlightLevel -= CellClass::BridgeHeight;
				}
			}
			return SkipGameCode;
		}
		// If dest is off-map this tick, fall through to default below
	}

	const auto flightLevel = pType->GetFlightLevel();

	// Smooth descent toward docks/spawn owner – only if destination cell is valid
	if (distance < pType->SlowdownDistance && pAircraft->Destination
		&& (pAircraft->DockNowHeadingTo == pAircraft->Destination
			|| pAircraft->SpawnOwner == pAircraft->Destination))
	{
		if (MapClass::Instance.TryGetCellAt(pThis->MovingDestination))
		{
			const auto floorHeight = MapClass::Instance.GetCellFloorHeight(pThis->MovingDestination);
			const auto destHeight = pThis->MovingDestination.Z - floorHeight + 1;

			// vanilla interpolation retained
			pThis->FlightLevel = static_cast<int>(
				(flightLevel - destHeight) * (static_cast<double>(distance) / pType->SlowdownDistance)
			) + destHeight;

			return SkipGameCode;
		}
		// If dest is invalid, fall back to default level below
	}

	// Default behaviour
	pThis->FlightLevel = flightLevel;
	return SkipGameCode;
}

DEFINE_HOOK_AGAIN(0x41A982, AircraftClass_Mission_AreaGuard, 0x6)
DEFINE_HOOK(0x41A96C, AircraftClass_Mission_AreaGuard, 0x6)
{
	enum { SkipGameCode = 0x41A97A };
	GET(AircraftClass* const, pThis, ESI);

	if (RulesExt::Global()->ExtendedAircraftMissions && !pThis->Team && pThis->Ammo && pThis->IsArmed())
	{
		auto coords = pThis->GetCoords();
		if (pThis->TargetAndEstimateDamage(coords, ThreatType::Area))
			pThis->QueueMission(Mission::Attack, false);
		else if (pThis->Destination && pThis->Destination != pThis->DockNowHeadingTo)
			pThis->EnterIdleMode(false, true);
		return SkipGameCode;
	}
	return 0;
}

int __fastcall AircraftClass_Mission_Sleep(AircraftClass* pThis)
{
	if (!pThis->Destination || pThis->Destination == pThis->DockNowHeadingTo)
		return 450;
	pThis->EnterIdleMode(false, true);
	return 1;
}
DEFINE_FUNCTION_JUMP(VTABLE, 0x7E24A8, AircraftClass_Mission_Sleep)

bool __fastcall AircraftTypeClass_CanAttackMove(AircraftTypeClass* /*pThis*/)
{
	return RulesExt::Global()->ExtendedAircraftMissions.Get();
}
DEFINE_FUNCTION_JUMP(VTABLE, 0x7E290C, AircraftTypeClass_CanAttackMove)

// CHANGED: Only pause attack-move on no-ammo/not-airborne (don’t pause just for landing intent)
DEFINE_HOOK(0x6FA68B, TechnoClass_Update_AttackMove_AttackMovePaused, 0xA)
{
	enum { SkipGameCode = 0x6FA6F5 };
	GET(TechnoClass* const, pThis, ESI);

	bool skip = RulesExt::Global()->ExtendedAircraftMissions
		&& pThis->WhatAmI() == AbstractType::Aircraft
		&& (!pThis->Ammo || !pThis->IsInAir());

	return skip ? SkipGameCode : 0;
}

DEFINE_HOOK(0x4DF3BA, FootClass_UpdateAttackMove_AircraftHoldAttackMoveTarget1, 0x6)
{
	enum { LoseTarget = 0x4DF3D3, HoldTarget = 0x4DF4AB };
	GET(FootClass* const, pThis, ESI);
	return (RulesExt::Global()->ExtendedAircraftMissions && pThis->WhatAmI() == AbstractType::Aircraft)
		? HoldTarget
		: (pThis->InAuxiliarySearchRange(pThis->Target) ? HoldTarget : LoseTarget);
}

DEFINE_HOOK(0x4DF42A, FootClass_UpdateAttackMove_AircraftHoldAttackMoveTarget2, 0x6)
{
	enum { ContinueCheck = 0x4DF462, HoldTarget = 0x4DF4AB };
	GET(FootClass* const, pThis, ESI);
	return (RulesExt::Global()->ExtendedAircraftMissions && pThis->WhatAmI() == AbstractType::Aircraft) ? HoldTarget : ContinueCheck;
}

DEFINE_HOOK(0x418CD1, AircraftClass_Mission_Attack_ContinueFlyToDestination, 0x6)
{
	enum { Continue = 0x418C43, Return = 0x418CE8 };
	GET(AircraftClass* const, pThis, ESI);

	if (!pThis->Target)
	{
		if (!RulesExt::Global()->ExtendedAircraftMissions || !pThis->MegaMissionIsAttackMove() || !pThis->MegaDestination)
			return Continue;

		pThis->SetDestination(pThis->MegaDestination, false);
		pThis->QueueMission(Mission::Move, true);
		pThis->HaveAttackMoveTarget = false;
	}
	else
	{
		pThis->MissionStatus = 1;
	}

	R->EAX(1);
	return Return;
}

AbstractClass* __fastcall AircraftClass_GreatestThreat(AircraftClass* pThis, void* _, ThreatType threatType, CoordStruct* pSelectCoords, bool onlyTargetHouseEnemy)
{
	if (RulesExt::Global()->ExtendedAircraftMissions && !pThis->Team && pThis->Ammo && !pThis->Airstrike && !pThis->Spawned)
	{
		if (const auto pPrimaryWeapon = pThis->GetWeapon(0)->WeaponType)
			threatType |= pPrimaryWeapon->AllowedThreats();

		if (const auto pSecondaryWeapon = pThis->GetWeapon(1)->WeaponType)
			threatType |= pSecondaryWeapon->AllowedThreats();
	}

	return pThis->FootClass::GreatestThreat(threatType, pSelectCoords, onlyTargetHouseEnemy);
}
DEFINE_FUNCTION_JUMP(VTABLE, 0x7E2668, AircraftClass_GreatestThreat)

// Handle assigning area guard mission to aircraft.
DEFINE_HOOK(0x4C7403, EventClass_Execute_AircraftAreaGuard, 0x6)
{
	enum { SkipGameCode = 0x4C7435 };
	GET(TechnoClass* const, pTechno, EDI);

	if (RulesExt::Global()->ExtendedAircraftMissions && pTechno->WhatAmI() == AbstractType::Aircraft)
		return SkipGameCode;

	return 0;
}

// Do not untether aircraft by default for area guard, unless really needed.
DEFINE_HOOK(0x4C72F2, EventClass_Execute_AircraftAreaGuard_Untether, 0x6)
{
	enum { SkipGameCode = 0x4C7349 };
	GET(EventClass* const, pThis, ESI);
	GET(TechnoClass* const, pTechno, EDI);

	if (RulesExt::Global()->ExtendedAircraftMissions
		&& pTechno->WhatAmI() == AbstractType::Aircraft
		&& pThis->MegaMission.Mission == (char)Mission::Area_Guard
		&& (pTechno->CurrentMission != Mission::Sleep || !pTechno->Ammo))
	{
		return SkipGameCode;
	}
	return 0;
}

DEFINE_HOOK(0x418CF3, AircraftClass_Mission_Attack_PlanningFix, 0x5)
{
	enum { SkipIdle = 0x418D00 };
	GET(AircraftClass*, pThis, ESI);
	return pThis->Ammo <= 0 || !pThis->TryNextPlanningTokenNode() ? 0 : SkipIdle;
}

#pragma endregion

// Spy Plane camera shot limiter (deterministic)
static __forceinline bool CheckSpyPlaneCameraCount(AircraftClass* pThis)
{
	auto const pWT0 = pThis->GetWeapon(0);
	if (!pWT0 || !pWT0->WeaponType) return true;
	auto const pWTExt = WeaponTypeExt::ExtMap.Find(pWT0->WeaponType);

	if (!pWTExt->Strafing_Shots.isset())
		return true;

	auto const pExt = TechnoExt::ExtMap.Find(pThis);
	if (pExt->Strafe_BombsDroppedThisRound >= pWTExt->Strafing_Shots)
		return false;

	pExt->Strafe_BombsDroppedThisRound++;
	return true;
}

DEFINE_HOOK(0x415666, AircraftClass_Mission_SpyPlaneApproach_MaxCount, 0x6)
{
	enum { Skip = 0x41570C };
	GET(AircraftClass*, pThis, ESI);
	return CheckSpyPlaneCameraCount(pThis) ? 0 : Skip;
}
DEFINE_HOOK(0x4157EB, AircraftClass_Mission_SpyPlaneOverfly_MaxCount, 0x6)
{
	enum { Skip = 0x415863 };
	GET(AircraftClass*, pThis, ESI);
	return CheckSpyPlaneCameraCount(pThis) ? 0 : Skip;
}

// CHANGED: Player orders immediately cancel RTB/docking and unlock strafing state
DEFINE_HOOK(0x708FC0, TechnoClass_ResponseMove_AircraftUnlock, 0x5)
{
	GET(TechnoClass*, pThis, ECX);
	if (auto* const pAC = abstract_cast<AircraftClass*, true>(pThis))
	{
		DBG("ResponseMove: unlock (Ammo=%d Cur=%s Dest=%p Dock=%p)",
			pAC->Ammo, MissionStr(pAC->CurrentMission), pAC->Destination, pAC->DockNowHeadingTo);
		pAC->IsLocked = false;
		if (auto* const pExt = TechnoExt::ExtMap.TryFind(pAC))
		{
			pExt->Strafe_BombsDroppedThisRound = 0;
			pExt->Strafe_TargetCell = nullptr;
		}
	}
	return 0;
}

// Optional: carryall pickup voice (deterministic, single-call)
/*Already in TechnoHooks

DEFINE_HOOK(0x708FC0, TechnoClass_ResponseMove_Pickup, 0x5)
{
	enum { SkipResponse = 0x709015 };
	GET(TechnoClass*, pThis, ECX);

	if (auto const pAircraft = abstract_cast<AircraftClass*>(pThis))
	{
		auto const pType = pAircraft->Type;
		if (pType->Carryall && pAircraft->HasAnyLink() && generic_cast<FootClass*>(pAircraft->Destination))
		{
			auto const pTypeExt = TechnoTypeExt::ExtMap.Find(pType);
			if (pTypeExt->VoicePickup.isset())
			{
				pThis->QueueVoice(pTypeExt->VoicePickup.Get());
				R->EAX(1);
				return SkipResponse;
			}
		}
	}
	return 0;
}
*/
