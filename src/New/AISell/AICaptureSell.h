#pragma once

#include <BuildingClass.h>
#include <TechnoClass.h>

namespace AICaptureSell
{
	// Returns true if it queued a SELL for pBld (MP-synced) and cleared the engineer's destination
	// so capture won't complete this frame.
	bool TryAtEngineerDoor(BuildingClass* pBld, TechnoClass* pFrom);
}
