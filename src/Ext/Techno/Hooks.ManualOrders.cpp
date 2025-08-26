#include <MapClass.h>
#include <ObjectClass.h>
#include <UnitClass.h>

#include <Ext/Techno/Body.h>
#include <Ext/TechnoType/Body.h>
#include <Ext/Rules/Body.h>

// Is this unit a miner/ore gatherer?
static __forceinline bool IsMiner(const UnitClass* u) noexcept
{
	const auto t = u->Type;
	return t && (t->Harvester || t->Weeder);
}

static void SetSuppressedForSelectedMiners(bool value)
{
	auto& sel = ObjectClass::CurrentObjects;
	for (int i = 0; i < sel.Count; ++i)
	{
		if (auto* obj = sel.GetItem(i))
		{
			if (!obj->IsSelected) continue;
			if (auto* u = abstract_cast<UnitClass*>(obj))
			{
				if (u->Type && (u->Type->Harvester || u->Type->Weeder))
				{
					if (auto* ext = TechnoExt::ExtMap.Find(u))
					{
						if (auto* te = ext->TypeExtData; te && te->Harvester_AutoReturn_GlobalEligible)
						{
							ext->Harvester_AutoReturn_SetSuppressed(value); // <— use helper
						}
					}
				}
			}
		}
	}
}

// ==== STOP (S key) — set suppression if rules allow ====
// gamemd: 0001:0032FEA0 → 0x0072FEA0
DEFINE_HOOK(0x00730EA0, StopCommandClass_Execute_AutoReturn_Suppress, 0x5)
{
	if (const auto rules = RulesExt::Global())
	{
		if (rules->Harvester_AutoReturn_SuppressOnStop)
		{
			SetSuppressedForSelectedMiners(true);
		}
	}
	return 0;
}

// ==== GUARD — clear suppression (player gave a manual order) ====
// gamemd: 0001:0032FD60 → 0x0072FD60
DEFINE_HOOK(0x00730D60, GuardCommandClass_Execute_AutoReturn_Clear, 0x5)
{
	SetSuppressedForSelectedMiners(false);
	return 0;
}

// ==== DEPLOY — clear suppression ====
// gamemd: 0001:0032FAF0 → 0x0072FAF0
DEFINE_HOOK(0x00730AF0, DeployCommandClass_Execute_AutoReturn_Clear, 0x8)
{
	SetSuppressedForSelectedMiners(false);
	return 0;
}

// ==== ATTACK-MOVE — clear suppression ====
// gamemd: 0001:00330AF0 → 0x00730AF0
DEFINE_HOOK(0x00731AF0, AttackMoveCommandClass_Execute_AutoReturn_Clear, 0x5)
{
	SetSuppressedForSelectedMiners(false);
	return 0;
}


DEFINE_HOOK(0x0073D450, UnitClass_Harvest_AutoReturn_Clear, 0x5)
{
	// mirror the other hooks: affect only the selection
	// (AI/miner autopilot calls here won't clear anything because they aren't selected)
	SetSuppressedForSelectedMiners(false);
	return 0; // resume original at hook+size
}
