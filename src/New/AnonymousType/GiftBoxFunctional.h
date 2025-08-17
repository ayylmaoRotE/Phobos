#pragma once
#include <GeneralDefinitions.h>
#include <Ext/TechnoType/Body.h>
#include <Ext/Techno/Body.h>

// Aliases to match Otamaa's naming convention
using TechnoExtData = TechnoExt::ExtData;
using TechnoTypeExtData = TechnoTypeExt::ExtData;

class WarheadTypeClass;

struct GiftBoxFunctional
{
	static void Init(TechnoExtData* pExt, TechnoTypeExtData* pTypeExt);
	static void AI(TechnoExtData* pExt, TechnoTypeExtData* pTypeExt);
	static void Destroy(TechnoExtData* pExt, TechnoTypeExtData* pTypeExt);
	static void TakeDamage(TechnoExtData* pExt, TechnoTypeExtData* pTypeExt, WarheadTypeClass* pWH, DamageState nState);
};