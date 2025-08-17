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

// Helper: collect the TechnoTypes to spawn from GiftBoxData.
// This version keeps it simple and avoids external DP helpers.
static void GetGifts(const GiftBoxData& nData, std::vector<TechnoTypeClass*>& nOut)
{
    const auto giftCount = nData.Gifts.size();

    // If RandomType is set: pick one entry, otherwise take all with their counts.
    if(nData.RandomType.Get())
    {
        if(giftCount == 0) { return; }
        const auto idx = ScenarioClass::Instance->Random.Random() % giftCount;
        const int count = (idx < (int)nData.Nums.size()) ? nData.Nums[idx] : 1;
        for(int i=0; i<count; ++i) {
            nOut.push_back(nData.Gifts[idx]);
        }
        return;
    }

    // All types with their Nums (fallback to 1 if missing)
    for(size_t i=0; i<giftCount; ++i)
    {
        const int count = (i < nData.Nums.size()) ? nData.Nums[i] : 1;
        for(int k=0; k<count; ++k) {
            nOut.push_back(nData.Gifts[i]);
        }
    }
}

void GiftBox::Release(TechnoClass* pOwner, GiftBoxData& nData)
{
    const auto pHouse = pOwner->GetOwningHouse();
    CoordStruct location = pOwner->GetCoords();

    Debug::Log("GiftBox::Release called for %s at %d,%d,%d\n",
        pOwner->GetTechnoType()->ID, location.X, location.Y, location.Z);

    // Pull orders from owner (Foot: Destination+ArchiveTarget, Building: rally in ArchiveTarget)
    AbstractClass* pDest  = nullptr;
    AbstractClass* pFocus = nullptr;

    if(auto pFootOwner = abstract_cast<FootClass*>(pOwner)) {
        pDest  = pFootOwner->Destination;
        pFocus = pOwner->ArchiveTarget;
    } else if(abstract_cast<BuildingClass*>(pOwner)) {
        pFocus = pOwner->ArchiveTarget; // rally holder for buildings
    }

    std::vector<TechnoTypeClass*> nOut;
    GetGifts(nData, nOut);
    Debug::Log("GiftBox: GetGifts returned %d gifts\n", nOut.size());

    for(auto const& pTech : nOut) {
        if(!pTech || !pHouse) {
            Debug::Log("GiftBox: Skipping null tech or house\n");
            continue;
        }

        TechnoClass* pGift = nullptr;
        switch(pTech->WhatAmI())
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
                // buildings require special placement logic; skip for now
                Debug::Log("GiftBox: Skipping building placement for %s\n", pTech->ID);
                continue;
            default:
                Debug::Log("GiftBox: Unknown type %d\n", pTech->WhatAmI());
                continue;
        }

        if(!pGift) {
            Debug::Log("GiftBox: Failed to create gift %s\n", pTech->ID);
            continue;
        }

        // Find a clear cell near the owner for safe placement
        CoordStruct giftLocation = location;
        CellClass* pChosenCell = nullptr;

        // Precompute owner cell
        const auto ownerCell = CellClass::Coord2Cell(location);

        // Try random offset if requested
        if(nData.RandomRange > 0) {
            CellStruct randomOffset = {
                static_cast<short>(ScenarioClass::Instance->Random.RandomRanged(-nData.RandomRange, nData.RandomRange)),
                static_cast<short>(ScenarioClass::Instance->Random.RandomRanged(-nData.RandomRange, nData.RandomRange))
            };
            if(auto pNew = MapClass::Instance.TryGetCellAt(ownerCell + randomOffset)) {
                if(!nData.EmptyCell || !pNew->GetContent()) {
                    pChosenCell = pNew;
                }
            }
        }

        // If none yet, scan a small area
        if(!pChosenCell) {
            for(int r=0; r<=3 && !pChosenCell; ++r) {
                for(int dx=-r; dx<=r && !pChosenCell; ++dx) {
                    for(int dy=-r; dy<=r && !pChosenCell; ++dy) {
                        if(r==0 && dx==0 && dy==0) { continue; }
                        CellStruct off { static_cast<short>(dx), static_cast<short>(dy) };
                        if(auto pNew = MapClass::Instance.TryGetCellAt(ownerCell + off)) {
                            if(!nData.EmptyCell || !pNew->GetContent()) {
                                if(pNew->IsClearToMove(SpeedType::Foot, false, false, -1, MovementZone::Normal, -1, false)) {
                                    pChosenCell = pNew;
                                }
                            }
                        }
                    }
                }
            }
        }

        if(pChosenCell) {
            giftLocation = pChosenCell->GetCoordsWithBridge();
            Debug::Log("GiftBox: Found clear cell for %s\n", pTech->ID);
        } else {
            Debug::Log("GiftBox: Using original location for %s\n", pTech->ID);
        }

        // Place the gift
        if(pGift->Unlimbo(giftLocation, static_cast<DirType>(0))) {
            Debug::Log("GiftBox: Successfully placed gift %s\n", pTech->ID);

            if(auto pOwnerHouse = pGift->GetOwningHouse()) {
                if(!pOwnerHouse->IsNeutral() && !pGift->GetTechnoType()->Insignificant) {
                    pOwnerHouse->RegisterGain(pGift, false);
                    pOwnerHouse->AddTracking(pGift);
                    pOwnerHouse->RecheckTechTree = true;
                }
            }

            if(pOwner->IsSelected) {
                pGift->Select();
            }

            // Orders inheritance (match Otamaa semantics; adapt for RotE APIs)
            if(!pDest && !pFocus && pTech->Speed != 0) {
                pGift->Scatter(CoordStruct::Empty, true, false);
                Debug::Log("GiftBox: No destination, scattering %s\n", pTech->ID);
            } else {
                if(auto pGiftFoot = abstract_cast<FootClass*>(pGift)) {
                    if(pTech->Speed != 0) {
                        CoordStruct des = giftLocation;
                        if(pDest) {
                            des = pDest->GetCoords();
                        }
                        if(pFocus) {
                            pGift->SetArchiveTarget(pFocus);
                            if(pGift->WhatAmI() != BuildingClass::AbsID) {
                                des = pFocus->GetCoords();
                            }
                            Debug::Log("GiftBox: Set archive target for %s\n", pTech->ID);
                        }
                        if(auto pDestCell = MapClass::Instance.TryGetCellAt(des)) {
                            pGiftFoot->SetDestination(pDestCell, true);
                            pGiftFoot->QueueMission(Mission::Move, false);
                            pGiftFoot->NextMission();
                            Debug::Log("GiftBox: Set destination, Move mission, and started for %s\n", pTech->ID);
                        } else {
                            Debug::Log("GiftBox: Failed to get target cell for %s\n", pTech->ID);
                        }
                    }
                } else {
                    Debug::Log("GiftBox: %s is not a FootClass, no movement commands\n", pTech->ID);
                }
            }
        } else {
            Debug::Log("GiftBox: Failed to place gift %s\n", pTech->ID);
            pGift->UnInit();
        }
    }
}
