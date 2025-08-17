#include "Body.h"
#include <FactoryClass.h>
#include <TechnoClass.h>
#include <TechnoTypeClass.h>
#include <HouseClass.h>
#include <RulesClass.h>
#include <Ext/TechnoType/Body.h>
#include <Ext/BuildingType/Body.h>
#include <Utilities/Debug.h>

// Hook FactoryClass_GetBuildTimeFrames to apply ExternalFactorySpeedBonus
DEFINE_HOOK(0x4C9FB9, FactoryClass_GetBuildTimeFrames_WithSpeedBonus, 0x6)
{
	GET(FactoryClass*, pFactory, ECX);

	Debug::Log("FactoryClass_GetBuildTimeFrames hook called\n");
	
	if (!pFactory || !pFactory->Object || !pFactory->Owner)
	{
		Debug::Log("Factory hook: Missing factory, object, or owner\n");
		return 0;
	}

	const auto pTechno = pFactory->Object;
	const auto pType = pTechno->GetTechnoType();
	const auto pOwner = pFactory->Owner;

	Debug::Log("Factory producing: %s\n", pType->ID);

	// Get normal build time - let the original code calculate it first
	int originalTime = pType->GetBuildSpeed();
	
	// Apply ExternalFactorySpeedBonus
	const auto bonus = BuildingTypeExt::ExtData::GetExternalFactorySpeedBonus(pTechno, pOwner);
	Debug::Log("Bonus for %s: %.2f\n", pType->ID, bonus);
	
	if (bonus > 1.0)
	{
		int boostedTime = int((double)originalTime / bonus); // Divide because lower time = faster
		Debug::Log("Factory speed boost: %s %d->%d frames (%.2fx faster)\n", 
			pType->ID, originalTime, boostedTime, bonus);
		R->EAX(boostedTime);
		return 0x4C9FBE; // Skip original calculation
	}

	// No bonus, continue with normal calculation
	Debug::Log("No bonus, continuing normally\n");
	return 0;
}