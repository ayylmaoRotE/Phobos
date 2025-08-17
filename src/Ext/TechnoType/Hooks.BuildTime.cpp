#include "Body.h"
#include <TechnoTypeClass.h>
#include <RulesClass.h>
#include <Ext/BuildingType/Body.h>
#include <Utilities/Debug.h>

// Override build speed calculation to use custom BuildTime.Cost and ExternalFactorySpeedBonus
DEFINE_HOOK(0x711EE0, TechnoTypeClass_GetBuildSpeed, 0x6)
{
	GET(TechnoTypeClass*, pThis, ECX);

	const auto pTypeExt = TechnoTypeExt::ExtMap.Find(pThis);
	if (!pTypeExt)
		return 0;

	const auto buildSpeed = pTypeExt->BuildTime_Speed.Get(RulesClass::Instance->BuildSpeed);
	const auto buildCost = pTypeExt->BuildTime_Cost.Get(pThis->Cost);

	// Formula: (BuildTime_Speed * BuildTime_Cost / 1000.0 * 900.0)
	int result = static_cast<int>(buildSpeed * buildCost / 1000.0 * 900.0);

	Debug::Log("TechnoTypeClass_GetBuildSpeed for %s: base result=%d\n", pThis->ID, result);

	// Check if this is being called for a unit being produced
	// We need to find the current producing techno to apply ExternalFactorySpeedBonus
	// For now, let's just see if this hook is being called when units are produced
	Debug::Log("GetBuildSpeed hook called for %s\n", pThis->ID);

	R->EAX(result);
	return 0x711EDE;
}