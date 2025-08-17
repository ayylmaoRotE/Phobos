#include "GiftBox.h"
#include "GiftBoxData.h"
#include <TechnoClass.h>
#include <HouseClass.h>
#include <MapClass.h>
#include <ScenarioClass.h>
#include <CellClass.h>
#include <FootClass.h>
#include <BuildingClass.h>
#include <InfantryClass.h>
#include <InfantryTypeClass.h>
#include <UnitClass.h>
#include <UnitTypeClass.h>
#include <AircraftClass.h>
#include <AircraftTypeClass.h>
#include <BuildingTypeClass.h>
#include <Utilities/GeneralUtils.h>
#include <Utilities/Debug.h>

static void GetGifts(const GiftBoxData& nData, std::vector<TechnoTypeClass*>& nOut)
{
	const auto giftCount = nData.Gifts.size();
	Debug::Log("GetGifts: Found %d gift types\n", giftCount);

	if (nData.UseChancesAndWeight.Get())
	{
		int numsCount = nData.Nums.size();

		if (nData.RandomType.Get())
		{
			int times = 1;
			if (numsCount > 0)
			{
				times = 0;
				for (auto const& num : nData.Nums)
				{
					times += num;
				}
			}

			auto weightCount = nData.RandomWeights.size();
			std::vector<std::pair<std::pair<int, int>, int>> targetPad;
			int flag = 0;

			for (size_t index = 0; index < giftCount; index++)
			{
				int startRange = flag;
				int weight = 1;
				if (weightCount > 0 && index < weightCount)
				{
					int w = nData.RandomWeights[index];
					if (w > 0)
					{
						weight = w;
					}
				}
				flag += weight;
				int endRange = flag;
				targetPad.push_back(std::make_pair(std::make_pair(startRange, endRange), static_cast<int>(index)));
			}

			for (int i = 0; i < times; i++)
			{
				int index = 0;
				const int p = ScenarioClass::Instance->Random.RandomRanged(0, flag - 1);

				for (auto const& pair : targetPad)
				{
					auto const& range = pair.first;
					if (p >= range.first && p < range.second)
					{
						index = pair.second;
						break;
					}
				}

				// Check chance
				if (nData.Chances.size() > static_cast<size_t>(index))
				{
					double chance = nData.Chances[index];
					if (ScenarioClass::Instance->Random.RandomDouble() <= chance)
					{
						nOut.push_back(nData.Gifts[index]);
					}
				}
				else
				{
					nOut.push_back(nData.Gifts[index]);
				}
			}
		}
		else
		{
			for (size_t index = 0; index < giftCount; index++)
			{
				auto id = nData.Gifts[index];
				int times = 1;
				if (numsCount > 0 && index < (size_t)numsCount)
				{
					times = nData.Nums[index];
				}

				for (int i = 0; i < times; i++)
				{
					// Check chance
					if (nData.Chances.size() > static_cast<size_t>(index))
					{
						double chance = nData.Chances[index];
						if (ScenarioClass::Instance->Random.RandomDouble() <= chance)
						{
							nOut.push_back(id);
						}
					}
					else
					{
						nOut.push_back(id);
					}
				}
			}
		}
	}
	else
	{
		if (nData.RandomType)
		{
			auto const nIdx = ScenarioClass::Instance->Random.RandomRanged(0, giftCount - 1);
			for (int i = 0; i < nData.Nums[nIdx]; ++i)
				nOut.push_back(nData.Gifts[nIdx]);
		}
		else
		{
			for (size_t i = 0; i < (giftCount); ++i)
			{
				for (int a = 0; a < nData.Nums[i]; ++a)
				{
					nOut.push_back(nData.Gifts[i]);
				}
			}
		}
	}
}

void GiftBox::Release(TechnoClass* pOwner, GiftBoxData& nData)
{
	const auto pHouse = pOwner->GetOwningHouse();
	CoordStruct location = pOwner->GetCoords();

	Debug::Log("GiftBox::Release called for %s at %d,%d,%d\n", 
		pOwner->GetTechnoType()->ID, location.X, location.Y, location.Z);

	// Get original unit's destination and target for inheritance
	AbstractClass* pDest = nullptr;
	AbstractClass* pFocus = nullptr;
	Mission currentMission = Mission::Guard;
	
	if (auto pFoot = abstract_cast<FootClass*>(pOwner))
	{
		pDest = pFoot->Destination;
		pFocus = pOwner->Target;
		currentMission = pOwner->CurrentMission;
		Debug::Log("GiftBox: Original unit has destination=%p, target=%p, mission=%d\n", pDest, pFocus, currentMission);
	}

	std::vector<TechnoTypeClass*> nOut;
	GetGifts(nData, nOut);

	Debug::Log("GiftBox: GetGifts returned %d gifts\n", nOut.size());

	for (auto const& pTech : nOut) {
		if (!pTech || !pHouse) {
			Debug::Log("GiftBox: Skipping null tech or house\n");
			continue;
		}

		Debug::Log("GiftBox: Creating gift %s\n", pTech->ID);

		TechnoClass* pGift = nullptr;
		
		// Create the gift based on type - use GameCreate for safety
		switch (pTech->WhatAmI())
		{
		case AbstractType::InfantryType:
			pGift = GameCreate<InfantryClass>(static_cast<InfantryTypeClass*>(pTech), pHouse);
			break;
		case AbstractType::UnitType:
			pGift = GameCreate<UnitClass>(static_cast<UnitTypeClass*>(pTech), pHouse);
			break;
		case AbstractType::AircraftType:
			pGift = GameCreate<AircraftClass>(static_cast<AircraftTypeClass*>(pTech), pHouse);
			break;
		case AbstractType::BuildingType:
			pGift = GameCreate<BuildingClass>(static_cast<BuildingTypeClass*>(pTech), pHouse);
			break;
		default:
			Debug::Log("GiftBox: Unknown type %d\n", pTech->WhatAmI());
			continue;
		}

		if (pGift)
		{
			Debug::Log("GiftBox: Created gift %s, attempting to place\n", pTech->ID);
			
			// Find a clear location for placement
			CoordStruct giftLocation = location;
			CellClass* pTargetCell = nullptr;
			
			// For buildings, skip placement entirely (they need special handling)
			if (pTech->WhatAmI() == AbstractType::BuildingType)
			{
				Debug::Log("GiftBox: Skipping building placement for %s\n", pTech->ID);
				pGift->UnInit();
				continue;
			}
			
			// Try to find a clear cell nearby
			for (int range = 0; range <= 3; range++)
			{
				for (int x = -range; x <= range; x++)
				{
					for (int y = -range; y <= range; y++)
					{
						if (x == 0 && y == 0 && range == 0) continue; // Skip original location
						
						CellStruct offset = { static_cast<short>(x), static_cast<short>(y) };
						if (auto pCell = MapClass::Instance.TryGetCellAt(CellClass::Coord2Cell(location) + offset))
						{
							if (!pCell->GetContent() && pCell->IsClearToMove(SpeedType::Foot, false, false, -1, MovementZone::Normal, -1, false))
							{
								giftLocation = pCell->GetCoordsWithBridge();
								pTargetCell = pCell;
								goto found_cell;
							}
						}
					}
				}
			}
			
			found_cell:
			if (pTargetCell)
			{
				Debug::Log("GiftBox: Found clear cell for %s at offset\n", pTech->ID);
			}
			else
			{
				Debug::Log("GiftBox: Using original location for %s\n", pTech->ID);
			}
			
			// Place the gift
			if (pGift->Unlimbo(giftLocation, static_cast<DirType>(0)))
			{
				Debug::Log("GiftBox: Successfully placed gift %s\n", pTech->ID);
				
				if (auto pOwnerHouse = pGift->GetOwningHouse())
				{
					if (!pOwnerHouse->IsNeutral() && !pGift->GetTechnoType()->Insignificant)
					{
						pOwnerHouse->RegisterGain(pGift, false);
						pOwnerHouse->AddTracking(pGift);
						pOwnerHouse->RecheckTechTree = true;
					}
				}

				if (pOwner->IsSelected)
					pGift->Select();

				// Inherit commands from original unit - match Otamaa's exact implementation
				if (!pDest && !pFocus && pTech->Speed != 0)
				{
					pGift->Scatter(CoordStruct::Empty, true, false);
					Debug::Log("GiftBox: No destination, scattering %s\n", pTech->ID);
				}
				else
				{
					if (auto pGiftFoot = abstract_cast<FootClass*>(pGift))
					{
						if (pTech->Speed != 0)
						{
							CoordStruct des = pDest ? pDest->GetCoords() : giftLocation;

							if (pFocus)
							{
								pGift->SetArchiveTarget(pFocus);
								if (pGift->WhatAmI() != BuildingClass::AbsID)
								{
									des = pFocus->GetCoords();
								}
								Debug::Log("GiftBox: Set archive target for %s\n", pTech->ID);
							}

							if (auto pDestCell = MapClass::Instance.TryGetCellAt(des))
							{
								pGiftFoot->SetDestination(pDestCell, true);
								pGiftFoot->QueueMission(Mission::Move, false);
								// Force the unit to start moving immediately
								pGiftFoot->NextMission();
								
								// Additional debugging
								Debug::Log("GiftBox: Unit %s state: Mission=%d, InLimbo=%d, Health=%d, Speed=%d\n", 
									pTech->ID, pGift->CurrentMission, pGift->InLimbo, pGift->Health, pTech->Speed);
								Debug::Log("GiftBox: Destination set to cell at %d,%d,%d\n", 
									des.X, des.Y, des.Z);
								
								// Try alternative activation methods
								pGift->SetTarget(nullptr);
								pGift->EnterIdleMode(false, true);
								
								Debug::Log("GiftBox: Set destination, Move mission, and started for %s\n", pTech->ID);
							}
							else
							{
								Debug::Log("GiftBox: Failed to get target cell for %s\n", pTech->ID);
							}
						}
					}
					else
					{
						Debug::Log("GiftBox: %s is not a FootClass, no movement commands\n", pTech->ID);
					}
				}
			}
			else
			{
				Debug::Log("GiftBox: Failed to place gift %s even in clear cell\n", pTech->ID);
				// Failed to place, safely remove
				pGift->UnInit();
			}
		}
		else
		{
			Debug::Log("GiftBox: Failed to create gift %s\n", pTech->ID);
		}
	}
}