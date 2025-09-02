// Hooks.ManualOrders.cpp — per-unit, deterministic harvester Stop/Clear handling

#include <GeneralDefinitions.h>
#include <ObjectClass.h>
#include <UnitClass.h>

#include <Ext/Techno/Body.h>
#include <Ext/TechnoType/Body.h>
#include <Ext/Rules/Body.h>
#include <Unsorted.h>

// Is this unit a miner/ore gatherer?
static __forceinline bool IsMiner(const UnitClass* u) noexcept
{
	const auto t = u ? u->Type : nullptr;
	return t && (t->Harvester || t->Weeder);
}

// Visit all command targets in the engine's replicated list (NOT local selection)
template<typename Func>
static __forceinline void ForEachCommandObjectMiner(Func&& fn)
{
	auto& cmd = ObjectClass::CurrentObjects;  // DynamicVectorClass<ObjectClass*>
	const int n = cmd.Count;
	for (int i = 0; i < n; ++i)
	{
		if (auto* obj = cmd.GetItem(i))
		{
			if (auto* u = abstract_cast<UnitClass*>(obj))
			{
				if (IsMiner(u))
				{
					if (auto* ext = TechnoExt::ExtMap.Find(u))
					{
						fn(u, ext);
					}
				}
			}
		}
	}
}

// Replicated, per-target apply/clear
static void SetSuppressedForCommandObjects(bool value)
{
	ForEachCommandObjectMiner([&](UnitClass* /*u*/, TechnoExt::ExtData* ext)
 {
	 if (value)
	 {
		 // STOP: mark intent only; consumption happens in synced update (UpdateHarvesterAutoReturn)
		 ext->Harv_MarkPendingStop();
		 // Safety: drop any in-flight confirm so nothing fires next frame
		 ext->Harvester_AutoReturn_ConfirmFrame = INT_MIN;
	 }
	 else
	 {
		 // Manual clear (Guard / Deploy / AttackMove / Manual Harvest)
		 ext->Harvester_AutoReturn_SetSuppressed(false);
		 ext->Harv_MarkManualClear();
		 ext->Harvester_AutoReturn_ConfirmFrame = INT_MIN;
	 }
	});
}

// STOP — replicate only; actual changes apply in synced AI update
// Addresses: these are the stock RA2/YR command Execute() hooks
DEFINE_HOOK(0x00730EA0, StopCommandClass_Execute_AutoReturn_Suppress, 0x5)
{
	const auto rules = RulesExt::Global();
	if (rules && rules->Harvester_AutoReturn_SuppressOnStop)
	{
		SetSuppressedForCommandObjects(true);
	}
	return 0;
}

// GUARD / DEPLOY / ATTACK-MOVE — clear suppression explicitly for the targets
DEFINE_HOOK(0x00730D60, GuardCommandClass_Execute_AutoReturn_Clear, 0x5) { SetSuppressedForCommandObjects(false); return 0; }
DEFINE_HOOK(0x00730AF0, DeployCommandClass_Execute_AutoReturn_Clear, 0x8) { SetSuppressedForCommandObjects(false); return 0; }
DEFINE_HOOK(0x00731AF0, AttackMoveCommandClass_Execute_AutoReturn_Clear, 0x5) { SetSuppressedForCommandObjects(false); return 0; }

// Harvest mission entry — treat as manual clear unless we just auto-issued Harvest
DEFINE_HOOK(0x0073D450, UnitClass_Harvest_AutoReturn_Clear, 0x5)
{
	GET(UnitClass*, self, ECX);
	if (!self || !IsMiner(self)) { return 0; }

	if (auto* ext = TechnoExt::ExtMap.Find(self))
	{
		const auto* te = ext->TypeExtData;
		if (te && te->Harvester_AutoReturn_GlobalEligible)
		{

			// Clear if this unit is in the replicated command target list (manual harvest),
			// OR if we did NOT just auto-issue harvest ourselves (deterministic stamp).
			bool inReplicated = false;
			auto& cmd = ObjectClass::CurrentObjects;
			for (int i = 0; i < cmd.Count; ++i)
			{
				if (cmd.GetItem(i) == self) { inReplicated = true; break; }
			}

			if (inReplicated || !ext->Harv_IssuedAutoReturnRecently(1))
			{
				ext->Harvester_AutoReturn_SetSuppressed(false);
				ext->Harv_MarkManualClear();
				ext->Harvester_AutoReturn_ConfirmFrame = INT_MIN;
			}
		}
	}
	return 0;
}
