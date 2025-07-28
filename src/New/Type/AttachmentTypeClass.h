#pragma once

#include <Utilities/Enum.h>
#include <Utilities/Enumerable.h>
#include <Utilities/Template.h>

#include <TechnoTypeClass.h>

class AttachmentTypeClass final : public Enumerable<AttachmentTypeClass>
{
public:
	Valueable<bool> RespawnAtCreation; // whether to spawn the attachment initially
	Valueable<int> RespawnDelay;
	Valueable<bool> InheritCommands;
	Valueable<bool> InheritCommands_StopCommand;
	Valueable<bool> InheritCommands_DeployCommand;
	Valueable<bool> InheritOwner; // aka mind control inheritance
	Valueable<bool> InheritStateEffects; // phasing out, stealth etc.
	Valueable<bool> InheritDestruction;
	Valueable<bool> InheritHeightStatus;
	Valueable<bool> OccupiesCell;
	Valueable<bool> LowSelectionPriority;
	Valueable<bool> TransparentToMouse;
	Valueable<AttachmentYSortPosition> YSortPosition;
	Nullable<WeaponTypeClass*> DestructionWeapon_Child;
	Nullable<WeaponTypeClass*> DestructionWeapon_Parent;
	Nullable<Mission> ParentDestructionMission;
	Nullable<Mission> ParentDetachmentMission;
	Valueable<bool> InheritMission;
	Valueable<bool> InheritTarget;
	Valueable<bool> CanBeTargeted;
	Valueable<bool> CanBeDamaged;

	// Formation system properties
	Valueable<AttachmentFormationType> FormationType;
	Valueable<double> FormationSpacing;
	Valueable<double> FormationOffset;
	Valueable<int> FormationPriority;
	Valueable<bool> MaintainFormationInCombat;
	Valueable<bool> AvoidParentCollision;
	Valueable<int> TrailDelay;
	Valueable<TrailMode> TrailMode;
	Valueable<bool> UseNativeLocomotion;
	Valueable<double> OrbitRadius;
	Valueable<double> OrbitSpeed;
	Valueable<bool> SmoothMovement;

	AttachmentTypeClass(const char* pTitle = NONE_STR) : Enumerable<AttachmentTypeClass>(pTitle)
		, RespawnAtCreation { true }
		, RespawnDelay { -1 }
		, InheritCommands { true }
		, InheritCommands_StopCommand { true }
		, InheritCommands_DeployCommand { true }
		, InheritOwner { true }
		, InheritStateEffects { true }
		, OccupiesCell { true }
		, InheritDestruction { true }
		, InheritHeightStatus { true }
		, LowSelectionPriority { true }
		, TransparentToMouse { false }
		, YSortPosition { AttachmentYSortPosition::Default }
		, DestructionWeapon_Child { }
		, DestructionWeapon_Parent { }
		, ParentDestructionMission { }
		, ParentDetachmentMission { }
		, InheritMission { false }
		, InheritTarget { false }
		, CanBeTargeted { true }
		, CanBeDamaged { true }
		, FormationType { AttachmentFormationType::Follow }
		, FormationSpacing { 64.0 }
		, FormationOffset { 0.0 }
		, FormationPriority { 0 }
		, MaintainFormationInCombat { true }
		, AvoidParentCollision { true }
		, TrailDelay { 5 }
		, TrailMode { TrailMode::Historical }
		, UseNativeLocomotion { false }
		, OrbitRadius { 128.0 }
		, OrbitSpeed { 1.0 }
		, SmoothMovement { true }
	{ }

	virtual ~AttachmentTypeClass() = default;

	virtual void LoadFromINI(CCINIClass* pINI);
	virtual void LoadFromStream(PhobosStreamReader& Stm);
	virtual void SaveToStream(PhobosStreamWriter& Stm);

private:
	template <typename T>
	void Serialize(T& Stm);
};
