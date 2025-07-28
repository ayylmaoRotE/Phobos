#include "AttachmentTypeClass.h"

#include <Utilities/TemplateDef.h>

const char* Enumerable<AttachmentTypeClass>::GetMainSection()
{
	return "AttachmentTypes";
}

void AttachmentTypeClass::LoadFromINI(CCINIClass* pINI)
{
	const char* section = this->Name;

	INI_EX exINI(pINI);

	this->RespawnAtCreation.Read(exINI, section, "RespawnAtCreation");
	this->RespawnDelay.Read(exINI, section, "RespawnDelay");
	this->InheritCommands.Read(exINI, section, "InheritCommands");
	this->InheritCommands_StopCommand.Read(exINI, section, "InheritCommands.StopCommand");
	this->InheritCommands_DeployCommand.Read(exINI, section, "InheritCommands.DeployCommand");
	this->InheritOwner.Read(exINI, section, "InheritOwner");
	this->InheritStateEffects.Read(exINI, section, "InheritStateEffects");
	this->InheritDestruction.Read(exINI, section, "InheritDestruction");
	this->InheritHeightStatus.Read(exINI, section, "InheritHeightStatus");
	this->OccupiesCell.Read(exINI, section, "OccupiesCell");
	this->LowSelectionPriority.Read(exINI, section, "LowSelectionPriority");
	this->TransparentToMouse.Read(exINI, section, "TransparentToMouse");
	this->YSortPosition.Read(exINI, section, "YSortPosition");
	this->DestructionWeapon_Child.Read(exINI, section, "DestructionWeapon.Child");
	this->DestructionWeapon_Parent.Read(exINI, section, "DestructionWeapon.Parent");
	this->ParentDestructionMission.Read(exINI, section, "ParentDestructionMission");
	this->ParentDetachmentMission.Read(exINI, section, "ParentDetachmentMission");
	this->InheritMission.Read(exINI, section, "InheritMission");
	this->InheritTarget.Read(exINI, section, "InheritTarget");
	this->CanBeTargeted.Read(exINI, section, "CanBeTargeted");
	this->CanBeDamaged.Read(exINI, section, "CanBeDamaged");

	// Formation system properties
	this->FormationType.Read(exINI, section, "FormationType");
	this->FormationSpacing.Read(exINI, section, "FormationSpacing");
	this->FormationOffset.Read(exINI, section, "FormationOffset");
	this->FormationPriority.Read(exINI, section, "FormationPriority");
	this->MaintainFormationInCombat.Read(exINI, section, "MaintainFormationInCombat");
	this->AvoidParentCollision.Read(exINI, section, "AvoidParentCollision");
	this->TrailDelay.Read(exINI, section, "TrailDelay");
	this->OrbitRadius.Read(exINI, section, "OrbitRadius");
	this->OrbitSpeed.Read(exINI, section, "OrbitSpeed");
	this->SmoothMovement.Read(exINI, section, "SmoothMovement");
}

template <typename T>
void AttachmentTypeClass::Serialize(T& Stm)
{
	Stm
		.Process(this->RespawnAtCreation)
		.Process(this->RespawnDelay)
		.Process(this->InheritCommands)
		.Process(this->InheritCommands_StopCommand)
		.Process(this->InheritCommands_DeployCommand)
		.Process(this->InheritOwner)
		.Process(this->InheritStateEffects)
		.Process(this->InheritDestruction)
		.Process(this->InheritHeightStatus)
		.Process(this->OccupiesCell)
		.Process(this->LowSelectionPriority)
		.Process(this->TransparentToMouse)
		.Process(this->YSortPosition)
		.Process(this->DestructionWeapon_Child)
		.Process(this->DestructionWeapon_Parent)
		.Process(this->ParentDestructionMission)
		.Process(this->ParentDetachmentMission)
		.Process(this->InheritMission)
		.Process(this->InheritTarget)
		.Process(this->CanBeTargeted)
		.Process(this->CanBeDamaged)
		.Process(this->FormationType)
		.Process(this->FormationSpacing)
		.Process(this->FormationOffset)
		.Process(this->FormationPriority)
		.Process(this->MaintainFormationInCombat)
		.Process(this->AvoidParentCollision)
		.Process(this->TrailDelay)
		.Process(this->OrbitRadius)
		.Process(this->OrbitSpeed)
		.Process(this->SmoothMovement)
		;
}

void AttachmentTypeClass::LoadFromStream(PhobosStreamReader& Stm)
{
	this->Serialize(Stm);
}

void AttachmentTypeClass::SaveToStream(PhobosStreamWriter& Stm)
{
	this->Serialize(Stm);
}
