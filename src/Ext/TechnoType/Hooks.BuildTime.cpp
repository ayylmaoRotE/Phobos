#include "Body.h"
#include <TechnoTypeClass.h>
#include <RulesClass.h>
#include <Ext/BuildingType/Body.h>
#include <Utilities/Debug.h>

// Override build speed calculation to use custom BuildTime.Cost
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

	Debug::Log("TechnoTypeClass_GetBuildSpeed for %s: result=%d\n", pThis->ID, result);

	R->EAX(result);
	return 0x711EDE;
}