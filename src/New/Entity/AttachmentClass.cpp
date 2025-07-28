#include "AttachmentClass.h"

#include <Dir.h>
#include <BulletClass.h>
#include <BulletTypeClass.h>
#include <WarheadTypeClass.h>

#include <ObjBase.h>

#include <Ext/Techno/Body.h>
#include <Locomotion/AttachmentLocomotionClass.h>

std::vector<AttachmentClass*> AttachmentClass::Array;

AttachmentTypeClass* AttachmentClass::GetType()
{
	// Critical null check to prevent crashes
	if (!this->Data)
		return nullptr;

	// Bounds check for array access
	if (this->Data->Type < 0 || static_cast<size_t>(this->Data->Type) >= AttachmentTypeClass::Array.size())
		return nullptr;

	return AttachmentTypeClass::Array[this->Data->Type].get();
}

TechnoTypeClass* AttachmentClass::GetChildType()
{
	// Critical null check to prevent crashes
	if (!this->Data)
		return nullptr;

	return this->Data->TechnoType.isset()
		? TechnoTypeClass::Array[this->Data->TechnoType]
		: nullptr;
}

CoordStruct AttachmentClass::GetChildLocation()
{
	// Critical null check to prevent crashes
	if (!this->Parent || !this->Data)
		return CoordStruct::Empty;

	AttachmentTypeClass* pType = this->GetType();
	if (!pType)
		return CoordStruct::Empty;

	// Use formation system if enabled, otherwise fall back to FLH
	if (pType->FormationType != AttachmentFormationType::Follow)
	{
		return this->CalculateFormationPosition();
	}

	// Default FLH-based positioning
	auto& flh = this->Data->FLH.Get();
	return TechnoExt::GetFLHAbsoluteCoords(this->Parent, flh, this->Data->IsOnTurret);
}

AttachmentClass::~AttachmentClass()
{
	// clean up non-owning references
	if (this->Child)
	{
		auto const& pChildExt = TechnoExt::ExtMap.Find(Child);
		pChildExt->ParentAttachment = nullptr;
	}

	auto position = std::find(Array.begin(), Array.end(), this);
	if (position != Array.end())
		Array.erase(position);
}

void AttachmentClass::Initialize()
{
	if (this->Child)
		return;

	// Critical null check
	AttachmentTypeClass* pType = this->GetType();
	if (!pType)
		return;

	if (pType->RespawnAtCreation)
		this->CreateChild();
}

void AttachmentClass::CreateChild()
{
	// Critical null checks
	if (!this->Parent)
		return;

	if (auto const pChildType = this->GetChildType())
	{
		if (pChildType->WhatAmI() != AbstractType::UnitType)
			return;

		if (const auto pTechno = static_cast<TechnoClass*>(pChildType->CreateObject(this->Parent->Owner)))
		{
			this->AttachChild(pTechno);
		}
		else
		{
			Debug::Log("[" __FUNCTION__ "] Failed to create child %s of parent %s!\n",
				pChildType->ID, this->Parent->GetTechnoType()->ID);
		}
	}
}

// Formation system implementation
CoordStruct AttachmentClass::CalculateFormationPosition()
{
	AttachmentTypeClass* pType = this->GetType();

	switch (pType->FormationType)
	{
	case AttachmentFormationType::Trail:
		return this->GetTrailPosition();
	case AttachmentFormationType::Escort:
		return this->GetEscortPosition();
	case AttachmentFormationType::Orbit:
		return this->GetOrbitPosition();
	case AttachmentFormationType::Line:
		return this->GetLineFormationPosition();
	case AttachmentFormationType::Wedge:
		return this->GetWedgeFormationPosition();
	case AttachmentFormationType::Diamond:
		return this->GetDiamondFormationPosition();
	case AttachmentFormationType::Custom:
	case AttachmentFormationType::Follow:
	default:
		// Fall back to FLH positioning
		auto& flh = this->Data->FLH.Get();
		return TechnoExt::GetFLHAbsoluteCoords(this->Parent, flh, this->Data->IsOnTurret);
	}
}

CoordStruct AttachmentClass::GetTrailPosition()
{
	// Critical null checks
	if (!this->Parent)
		return CoordStruct::Empty;

	AttachmentTypeClass* pType = this->GetType();
	if (!pType)
		return CoordStruct::Empty;

	// Use train-inspired direct following if enabled
	if (pType->TrailMode == TrailMode::Direct)
	{
		return this->GetDirectFollowPosition();
	}

	// Original historical path following
	// Update movement history
	this->UpdateMovementHistory();

	// Calculate trail position based on delay - FIX: Use proper indexing
	int trailDelay = Math::max(1, pType->TrailDelay.Get()); // Ensure minimum delay of 1

	// Check if we have enough history for the trail
	if (this->ParentMovementHistory.size() >= static_cast<size_t>(trailDelay))
	{
		// FIX: Access the correct historical position (trailDelay-1 because 0 is most recent)
		CoordStruct trailPos = this->ParentMovementHistory[trailDelay - 1].Position;

		// Check for collision avoidance
		if (pType->AvoidParentCollision && this->IsPositionBlocked(trailPos))
		{
			return this->FindAlternativePosition(trailPos);
		}

		return trailPos;
	}

	// Fall back to current parent position with offset if not enough history
	CoordStruct fallbackPos = this->Parent->GetCoords();
	fallbackPos.X -= (int)pType->FormationSpacing;
	return fallbackPos;
}

CoordStruct AttachmentClass::GetEscortPosition()
{
	// Critical null checks
	if (!this->Parent)
		return CoordStruct::Empty;

	AttachmentTypeClass* pType = this->GetType();
	if (!pType)
		return CoordStruct::Empty;

	CoordStruct parentPos = this->Parent->GetCoords();

	// Calculate escort position based on formation offset and spacing
	DirStruct parentFacing = this->Parent->PrimaryFacing.Current();
	double angle = parentFacing.GetRadian<256>() + (pType->FormationOffset * Math::Pi / 180.0);

	CoordStruct escortPos = parentPos;
	escortPos.X += (int)(cos(angle) * pType->FormationSpacing);
	escortPos.Y += (int)(sin(angle) * pType->FormationSpacing);

	// Check for collision avoidance
	if (pType->AvoidParentCollision && this->IsPositionBlocked(escortPos))
	{
		return this->FindAlternativePosition(escortPos);
	}

	return escortPos;
}

CoordStruct AttachmentClass::GetOrbitPosition()
{
	// Critical null checks
	if (!this->Parent)
		return CoordStruct::Empty;

	AttachmentTypeClass* pType = this->GetType();
	if (!pType)
		return CoordStruct::Empty;

	CoordStruct parentPos = this->Parent->GetCoords();

	// Update orbit angle
	this->OrbitAngle += pType->OrbitSpeed * 0.1; // Adjust speed factor as needed
	if (this->OrbitAngle >= 2.0 * Math::Pi)
		this->OrbitAngle -= 2.0 * Math::Pi;

	// Calculate orbit position
	CoordStruct orbitPos = parentPos;
	orbitPos.X += (int)(cos(this->OrbitAngle) * pType->OrbitRadius);
	orbitPos.Y += (int)(sin(this->OrbitAngle) * pType->OrbitRadius);

	return orbitPos;
}

CoordStruct AttachmentClass::GetLineFormationPosition()
{
	// Critical null checks
	if (!this->Parent)
		return CoordStruct::Empty;

	AttachmentTypeClass* pType = this->GetType();
	if (!pType)
		return CoordStruct::Empty;

	CoordStruct parentPos = this->Parent->GetCoords();

	// Calculate line formation position
	DirStruct parentFacing = this->Parent->PrimaryFacing.Current();
	double angle = parentFacing.GetRadian<256>() + (Math::Pi / 2.0); // Perpendicular to facing

	// Offset based on formation index and priority
	int offset = (this->FormationIndex - pType->FormationPriority) * (int)pType->FormationSpacing;

	CoordStruct linePos = parentPos;
	linePos.X += (int)(cos(angle) * offset);
	linePos.Y += (int)(sin(angle) * offset);

	return linePos;
}

CoordStruct AttachmentClass::GetWedgeFormationPosition()
{
	// Critical null checks
	if (!this->Parent)
		return CoordStruct::Empty;

	AttachmentTypeClass* pType = this->GetType();
	if (!pType)
		return CoordStruct::Empty;

	CoordStruct parentPos = this->Parent->GetCoords();

	// Calculate wedge formation position
	DirStruct parentFacing = this->Parent->PrimaryFacing.Current();
	double baseAngle = parentFacing.GetRadian<256>();

	// Alternate sides for wedge formation
	bool leftSide = (this->FormationIndex % 2) == 0;
	double sideAngle = leftSide ? (baseAngle - Math::Pi / 4.0) : (baseAngle + Math::Pi / 4.0);

	int distance = ((this->FormationIndex / 2) + 1) * (int)pType->FormationSpacing;

	CoordStruct wedgePos = parentPos;
	wedgePos.X += (int)(cos(sideAngle) * distance);
	wedgePos.Y += (int)(sin(sideAngle) * distance);

	return wedgePos;
}

CoordStruct AttachmentClass::GetDiamondFormationPosition()
{
	// Critical null checks
	if (!this->Parent)
		return CoordStruct::Empty;

	AttachmentTypeClass* pType = this->GetType();
	if (!pType)
		return CoordStruct::Empty;

	CoordStruct parentPos = this->Parent->GetCoords();

	// Calculate diamond formation position
	DirStruct parentFacing = this->Parent->PrimaryFacing.Current();
	double baseAngle = parentFacing.GetRadian<256>();

	// Four cardinal directions for diamond
	double angles[4] = { 0, Math::Pi / 2.0, Math::Pi, 3.0 * Math::Pi / 2.0 };
	double angle = baseAngle + angles[this->FormationIndex % 4];

	CoordStruct diamondPos = parentPos;
	diamondPos.X += (int)(cos(angle) * pType->FormationSpacing);
	diamondPos.Y += (int)(sin(angle) * pType->FormationSpacing);

	return diamondPos;
}

bool AttachmentClass::IsPositionBlocked(CoordStruct position)
{
	// Simple collision detection - check if position is occupied
	auto pCell = MapClass::Instance.TryGetCellAt(position);
	if (!pCell)
		return true;

	// Check if cell is passable for the child unit
	if (this->Child)
	{
		auto pChildType = this->Child->GetTechnoType();
		return !pCell->IsClearToMove(pChildType->SpeedType, false, false, -1, pChildType->MovementZone, -1, false);
	}

	return false;
}

CoordStruct AttachmentClass::FindAlternativePosition(CoordStruct preferred)
{
	AttachmentTypeClass* pType = this->GetType();

	// Try positions in a spiral pattern around the preferred position
	for (int radius = 64; radius <= 256; radius += 64)
	{
		for (int angle = 0; angle < 360; angle += 45)
		{
			double radian = angle * Math::Pi / 180.0;
			CoordStruct testPos = preferred;
			testPos.X += (int)(cos(radian) * radius);
			testPos.Y += (int)(sin(radian) * radius);

			if (!this->IsPositionBlocked(testPos))
				return testPos;
		}
	}

	// If no alternative found, return preferred position anyway
	return preferred;
}

void AttachmentClass::UpdateMovementHistory()
{
	if (!this->Parent)
		return;

	CoordStruct currentPos = this->Parent->GetCoords();
	DirStruct currentFacing = this->Parent->PrimaryFacing.Current();
	int currentFrame = this->GetCurrentGameFrame();

	// Only update if parent has moved significantly or facing has changed
	bool positionChanged = this->LastParentPosition.DistanceFrom(currentPos) > 32;
	bool facingChanged = this->ParentMovementHistory.empty() ||
		abs(this->ParentMovementHistory.front().Facing.Raw - currentFacing.Raw) > 8;

	if (positionChanged || facingChanged)
	{
		TrailPoint newPoint(currentPos, currentFacing, currentFrame);
		this->ParentMovementHistory.push_front(newPoint);
		this->LastParentPosition = currentPos;

		// Limit history size to prevent memory bloat
		while (this->ParentMovementHistory.size() > 50)
		{
			this->ParentMovementHistory.pop_back();
		}
	}
}

void AttachmentClass::InitializeFormationData()
{
	this->LastParentPosition = this->Parent ? this->Parent->GetCoords() : CoordStruct::Empty;
	this->CurrentFormationPosition = CoordStruct::Empty;
	this->OrbitAngle = 0.0;
	this->FormationIndex = 0; // This should be set by the parent when creating multiple attachments
	this->ParentMovementHistory.clear();
	this->PositionUpdateTimer.Start(1); // Update every frame initially
}

void AttachmentClass::AI()
{
	// Critical null checks to prevent crashes
	if (!this->Parent || !this->Data)
		return;

	AttachmentTypeClass* pType = this->GetType();
	if (!pType)
		return;

	if (!this->Child)
	{
		if (pType->RespawnDelay == 0)
		{
			this->CreateChild();
		}
		else if (pType->RespawnDelay > 0)
		{
			if (!this->RespawnTimer.HasStarted())
			{
				this->RespawnTimer.Start(pType->RespawnDelay);
			}
			else if (this->RespawnTimer.Completed())
			{
				this->CreateChild();
				this->RespawnTimer.Stop();
			}
		}
	}

	if (this->Child)
	{
		if (this->Child->InLimbo && !this->Parent->InLimbo)
			this->Unlimbo();
		else if (!this->Child->InLimbo && this->Parent->InLimbo)
			this->Limbo();

		this->Child->SetLocation(this->GetChildLocation());

		// Enhanced trail system: Use independent facing for trail formations
		if (pType->FormationType == AttachmentFormationType::Trail)
		{
			// Update trail movement logic
			this->UpdateTrailMovement();

			// Use calculated child facing instead of parent facing
			DirStruct childDir = this->CurrentChildFacing;
			childDir.Raw += DirStruct(this->Data->RotationAdjust).Raw;
			this->Child->PrimaryFacing.SetCurrent(childDir);
		}
		else
		{
			// For non-trail formations, use parent facing as before
			DirStruct childDir = this->Data->IsOnTurret
				? this->Parent->SecondaryFacing.Current() : this->Parent->PrimaryFacing.Current();

			childDir.Raw += DirStruct(this->Data->RotationAdjust).Raw; // overflow = free modulo for rotation
			this->Child->PrimaryFacing.SetCurrent(childDir);
		}
		// TODO handle secondary facing in case the turret is idle

		FootClass* pParentAsFoot = abstract_cast<FootClass*>(this->Parent);
		FootClass* pChildAsFoot = abstract_cast<FootClass*>(this->Child);
		if (pParentAsFoot && pChildAsFoot)
		{
			pChildAsFoot->TubeIndex = pParentAsFoot->TubeIndex;
		}

		if (pType->InheritStateEffects)
		{
			this->Child->IsFallingDown = this->Parent->IsFallingDown;
			this->Child->WasFallingDown = this->Parent->WasFallingDown;
			this->Child->CloakState = this->Parent->CloakState;
			this->Child->WarpingOut = this->Parent->WarpingOut;
			this->Child->unknown_280 = this->Parent->unknown_280; // sth related to teleport
			this->Child->BeingWarpedOut = this->Parent->BeingWarpedOut;
			this->Child->Deactivated = this->Parent->Deactivated;
			this->Child->Flash(this->Parent->Flashing.DurationRemaining);

			this->Child->IronCurtainTimer = this->Parent->IronCurtainTimer;
			this->Child->IdleActionTimer = this->Parent->IdleActionTimer;
			this->Child->IronTintTimer = this->Parent->IronTintTimer;
			this->Child->CloakDelayTimer = this->Parent->CloakDelayTimer;
			this->Child->ChronoLockRemaining = this->Parent->ChronoLockRemaining;
			this->Child->Berzerk = this->Parent->Berzerk;
			this->Child->BerzerkDurationLeft = this->Parent->BerzerkDurationLeft;
			this->Child->ChronoWarpedByHouse = this->Parent->ChronoWarpedByHouse;
			this->Child->EMPLockRemaining = this->Parent->EMPLockRemaining;
			this->Child->ShouldLoseTargetNow = this->Parent->ShouldLoseTargetNow;
		}

		if (pType->InheritOwner)
			this->Child->SetOwningHouse(this->Parent->GetOwningHouse(), false);

		// Mission inheritance
		if (pType->InheritMission) {
			Mission currentParentMission = this->Parent->CurrentMission;
			if (currentParentMission != this->LastParentMission) {
				this->Child->QueueMission(currentParentMission, false);
				this->LastParentMission = currentParentMission;
			}
		}

		// Target inheritance
		if (pType->InheritTarget) {
			AbstractClass* currentParentTarget = this->Parent->Target;
			if (currentParentTarget != this->LastParentTarget) {
				this->Child->SetTarget(currentParentTarget);
				this->LastParentTarget = currentParentTarget;
			}
		}
	}
}

// Called in Kill_Cargo, handles logics for parent destruction on children
void AttachmentClass::Destroy(TechnoClass* pSource)
{
	if (this->Child)
	{
		auto const pChildExt = TechnoExt::ExtMap.Find(this->Child);
		pChildExt->ParentAttachment = nullptr;

		auto pType = this->GetType();

		if (pType->DestructionWeapon_Child.isset())
			TechnoExt::FireWeaponAtSelf(this->Child, pType->DestructionWeapon_Child);

		if (pType->InheritDestruction && this->Child)
		{
			// Fix: Handle null source by using parent as fallback
			TechnoClass* effectiveSource = pSource ? pSource : this->Parent;
			TechnoExt::Kill(this->Child, effectiveSource);
		}
		else if (!this->Child->InLimbo && pType->ParentDestructionMission.isset())
			this->Child->QueueMission(pType->ParentDestructionMission.Get(), false);

		this->Child = nullptr;
	}
}

void AttachmentClass::ChildDestroyed()
{
	if (this->Child)
	{
		if (auto const pChildExt = TechnoExt::ExtMap.Find(this->Child))
			pChildExt->ParentAttachment = nullptr;

		AttachmentTypeClass* pType = this->GetType();
		if (pType->DestructionWeapon_Parent.isset())
			TechnoExt::FireWeaponAtSelf(this->Parent, pType->DestructionWeapon_Parent);

		this->Child = nullptr;
	}
}

void AttachmentClass::Unlimbo()
{
	if (this->Child)
	{
		CoordStruct childCoord = TechnoExt::GetFLHAbsoluteCoords(
			this->Parent, this->Data->FLH, this->Data->IsOnTurret);

		DirStruct childDir = this->Data->IsOnTurret
			? this->Parent->SecondaryFacing.Current() : this->Parent->PrimaryFacing.Current();

		childDir.Raw += DirStruct(this->Data->RotationAdjust).Raw; // overflow = free modulo for rotation

		++Unsorted::ScenarioInit;
		this->Child->Unlimbo(childCoord, childDir.GetDir());
		--Unsorted::ScenarioInit;
	}
}

void AttachmentClass::Limbo()
{
	if (this->Child)
		this->Child->Limbo();
}

bool AttachmentClass::AttachChild(TechnoClass* pChild)
{
	if (this->Child)
		return false;

	if (pChild->WhatAmI() != AbstractType::Unit)
		return false;

	if (auto const pChildAsFoot = abstract_cast<FootClass*>(pChild))
	{
		if (IPersistPtr pLocoPersist = pChildAsFoot->Locomotor)
		{
			CLSID locoCLSID { };
			if (SUCCEEDED(pLocoPersist->GetClassID(&locoCLSID))
				&& locoCLSID != __uuidof(AttachmentLocomotionClass))
			{
				LocomotionClass::ChangeLocomotorTo(pChildAsFoot,
					__uuidof(AttachmentLocomotionClass));
			}
		}
	}

	this->Child = pChild;

	auto pChildExt = TechnoExt::ExtMap.Find(this->Child);
	pChildExt->ParentAttachment = this;

	// bandaid for jitterless drawing. TODO fix properly
	// this->Child->GetTechnoType()->DisableVoxelCache = true;
	// this->Child->GetTechnoType()->DisableShadowCache = true;

	AttachmentTypeClass* pType = this->GetType();

	if (pType->InheritOwner)
	{
		if (auto pController = this->Child->MindControlledBy)
			pController->CaptureManager->FreeUnit(this->Child);
	}

	// Initialize inheritance state
	this->LastParentMission = this->Parent->CurrentMission;
	this->LastParentTarget = this->Parent->Target;

	// Apply initial inheritance if enabled
	if (pType->InheritMission) {
		this->Child->QueueMission(this->Parent->CurrentMission, false);
	}
	if (pType->InheritTarget) {
		this->Child->SetTarget(this->Parent->Target);
	}

	// Initialize formation data
	this->InitializeFormationData();

	return true;
}

bool AttachmentClass::DetachChild()
{
	if (this->Child)
	{
		AttachmentTypeClass* pType = this->GetType();

		if (!this->Child->InLimbo && pType->ParentDetachmentMission.isset())
			this->Child->QueueMission(pType->ParentDetachmentMission.Get(), false);

		// FIXME this won't work probably
		if (pType->InheritOwner)
			this->Child->SetOwningHouse(this->Parent->GetOriginalOwner(), false);

		// remove the attachment locomotor manually just to be safe
		if (auto const pChildAsFoot = abstract_cast<FootClass*>(this->Child))
			LocomotionClass::End_Piggyback(pChildAsFoot->Locomotor);

		auto pChildExt = TechnoExt::ExtMap.Find(this->Child);
		pChildExt->ParentAttachment = nullptr;
		this->Child = nullptr;

		return true;
	}

	return false;
}


void AttachmentClass::InvalidatePointer(void* ptr)
{
	AnnounceInvalidPointer(this->Parent, ptr);
	AnnounceInvalidPointer(this->Child, ptr);
}

#pragma region Save/Load

template <typename T>
bool AttachmentClass::Serialize(T& stm)
{
	return stm
		.Process(this->Data)
		.Process(this->Parent)
		.Process(this->Child)
		.Process(this->RespawnTimer)
		.Process(this->LastParentMission)
		.Process(this->LastParentTarget)
		.Process(this->ParentMovementHistory)
		.Process(this->PositionUpdateTimer)
		.Process(this->LastParentPosition)
		.Process(this->CurrentFormationPosition)
		.Process(this->CurrentChildFacing)
		.Process(this->TargetTrailPosition)
		.Process(this->TargetTrailFacing)
		.Process(this->OrbitAngle)
		.Process(this->FormationIndex)
		.Process(this->LastUpdateFrame)
		.Process(this->IsMovingToTrailPosition)
		.Success();
}

bool AttachmentClass::Load(PhobosStreamReader& stm, bool RegisterForChange)
{
	return Serialize(stm);
}

bool AttachmentClass::Save(PhobosStreamWriter& stm) const
{
	return const_cast<AttachmentClass*>(this)->Serialize(stm);
}

// Enhanced trail system implementation
void AttachmentClass::UpdateTrailMovement()
{
	if (!this->Child || !this->Parent)
		return;

	AttachmentTypeClass* pType = this->GetType();
	if (!pType || pType->FormationType != AttachmentFormationType::Trail)
		return;

	// Update movement history
	this->UpdateMovementHistory();

	int trailDelay = Math::max(1, pType->TrailDelay.Get());

	if (this->ParentMovementHistory.size() >= trailDelay)
	{
		// Get target trail point
		const TrailPoint& targetPoint = this->ParentMovementHistory[trailDelay - 1];
		this->TargetTrailPosition = targetPoint.Position;
		this->TargetTrailFacing = targetPoint.Facing;

		// Calculate smooth movement toward target
		CoordStruct currentPos = this->Child->GetCoords();
		double distance = currentPos.DistanceFrom(this->TargetTrailPosition);

		if (distance > 16) // Only move if we're not close enough
		{
			// Calculate movement direction based on target position
			DirStruct movementDir = this->CalculateMovementFacing(currentPos, this->TargetTrailPosition);
			this->CurrentChildFacing = movementDir;
			this->IsMovingToTrailPosition = true;
		}
		else
		{
			// We're close enough, adopt the target facing
			this->CurrentChildFacing = this->TargetTrailFacing;
			this->IsMovingToTrailPosition = false;
		}
	}
}

CoordStruct AttachmentClass::InterpolateTrailPosition(const TrailPoint& from, const TrailPoint& to, double progress)
{
	progress = Math::clamp(progress, 0.0, 1.0);

	CoordStruct result;
	result.X = (int)(from.Position.X + (to.Position.X - from.Position.X) * progress);
	result.Y = (int)(from.Position.Y + (to.Position.Y - from.Position.Y) * progress);
	result.Z = (int)(from.Position.Z + (to.Position.Z - from.Position.Z) * progress);

	return result;
}

DirStruct AttachmentClass::InterpolateTrailFacing(const DirStruct& from, const DirStruct& to, double progress)
{
	progress = Math::clamp(progress, 0.0, 1.0);

	// Handle direction wrapping (0-255)
	int fromRaw = from.Raw;
	int toRaw = to.Raw;

	// Find shortest rotation path
	int diff = toRaw - fromRaw;
	if (diff > 128) diff -= 256;
	else if (diff < -128) diff += 256;

	int result = fromRaw + (int)(diff * progress);
	if (result < 0) result += 256;
	else if (result > 255) result -= 256;

	return DirStruct(result);
}

DirStruct AttachmentClass::CalculateMovementFacing(CoordStruct from, CoordStruct to)
{
	int deltaX = to.X - from.X;
	int deltaY = to.Y - from.Y;

	if (deltaX == 0 && deltaY == 0)
		return this->CurrentChildFacing; // No movement, keep current facing

	// Calculate angle in radians
	double angle = atan2(deltaY, deltaX);

	// Convert to game direction (0-255, where 0 is north)
	// Game uses: 0=North, 64=East, 128=South, 192=West
	int dir = (int)((angle * 128.0 / Math::Pi) + 64) % 256;
	if (dir < 0) dir += 256;

	return DirStruct(dir);
}

double AttachmentClass::CalculateTrailProgress()
{
	int currentFrame = this->GetCurrentGameFrame();
	int frameDiff = currentFrame - this->LastUpdateFrame;

	// Smooth progress based on frame timing
	return Math::clamp(frameDiff / 15.0, 0.0, 1.0); // 15 frames for full transition
}

int AttachmentClass::GetCurrentGameFrame()
{
	// Use game's frame counter - this is a simple implementation
	// In a real scenario, you'd want to use the actual game frame counter
	static int frameCounter = 0;
	return ++frameCounter;
}

// Train-inspired direct following implementation
CoordStruct AttachmentClass::GetDirectFollowPosition()
{
	// Critical null checks
	if (!this->Parent)
		return CoordStruct::Empty;

	AttachmentTypeClass* pType = this->GetType();
	if (!pType)
		return CoordStruct::Empty;

	// Find the attachment immediately ahead in the chain
	AttachmentClass* leadingAttachment = this->GetLeadingAttachment();
	if (leadingAttachment && leadingAttachment->Child)
	{
		CoordStruct leadPos = leadingAttachment->Child->GetCoords();
		DirStruct leadFacing = leadingAttachment->Child->PrimaryFacing.Current();

		// Calculate position behind the leading attachment
		return this->CalculateFollowPosition(leadPos, leadFacing);
	}

	// Fall back to following parent directly
	CoordStruct parentPos = this->Parent->GetCoords();
	DirStruct parentFacing = this->Parent->PrimaryFacing.Current();
	return this->CalculateFollowPosition(parentPos, parentFacing);
}

AttachmentClass* AttachmentClass::GetLeadingAttachment()
{
	// Critical null checks
	if (!this->Parent)
		return nullptr;

	// For now, implement a simple approach: find the attachment with the lowest FormationIndex
	// This is a placeholder implementation until we have proper chain management
	AttachmentClass* leadingAttachment = nullptr;
	int lowestIndex = this->FormationIndex;

	// Search through all attachments to find the one immediately ahead
	for (auto pAttachment : AttachmentClass::Array)
	{
		if (pAttachment && pAttachment->Parent == this->Parent &&
			pAttachment != this && pAttachment->Child)
		{
			// Find attachment with index just below ours
			if (pAttachment->FormationIndex < this->FormationIndex &&
				(leadingAttachment == nullptr || pAttachment->FormationIndex > leadingAttachment->FormationIndex))
			{
				leadingAttachment = pAttachment;
			}
		}
	}

	return leadingAttachment;
}

CoordStruct AttachmentClass::CalculateFollowPosition(CoordStruct leadPos, DirStruct leadFacing)
{
	AttachmentTypeClass* pType = this->GetType();
	if (!pType)
		return leadPos;

	// Calculate position behind the leader based on formation spacing
	double spacing = pType->FormationSpacing.Get();

	// Convert facing to radians (game uses 0-255, where 0 is north)
	double angle = leadFacing.GetRadian<256>();

	// Calculate offset position behind the leader
	CoordStruct followPos = leadPos;
	followPos.X -= static_cast<int>(cos(angle) * spacing);
	followPos.Y -= static_cast<int>(sin(angle) * spacing);

	// Check for collision avoidance if enabled
	if (pType->AvoidParentCollision && this->IsPositionBlocked(followPos))
	{
		return this->FindAlternativePosition(followPos);
	}

	return followPos;
}

#pragma endregion
