#include "Body.h"

#include <SuperClass.h>
#include <BuildingClass.h>
#include <HouseClass.h>
#include <ScenarioClass.h>
#include <MessageListClass.h>

#include <Utilities/EnumFunctions.h>
#include <Utilities/GeneralUtils.h>

#include "Ext/House/Body.h"
#include "Ext/WarheadType/Body.h"
#include "Ext/WeaponType/Body.h"
#include <Ext/Scenario/Body.h>
#include "Ext/Sidebar/SWSidebar/UISafeOps.h"

// ============= New SuperWeapon Effects================

void SWTypeExt::FireSuperWeaponExt(SuperClass* pSW, const CellStruct& cell)
{
	// 🔒 Guard all pointers up-front
	if (!pSW || !pSW->Owner || !pSW->Type)
	{
		return;
	}

	auto* const pHouse = pSW->Owner;
	auto* const pType = pSW->Type;
	auto* const pTypeExt = SWTypeExt::ExtMap.Find(pType);
	if (!pTypeExt)
	{
		return;
	}

	// Execute effects — each guarded so a missing feature doesn't crash
	if (!pTypeExt->LimboDelivery_Types.empty())
	{
		pTypeExt->ApplyLimboDelivery(pHouse);
	}

	if (!pTypeExt->LimboKill_IDs.empty())
	{
		pTypeExt->ApplyLimboKill(pHouse);
	}

	if (pTypeExt->Detonate_Warhead || pTypeExt->Detonate_Weapon)
	{
		pTypeExt->ApplyDetonation(pHouse, cell);
	}

	if (!pTypeExt->SW_Next.empty())
	{
		pTypeExt->ApplySWNext(pSW, cell);
	}

	if (!pTypeExt->Convert_Pairs.empty())
	{
		pTypeExt->ApplyTypeConversion(pSW, cell);
	}

	if (!pTypeExt->SW_Link.empty())
	{
		// match header signature (one-arg)
		pTypeExt->ApplyLinkedSW(pSW, cell);
	}

	// Ares' Type=EMPulse SW
	if (static_cast<int>(pType->Type) == 28 && !pTypeExt->EMPulse_TargetSelf)
	{
		pTypeExt->HandleEMPulseLaunch(pSW, cell);
	}

	// Exactly-once BP/CP spend/earn on successful fire (no pre-deducts anywhere)
	if (pTypeExt->BattlePoints_Amount != 0)
	{
		pTypeExt->ApplyBattlePoints(pSW);
	}
	if (pTypeExt->CommanderPoints_Amount != 0)
	{
		pTypeExt->ApplyCommanderPoints(pSW);
	}

	// ShotCount++
	if (auto* const pOwnerExt = HouseExt::ExtMap.Find(pHouse))
	{
		if (pType && pType->ArrayIndex >= 0 &&
			static_cast<size_t>(pType->ArrayIndex) < pOwnerExt->SuperExts.size())
		{
			pOwnerExt->SuperExts[pType->ArrayIndex].ShotCount++;
		}
	}

	// Trigger Tags (unchanged logic, null-safe)
	auto* const pTags = &pHouse->RelatedTags;
	if (pTags && pTags->Count > 0)
	{
		int index = 0;
		int TagCount = pTags->Count;

		while (TagCount > 0 && index < TagCount)
		{
			const auto pTag = pTags->GetItem(index);
			if (pTag && (
				pTag->RaiseEvent(static_cast<TriggerEvent>(77), nullptr, CellStruct::Empty, false, (TechnoClass*)(pSW)) ||
				pTag->RaiseEvent(static_cast<TriggerEvent>(75), nullptr, CellStruct::Empty, false, (TechnoClass*)(pSW))))
			{
				if (TagCount != pTags->Count)
				{ // collection changed → resync bounds
					TagCount = pTags->Count;
					continue;
				}
			}
			++index;
		}
	}
}

// ====================================================

#pragma region LimboDelivery
inline void LimboCreate(BuildingTypeClass* pType, HouseClass* pOwner, int ID)
{
	if (!pType || !pOwner) { return; }

	// BuildLimit check goes before creation
	if (pType->BuildLimit > 0)
	{
		int sum = pOwner->CountOwnedNow(pType);

		// copy Ares' deployable units x build limit fix
		if (auto const pUndeploy = pType->UndeploysInto)
			sum += pOwner->CountOwnedNow(pUndeploy);

		if (sum >= pType->BuildLimit)
			return;
	}

	if (auto const pBuilding = static_cast<BuildingClass*>(pType->CreateObject(pOwner)))
	{
		// All of these are mandatory
		pBuilding->InLimbo = false;
		pBuilding->IsAlive = true;
		pBuilding->IsOnMap = true;

		// Campaign discovery adjustments (unchanged)
		if (SessionClass::IsCampaign())
			pBuilding->DiscoveredBy(HouseClass::CurrentPlayer);
		pBuilding->DiscoveredBy(pOwner);

		pOwner->RegisterGain(pBuilding, false);
		pOwner->RecheckTechTree = true;
		pOwner->RecheckPower = true;
		pOwner->Buildings.AddItem(pBuilding);

		// Different types of building logics
		if (pType->ConstructionYard)
			pOwner->ConYards.AddItem(pBuilding);

		if (pType->SecretLab)
			pOwner->SecretLabs.AddItem(pBuilding);

		auto const pBuildingExt = BuildingExt::ExtMap.Find(pBuilding);
		auto const pOwnerExt = HouseExt::ExtMap.Find(pOwner);

		if (pType->FactoryPlant)
		{
			if (pBuildingExt->TypeExtData->FactoryPlant_AllowTypes.size() > 0 ||
				pBuildingExt->TypeExtData->FactoryPlant_DisallowTypes.size() > 0)
			{
				if (pOwnerExt)
				{
					pOwnerExt->RestrictedFactoryPlants.push_back(pBuilding);
				}
			}
			else
			{
				pOwner->FactoryPlants.AddItem(pBuilding);
				pOwner->CalculateCostMultipliers();
			}
		}

		// LimboKill ID
		pBuildingExt->LimboID = ID;

		// Add building to list of owned limbo buildings
		if (pOwnerExt)
		{
			pOwnerExt->OwnedLimboDeliveredBuildings.push_back(pBuilding);
		}
		auto const pBldType = pBuilding->Type;

		if (!pBldType->Insignificant && !pBldType->DontScore && pOwnerExt)
			pOwnerExt->AddToLimboTracking(pBldType);

		auto const pTechnoExt = TechnoExt::ExtMap.Find(pBuilding);
		auto const pTechnoTypeExt = pTechnoExt ? pTechnoExt->TypeExtData : nullptr;

		if (pTechnoTypeExt && pTechnoTypeExt->AutoDeath_Behavior.isset())
		{
			ScenarioExt::Global()->AutoDeathObjects.push_back(pTechnoExt);

			if (pTechnoTypeExt->AutoDeath_AfterDelay > 0)
				pTechnoExt->AutoDeathTimer.Start(pTechnoTypeExt->AutoDeath_AfterDelay);
		}
	}
}

void SWTypeExt::ExtData::ApplyLimboDelivery(HouseClass* pHouse)
{
	if (!pHouse) { return; }

	// random mode
	if (!this->LimboDelivery_RandomWeightsData.empty())
	{
		int id = -1;
		const size_t idsSize = this->LimboDelivery_IDs.size();
		const auto results = this->WeightedRollsHandler(
			&this->LimboDelivery_RollChances, &this->LimboDelivery_RandomWeightsData,
			this->LimboDelivery_Types.size());

		for (size_t result : results)
		{
			if (result < idsSize)
				id = this->LimboDelivery_IDs[result];

			LimboCreate(this->LimboDelivery_Types[result], pHouse, id);
		}
	}
	// no randomness mode
	else
	{
		int id = -1;
		const size_t idsSize = this->LimboDelivery_IDs.size();

		for (size_t i = 0; i < this->LimboDelivery_Types.size(); i++)
		{
			if (i < idsSize)
				id = this->LimboDelivery_IDs[i];

			LimboCreate(this->LimboDelivery_Types[i], pHouse, id);
		}
	}
}

void SWTypeExt::ExtData::ApplyLimboKill(HouseClass* pHouse)
{
	if (!pHouse) { return; }

	for (int limboKillID : this->LimboKill_IDs)
	{
		for (HouseClass* pTargetHouse : HouseClass::Array)
		{
			if (EnumFunctions::CanTargetHouse(this->LimboKill_Affected, pHouse, pTargetHouse))
			{
				auto const pHouseExt = HouseExt::ExtMap.Find(pTargetHouse);
				if (!pHouseExt) { continue; }

				auto& vec = pHouseExt->OwnedLimboDeliveredBuildings;

				for (auto it = vec.begin(); it != vec.end(); )
				{
					BuildingClass* const pBuilding = *it;
					auto const pBuildingExt = BuildingExt::ExtMap.Find(pBuilding);

					if (pBuildingExt->LimboID == limboKillID)
					{
						it = vec.erase(it);
						auto const pBldType = pBuilding->Type;

						// Remove limbo buildings' tracking here because they are not truly InLimbo
						if (!pBldType->Insignificant && !pBldType->DontScore)
						{
							if (auto* ownerExt = HouseExt::ExtMap.Find(pBuilding->Owner))
							{
								ownerExt->RemoveFromLimboTracking(pBldType);
							}
						}

						pBuilding->Stun();
						pBuilding->Limbo();
						pBuilding->RegisterDestruction(nullptr);
						pBuilding->UnInit();
					}
					else
					{
						++it;
					}
				}
			}
		}
	}
}
#pragma endregion

void SWTypeExt::ExtData::ApplyDetonation(HouseClass* pHouse, const CellStruct& cell)
{
	if (!pHouse) { return; }

	// Validate input cell first
	if (!MapClass::Instance.CoordinatesLegal(cell))
	{
		const auto pWeapon = this->Detonate_Weapon;
		const auto* id = pWeapon ? pWeapon->get_ID()
			: (this->Detonate_Warhead ? this->Detonate_Warhead->get_ID() : "NULL-WH");
		Debug::Log("ApplyDetonation: Superweapon [%s] failed to detonate [%s] - invalid cell %d,%d.\n",
			this->OwnerObject()->get_ID(), id, cell.X, cell.Y);
		return;
	}

	// Now safe to fetch coords
	auto* const pCell = MapClass::Instance.GetCellAt(cell);
	if (!pCell) { return; }

	auto coords = pCell->GetCoords();
	BuildingClass* pFirer = nullptr;

	for (auto const& pBld : pHouse->Buildings)
	{
		if (this->IsLaunchSiteEligible(cell, pBld, /*ignoreRange*/false))
		{
			pFirer = pBld;
			break;
		}
	}

	if (this->Detonate_AtFirer)
		coords = pFirer ? pFirer->GetCenterCoords() : CoordStruct::Empty;

	// ✅ Final legality check after all overrides
	const auto mapCoords = CellClass::Coord2Cell(coords);
	if (!MapClass::Instance.CoordinatesLegal(mapCoords))
	{
		const auto pWeapon = this->Detonate_Weapon;
		const auto* id = pWeapon ? pWeapon->get_ID()
			: (this->Detonate_Warhead ? this->Detonate_Warhead->get_ID() : "NULL-WH");
		Debug::Log("ApplyDetonation: Superweapon [%s] failed to detonate [%s] - cell at %d, %d is invalid.\n",
			this->OwnerObject()->get_ID(), id, mapCoords.X, mapCoords.Y);
		return;
	}

	const auto pWeapon = this->Detonate_Weapon;

	if (pWeapon)
	{
		WeaponTypeExt::DetonateAt(pWeapon, coords, pFirer, this->Detonate_Damage.Get(pWeapon->Damage), pHouse);
	}
	else
	{
		if (this->Detonate_Warhead_Full)
			WarheadTypeExt::DetonateAt(this->Detonate_Warhead, coords, pFirer, this->Detonate_Damage.Get(0), pHouse);
		else
			MapClass::DamageArea(coords, this->Detonate_Damage.Get(0), pFirer, this->Detonate_Warhead, true, pHouse);
	}
}

void SWTypeExt::ExtData::ApplySWNext(SuperClass* pSW, const CellStruct& cell)
{
	if (!pSW || !pSW->Owner) { return; }

	// SW.Next proper launching mechanic
	auto LaunchTheSW = [=](const int swIdxToLaunch)
		{
			auto* const pHouse = pSW->Owner;
			if (!pHouse) { return; }

			if (const auto pSuper = pHouse->Supers.GetItem(swIdxToLaunch))
			{
				const auto* pNextTypeExt = SWTypeExt::ExtMap.Find(pSuper->Type);
				if (!pNextTypeExt) { return; }

				auto* pHouseExt = HouseExt::ExtMap.Find(pHouse);

				// ✅ Deduped + hoisted predicate:
				const bool canLaunch =
					!this->SW_Next_RealLaunch ||
					(pSuper->IsPresent && pSuper->IsReady && !pSuper->IsSuspended &&
					 pHouse->CanTransactMoney(pNextTypeExt->Money_Amount) &&
					 (pNextTypeExt->BattlePoints_Amount == 0 || (pHouseExt && pHouseExt->CanTransactBattlePoints(pNextTypeExt->BattlePoints_Amount))) &&
					 (pNextTypeExt->CommanderPoints_Amount == 0 || (pHouseExt && pHouseExt->CanTransactCommanderPoints(pNextTypeExt->CommanderPoints_Amount))));

				if (canLaunch)
				{
					if ((this->SW_Next_IgnoreInhibitors || !pNextTypeExt->HasInhibitor(pHouse, cell)) &&
						(this->SW_Next_IgnoreDesignators || pNextTypeExt->HasDesignator(pHouse, cell)))
					{
						const int oldstart = pSuper->RechargeTimer.StartTime;
						const int oldleft = pSuper->RechargeTimer.TimeLeft;

						pSuper->SetReadiness(true);
						pSuper->Launch(cell, pHouse->IsCurrentPlayer());
						pSuper->Reset();

						// Preserve timers if not a "real" launch
						if (!this->SW_Next_RealLaunch)
						{
							pSuper->RechargeTimer.StartTime = oldstart;
							pSuper->RechargeTimer.TimeLeft = oldleft;
						}
					}
				}
			}
		};

	// random mode
	if (!this->SW_Next_RandomWeightsData.empty())
	{
		const auto results = this->WeightedRollsHandler(
			&this->SW_Next_RollChances, &this->SW_Next_RandomWeightsData,
			this->SW_Next.size());

		for (const int result : results)
		{
			LaunchTheSW(this->SW_Next[result]);
		}
	}
	// no randomness mode
	else
	{
		for (const auto swType : this->SW_Next)
		{
			LaunchTheSW(swType);
		}
	}
}

void SWTypeExt::ExtData::ApplyTypeConversion(SuperClass* pSW, const CellStruct& cell)
{
	if (!pSW || !pSW->Owner) { return; }

	if (this->Convert_UseSWRange)
	{
		// Range-based conversion: only affect units within Range of target cell
		const float rangeCells = pSW->Type->Range;
		const float rangeLeptons = rangeCells * Unsorted::LeptonsPerCell;
		const float rangeSquared = rangeLeptons * rangeLeptons;
		const CoordStruct targetCoords = CellClass::Cell2Coord(cell);

		for (const auto pTechno : TechnoClass::Array)
		{
			auto pTargetFoot = abstract_cast<FootClass*>(pTechno);
			if (!pTargetFoot)
				continue;

			// Skip dead/dying units but allow passengers (InLimbo with Transporter)
			if (pTargetFoot->Health <= 0 || !pTargetFoot->IsAlive || pTargetFoot->IsCrashing || pTargetFoot->IsSinking)
				continue;

			// Skip units in limbo that are NOT passengers
			if (pTargetFoot->InLimbo && !pTargetFoot->Transporter)
				continue;

			// For passengers, use the transporter's location for distance calculation
			const CoordStruct unitCoords = pTargetFoot->InLimbo && pTargetFoot->Transporter
				? pTargetFoot->Transporter->GetCoords()
				: pTargetFoot->GetCoords();
			const float distanceSquared = (float)(targetCoords - unitCoords).MagnitudeSquared();

			if (distanceSquared <= rangeSquared)
				TypeConvertGroup::Convert(pTargetFoot, this->Convert_Pairs, pSW->Owner, this->ConvertAnim.Get());
		}
	}
	else
	{
		// Global conversion: affect all units (original behavior)
		for (const auto pTechno : TechnoClass::Array)
		{
			auto pTargetFoot = abstract_cast<FootClass*>(pTechno);
			if (!pTargetFoot)
				continue;

			// Skip dead/dying units but allow passengers (InLimbo with Transporter)
			if (pTargetFoot->Health <= 0 || !pTargetFoot->IsAlive || pTargetFoot->IsCrashing || pTargetFoot->IsSinking)
				continue;

			TypeConvertGroup::Convert(pTargetFoot, this->Convert_Pairs, pSW->Owner, this->ConvertAnim.Get());
		}
	}
}

void SWTypeExt::ExtData::HandleEMPulseLaunch(SuperClass* pSW, const CellStruct& cell) const
{
	if (!pSW || !pSW->Owner) { return; }

	auto const& pBuildings = this->GetEMPulseCannons(pSW->Owner, cell);
	auto const count = this->SW_MaxCount >= 0 ? static_cast<size_t>(this->SW_MaxCount) : std::numeric_limits<size_t>::max();

	for (size_t i = 0; i < pBuildings.size(); i++)
	{
		auto const pBuilding = pBuildings[i];
		if (!pBuilding) { continue; }

		auto const pExt = BuildingExt::ExtMap.Find(pBuilding);
		if (pExt)
		{
			pExt->EMPulseSW = pSW;
		}

		if (i + 1 == count)
			break;
	}

	if (this->EMPulse_SuspendOthers)
	{
		auto const pHouse = pSW->Owner;
		auto const pHouseExt = HouseExt::ExtMap.Find(pHouse);

		for (auto const& pSuper : pHouse->Supers)
		{
			if (!pSuper) { continue; }
			if (static_cast<int>(pSuper->Type->Type) != 28 || pSuper == pSW)
				continue;

			auto const pTypeExt = SWTypeExt::ExtMap.Find(pSW->Type);
			if (!pTypeExt) { continue; }

			bool suspend = false;

			if (this->EMPulse_Cannons.empty() && pTypeExt->EMPulse_Cannons.empty())
			{
				suspend = true;
			}
			else
			{
				// Suspend if the two cannon lists share common items.
				suspend = std::find_first_of(this->EMPulse_Cannons.begin(), this->EMPulse_Cannons.end(),
					pTypeExt->EMPulse_Cannons.begin(), pTypeExt->EMPulse_Cannons.end()) != this->EMPulse_Cannons.end();
			}

			if (suspend)
			{
				pSuper->IsSuspended = true;
				const int arrayIndex = pSW->Type->ArrayIndex;

				if (pHouseExt && pHouseExt->SuspendedEMPulseSWs.count(arrayIndex))
					pHouseExt->SuspendedEMPulseSWs[arrayIndex].push_back(arrayIndex);
				else if (pHouseExt)
					pHouseExt->SuspendedEMPulseSWs.insert({ arrayIndex, std::vector<int>{arrayIndex} });
			}
		}
	}
}

void SWTypeExt::ExtData::ApplyLinkedSW(SuperClass* pSW, const CellStruct& /*cell*/)
{
	if (!pSW || !pSW->Owner) { return; }

	const auto pHouse = pSW->Owner;
	const bool notObserver = !pHouse->IsObserver() || !pHouse->IsCurrentPlayerObserver();

	if (pHouse->Defeated || !notObserver)
		return;

	// ✅ v1 behavior: if Grant=no, don't process any linked SWs at all
	if (!this->SW_Link_Grant)
		return;

	auto linkedSW = [=](const int swIdxToAdd) -> bool
		{
			if (const auto pSuper = pHouse->Supers.GetItem(swIdxToAdd))
			{
				const bool granted = !pSuper->IsPresent && pSuper->Grant(true, false, false);
				bool isActive = granted;

				if (pSuper->IsPresent)
				{
					if (this->SW_Link_Reset)
					{
						pSuper->Reset();
						isActive = true;
					}
					else if (this->SW_Link_Ready || (granted && (SWTypeExt::ExtMap.Find(pSuper->Type)
						? SWTypeExt::ExtMap.Find(pSuper->Type)->SW_InitialReady
						: false)))
					{
						pSuper->RechargeTimer.TimeLeft = 0;
						pSuper->SetReadiness(true);
						isActive = true;
					}
					else if (granted)
					{
						pSuper->Reset();
					}
				}

				if (granted && notObserver && pHouse->IsCurrentPlayer())
				{
					// only if linked type wants a cameo
					if (const auto linkedExt = SWTypeExt::ExtMap.Find(pSuper->Type))
					{
						if (linkedExt->SW_ShowCameo)
						{
							UISafeOps::EnqueueAddCameo(swIdxToAdd);
						}
					}
				}
				return isActive;
			}
			return false;
		};

	bool anyActive = false;
	if (!this->SW_Link_RandomWeightsData.empty())
	{
		const auto results = this->WeightedRollsHandler(&this->SW_Link_RollChances, &this->SW_Link_RandomWeightsData, this->SW_Link.size());
		for (const int result : results)
		{
			if (linkedSW(this->SW_Link[result])) anyActive = true;
		}
	}
	else
	{
		for (const auto swType : this->SW_Link)
		{
			if (linkedSW(swType)) anyActive = true;
		}
	}

	if (anyActive && notObserver && pHouse->IsCurrentPlayer())
	{
		if (this->EVA_LinkedSWAcquired.isset())
			VoxClass::PlayIndex(this->EVA_LinkedSWAcquired.Get(), -1, -1);

		MessageListClass::Instance.PrintMessage(
			this->Message_LinkedSWAcquired.Get(),
			RulesClass::Instance->MessageDelay,
			HouseClass::CurrentPlayer->ColorSchemeIndex, true);
	}
}

// --- REPLACE WHOLE METHOD ---
void SWTypeExt::ExtData::ApplyBattlePoints(SuperClass* pSW)
{
	if (!pSW || !pSW->Owner) { return; }
	if (this->BattlePoints_Amount == 0) { return; }

	const int frame = Unsorted::CurrentFrame; // global game frame
	const int hIdx = pSW->Owner->ArrayIndex;
	const int swIdx = (pSW->Type ? pSW->Type->ArrayIndex : -1);
	if (swIdx < 0) { return; }

	struct Key { int frame, h, sw; };
	static std::vector<Key> recent;      // small, per-process cache
	// drop anything older than 2 frames (keeps vector tiny and deterministic)
	while (!recent.empty() && recent.front().frame < frame - 2)
		recent.erase(recent.begin());

	const auto dupe = std::find_if(recent.begin(), recent.end(),
		[&](const Key& k) { return k.frame == frame && k.h == hIdx && k.sw == swIdx; });

	if (dupe != recent.end()) { return; } // already applied this frame
	recent.push_back({ frame, hIdx, swIdx });

	if (auto* const pOwnerExt = HouseExt::ExtMap.Find(pSW->Owner))
	{
		const int before = pOwnerExt->BattlePoints;
		pOwnerExt->UpdateBattlePoints(this->BattlePoints_Amount);
		if (pOwnerExt->BattlePoints != before)
		{
			// cause a single, synced tech/availability pass
			pSW->Owner->RecheckTechTree = true;
		}
	}
}

// --- REPLACE WHOLE METHOD ---
void SWTypeExt::ExtData::ApplyCommanderPoints(SuperClass* pSW)
{
	if (!pSW || !pSW->Owner) { return; }
	if (this->CommanderPoints_Amount == 0) { return; }

	const int frame = Unsorted::CurrentFrame;
	const int hIdx = pSW->Owner->ArrayIndex;
	const int swIdx = (pSW->Type ? pSW->Type->ArrayIndex : -1);
	if (swIdx < 0) { return; }

	struct Key { int frame, h, sw; };
	static std::vector<Key> recent;
	while (!recent.empty() && recent.front().frame < frame - 2)
		recent.erase(recent.begin());

	const auto dupe = std::find_if(recent.begin(), recent.end(),
		[&](const Key& k) { return k.frame == frame && k.h == hIdx && k.sw == swIdx; });

	if (dupe != recent.end()) { return; }
	recent.push_back({ frame, hIdx, swIdx });

	if (auto* const pOwnerExt = HouseExt::ExtMap.Find(pSW->Owner))
	{
		const int before = pOwnerExt->CommanderPoints;
		pOwnerExt->UpdateCommanderPoints(this->CommanderPoints_Amount);
		if (pOwnerExt->CommanderPoints != before)
		{
			pSW->Owner->RecheckTechTree = true;
		}
	}
}
