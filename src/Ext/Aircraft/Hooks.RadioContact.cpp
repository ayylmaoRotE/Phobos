#include "Body.h"
#include <AircraftClass.h>
#include <RadioClass.h>
#include <BuildingClass.h>

#include <Ext/TechnoType/Body.h>

// Prevent aircraft from establishing radio contact with airports if disabled
DEFINE_HOOK(0x415D10, AircraftClass_ReceiveCommand_DisableRadioContact, 0x6)
{
	enum { SkipRadioContact = 0x415D90, Continue = 0x0 };

	GET(AircraftClass*, pThis, ESI);
	GET(RadioCommand, command, EDX);

	// Only intercept RequestLink commands for radio contact
	if (command != RadioCommand::RequestLink)
		return Continue;

	const auto pTypeExt = TechnoTypeExt::ExtMap.Find(pThis->Type);
	if (!pTypeExt)
		return Continue;

	if (pTypeExt->NoAirportBound_DisableRadioContact)
	{
		// Reject radio contact request
		R->EAX(RadioCommand::Roger); // Return acknowledge but don't establish link
		return SkipRadioContact;
	}

	return Continue;
}

// Prevent aircraft from requesting radio contact with buildings during landing
DEFINE_HOOK(0x41AF40, AircraftClass_Mission_Landing_DisableRadioContact, 0x6)
{
	enum { SkipRadioRequest = 0x41AF80, Continue = 0x0 };

	GET(AircraftClass*, pThis, ESI);

	const auto pTypeExt = TechnoTypeExt::ExtMap.Find(pThis->Type);
	if (!pTypeExt)
		return Continue;

	if (pTypeExt->NoAirportBound_DisableRadioContact)
	{
		// Skip radio contact establishment during landing
		return SkipRadioRequest;
	}

	return Continue;
}

// Prevent automatic docking when radio contact is disabled
DEFINE_HOOK(0x41B200, AircraftClass_AI_PreventAutoDock, 0x6)
{
	enum { SkipDocking = 0x41B290, Continue = 0x0 };

	GET(AircraftClass*, pThis, ESI);

	const auto pTypeExt = TechnoTypeExt::ExtMap.Find(pThis->Type);
	if (!pTypeExt)
		return Continue;

	if (pTypeExt->NoAirportBound_DisableRadioContact)
	{
		// Prevent automatic docking behavior
		return SkipDocking;
	}

	return Continue;
}

// Additional hook: Prevent radio contact during approach sequences
DEFINE_HOOK(0x414F70, AircraftClass_AI_DisableRadioContact, 0x6)
{
	enum { SkipRadioLogic = 0x414FB0, Continue = 0x0 };

	GET(AircraftClass*, pThis, ESI);

	// Update OpenTopped aircraft passenger initialization
	if (auto pExt = TechnoExt::ExtMap.Find(pThis))
		pExt->UpdateAircraftOpentopped();

	const auto pTypeExt = TechnoTypeExt::ExtMap.Find(pThis->Type);
	if (!pTypeExt)
		return Continue;

	if (pTypeExt->NoAirportBound_DisableRadioContact)
	{
		// Skip all radio contact logic during AI update
		return SkipRadioLogic;
	}

	return Continue;
}
