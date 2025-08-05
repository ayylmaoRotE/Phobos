#include "Body.h"
#include <Utilities/AresHelper.h>
#include <Ext/Rules/Body.h>
#include <HouseClass.h>
#include <TechnoClass.h>
#include <FootClass.h>

// Bugfix: TAction 7,80,107.
DEFINE_HOOK(0x65DF67, TeamTypeClass_CreateMembers_LoadOntoTransport, 0x6)
{
	if(!AresHelper::CanUseAres) // If you're not using Ares you don't deserve a fix
		return 0;

	GET(FootClass* const, pPayload, EAX);
	GET(FootClass* const, pTransport, ESI);
	GET(TeamClass* const, pTeam, EBP);
	GET(TeamTypeClass const*, pThis, EBX);

	auto unmarkPayloadCreated = [](FootClass* member){reinterpret_cast<char*>(member->align_154)[0x9E] = false;};

	if (!pTransport)
	{
		for (auto pNext = pPayload;
		pNext && pNext != pTransport && pNext->Team == pTeam;
		pNext = abstract_cast<FootClass*>(pNext->NextObject))
			unmarkPayloadCreated(pNext);

		return 0x65DFE8;
	}

	unmarkPayloadCreated(pTransport);

	if (!pPayload || !pThis->Full)
		return 0x65E004;

	const auto pType = pTransport->GetTechnoType();
	const bool isTransportOpenTopped = pType->OpenTopped;
	FootClass* pGunner = nullptr;

	for (auto pNext = pPayload;
		pNext && pNext != pTransport && pNext->Team == pTeam;
		pNext = abstract_cast<FootClass*>(pNext->NextObject))
	{
		pPayload->Transporter = pTransport;
		pGunner = pNext;

		if (isTransportOpenTopped)
			pTransport->EnteredOpenTopped(pNext);
	}

	// Add to transport - this will load the payload object and everything linked to it (rest of the team) in reverse order
	pTransport->Passengers.AddPassenger(pPayload);

	// Handle gunner change - this is the 'last' passenger because of reverse order
	if (pType->Gunner && pGunner)
		pTransport->ReceiveGunner(pGunner);

	return 0x65DF8D;
}

// Build test hook temporarily disabled
// DEFINE_HOOK(0x6E8A90, TeamHooks_BuildTest, 0x6)
// {
//     Debug::Log("[BUILD TEST] Team hooks loaded successfully!\n");
//     return 0;
// }

// TeamRetaliate hook temporarily disabled for testing
/*
// Improved TeamRetaliate implementation with target persistence  
// Prevents "ballistic" behavior where teams constantly switch targets
DEFINE_HOOK(0x6EB432, TeamClass_AttackedBy_Retaliate, 0x9)
{
	GET(TeamClass*, pThis, ESI);
	GET(AbstractClass*, pAttacker, EBP);

	if (RulesExt::Global()->TeamRetaliate)
	{
		Debug::Log("[RETALIATE] TeamRetaliate enabled, team attacked\n");
		
		auto pTeamExt = TeamExt::ExtMap.Find(pThis);
		if (!pTeamExt)
		{
			Debug::Log("[RETALIATE] No team extension found!\n");
			return 0x6EB47A;
		}

		// Skip invalid attackers
		if (pAttacker->WhatAmI() == AbstractType::Aircraft)
			return 0x6EB47A;

		auto pAttackerTechno = abstract_cast<TechnoClass*>(pAttacker);
		if (!pAttackerTechno)
			return 0x6EB47A;

		// Skip allied attackers
		if (pThis->Owner->IsAlliedWith(pAttackerTechno->Owner))
			return 0x6EB47A;

		// Skip invalid foot units
		if (auto pAttackerFoot = abstract_cast<FootClass*>(pAttacker)) {
			if (pAttackerFoot->InLimbo || pAttackerFoot->GetTechnoType()->ConsideredAircraft) {
				return 0x6EB47A;
			}
		}

		// Check if we have a valid current target that we should persist with
		auto pCurrentTarget = abstract_cast<TechnoClass*>(pTeamExt->RetaliateCurrentTarget);
		bool shouldSwitchTarget = false;

		// Additional safety: validate current target pointer
		if (pCurrentTarget && !VTable::Get(pCurrentTarget))
			pCurrentTarget = nullptr;

		if (!pCurrentTarget || !pCurrentTarget->IsAlive || pCurrentTarget->InLimbo) {
			// Current target is invalid, switch immediately
			shouldSwitchTarget = true;
		}
		else if (pTeamExt->RetaliateTargetPersistTimer.Expired()) {
			// Timer expired, we can consider switching targets
			
			// Calculate priority scores for current vs new target
			int currentPriority = 0;  
			int attackerPriority = 0;

			// Distance priority (closer = higher priority) - use team's first unit as reference
			if (pThis->FirstUnit) {
				int currentDistance = pCurrentTarget->DistanceFrom(pThis->FirstUnit);
				int attackerDistance = pAttackerTechno->DistanceFrom(pThis->FirstUnit);
				
				// Closer targets get higher priority (inverted scoring)
				currentPriority += std::max(0, 10000 - currentDistance / 256);
				attackerPriority += std::max(0, 10000 - attackerDistance / 256);
			}

			// Health priority (lower health = higher priority for finishing off)
			currentPriority += (100 - (pCurrentTarget->Health * 100 / pCurrentTarget->GetTechnoType()->Strength));
			attackerPriority += (100 - (pAttackerTechno->Health * 100 / pAttackerTechno->GetTechnoType()->Strength));

			// Threat priority (armed units get higher priority)
			if (pCurrentTarget->IsArmed()) currentPriority += 50;
			if (pAttackerTechno->IsArmed()) attackerPriority += 50;

			// Switch only if new target is significantly better (25% threshold)
			if (attackerPriority > currentPriority * 1.25) {
				shouldSwitchTarget = true;
			}
		}

		// Apply target switch if needed
		if (shouldSwitchTarget) {
			Debug::Log("[RETALIATE] Switching target from %p to %p\n", pCurrentTarget, pAttacker);
			pTeamExt->RetaliateCurrentTarget = pAttacker;
			pThis->Focus = pAttacker;
			
			// Start persistence timer to prevent immediate switching
			pTeamExt->RetaliateTargetPersistTimer.Start(pTeamExt->RetaliateTargetSwitchDelay);
		}
		else if (pCurrentTarget) {
			Debug::Log("[RETALIATE] Keeping current target %p (timer active)\n", pCurrentTarget);
		}
	}

	return 0x6EB47A;
}
*/
