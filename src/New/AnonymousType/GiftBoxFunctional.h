#pragma once
#include <GeneralDefinitions.h>
#include <Ext/Techno/Body.h>
#include <Ext/TechnoType/Body.h>

class TechnoClass;
class WarheadTypeClass;

struct GiftBoxFunctional
{
    static void Init(TechnoExt::ExtData* pExt, TechnoTypeExt::ExtData* pTypeExt);
    static void Destroy(TechnoExt::ExtData* pExt, TechnoTypeExt::ExtData* pTypeExt);
    static void AI(TechnoExt::ExtData* pExt, TechnoTypeExt::ExtData* pTypeExt);
    static void TakeDamage(TechnoExt::ExtData* pExt, TechnoTypeExt::ExtData* pTypeExt, WarheadTypeClass* pWH, DamageState nState);
};
