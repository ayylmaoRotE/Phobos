#pragma once
#include <BuildingClass.h>

namespace AILowHPSell
{
	// Call this if you are calling AFTER HP was subtracted this tick.
	void ConsiderOnDamage(BuildingClass* bld, int appliedDamage);

	// Call this if you are calling BEFORE HP is subtracted (pre-apply).
	// prevHP is bld->Health before the engine applies the damage.
	void ConsiderOnDamage_PreApply(BuildingClass* bld, int prevHP, int appliedDamage);
}
