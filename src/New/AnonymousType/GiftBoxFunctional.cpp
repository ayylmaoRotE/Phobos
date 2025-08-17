#include "GiftBoxFunctional.h"

#include <Ext/TechnoType/Body.h>
#include <Ext/Techno/Body.h>
#include "GiftBox.h"
#include "GiftBoxData.h"

const bool OpenDisallowed(TechnoClass* const pTechno)
{
	if (pTechno)
	{
		if (pTechno->InLimbo)
			return false;

		const bool bIsOnWarfactory = false;

		return pTechno->Absorbed ||
			pTechno->InOpenToppedTransport ||
			bIsOnWarfactory ||
			pTechno->TemporalTargetingMe;
	}

	return false;
}

void GiftBoxFunctional::Init(TechnoExtData* pExt, TechnoTypeExtData* pTypeExt)
{
	Debug::Log("GiftBoxFunctional::Init called, Enable=%d\n", pTypeExt->MyGiftBoxData.Enable.Get());
	
	if (!pTypeExt->MyGiftBoxData.Enable)
		return;

	auto const nDelay = pTypeExt->MyGiftBoxData.DelayMax == 0 ?
		pTypeExt->MyGiftBoxData.Delay :
		ScenarioClass::Instance->Random.Random() %
		(pTypeExt->MyGiftBoxData.DelayMax - pTypeExt->MyGiftBoxData.DelayMin + 1) +
		pTypeExt->MyGiftBoxData.DelayMin;

	Debug::Log("GiftBox: Creating with delay %d\n", nDelay);
	pExt->MyGiftBox = std::make_unique<GiftBox>(nDelay);
	Debug::Log("GiftBox: Created successfully\n");
}

void GiftBoxFunctional::Destroy(TechnoExtData* pExt, TechnoTypeExtData* pTypeExt)
{
	if (!pExt->MyGiftBox || OpenDisallowed(pExt->OwnerObject()))
		return;

	if (pTypeExt->MyGiftBoxData.OpenWhenDestoryed && !pExt->MyGiftBox->IsOpen)
	{
		pExt->MyGiftBox->Release(pExt->OwnerObject(), pTypeExt->MyGiftBoxData);
		pExt->MyGiftBox->IsOpen = true;
	}
}

void GiftBoxFunctional::AI(TechnoExtData* pExt, TechnoTypeExtData* pTypeExt)
{
	if (!pExt->MyGiftBox || OpenDisallowed(pExt->OwnerObject()))
		return;

	if (!pTypeExt->MyGiftBoxData.Enable){
		pExt->MyGiftBox.reset(nullptr);
		return;
	}

	// Debug: Check if we can open
	bool canOpen = pExt->MyGiftBox->CanOpen();
	bool timeup = pExt->MyGiftBox->Timeup();
	Debug::Log("GiftBox AI: CanOpen=%d, Timeup=%d, IsOpen=%d, Delay=%d\n", 
		canOpen, timeup, pExt->MyGiftBox->IsOpen, pExt->MyGiftBox->Delay);

	// Check if we should open the box (not opened yet but timer expired)
	if (!pExt->MyGiftBox->IsOpen && pExt->MyGiftBox->Timeup())
	{
		if (!pTypeExt->MyGiftBoxData.OpenWhenDestoryed && 
			!pTypeExt->MyGiftBoxData.OpenWhenHealthPercent.isset())
		{
			Debug::Log("GiftBox: Timer expired, opening and releasing gifts\n");
			pExt->MyGiftBox->Release(pExt->OwnerObject(), pTypeExt->MyGiftBoxData);
			pExt->MyGiftBox->IsOpen = true;
		}
	}

	if (pExt->MyGiftBox->IsOpen)
	{
		Debug::Log("GiftBox: Is open, checking remove/destroy\n");
		
		if (pTypeExt->MyGiftBoxData.Remove)
		{
			Debug::Log("GiftBox: Removing original unit\n");
			pExt->OwnerObject()->Limbo();
			pExt->OwnerObject()->UnInit();
			return;
		}

		if (pTypeExt->MyGiftBoxData.Destroy)
		{
			Debug::Log("GiftBox: Destroying original unit\n");
			auto nDamage = (pExt->OwnerObject()->GetTechnoType()->Strength);
			pExt->OwnerObject()->ReceiveDamage(&nDamage, 0, RulesClass::Instance->C4Warhead, nullptr, false,
				!pExt->OwnerObject()->GetTechnoType()->Crewed, nullptr);
			return;
		}

		if(pExt->OwnerObject()->IsAlive)
		{
			auto const nDelay = pTypeExt->MyGiftBoxData.DelayMax == 0 ?
				pTypeExt->MyGiftBoxData.Delay :
				ScenarioClass::Instance->Random.Random() %
				(pTypeExt->MyGiftBoxData.DelayMax - pTypeExt->MyGiftBoxData.DelayMin + 1) +
				pTypeExt->MyGiftBoxData.DelayMin;

			Debug::Log("GiftBox: Resetting with delay %d\n", nDelay);
			pExt->MyGiftBox->Reset(nDelay);
		}
	}
}

void GiftBoxFunctional::TakeDamage(TechnoExtData* pExt, TechnoTypeExtData* pTypeExt, WarheadTypeClass* pWH, DamageState nState)
{
	if (!pExt->MyGiftBox.get())
		return;

	if (nState != DamageState::NowDead &&
		(!OpenDisallowed(pExt->OwnerObject())) &&
		pTypeExt->MyGiftBoxData.OpenWhenHealthPercent.isset())
	{
		double healthPercent = pExt->OwnerObject()->GetHealthPercentage();
		if (healthPercent <= pTypeExt->MyGiftBoxData.OpenWhenHealthPercent.Get())
		{
			pExt->MyGiftBox->Release(pExt->OwnerObject(), pTypeExt->MyGiftBoxData);
			pExt->MyGiftBox->IsOpen = true;
		}
	}
}