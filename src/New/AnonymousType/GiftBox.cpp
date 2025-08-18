#include "GiftBox.h"
#include "GiftBoxData.h"
#include <Ext/Techno/Body.h>
#include <Ext/TechnoType/Body.h>
#include <InfantryClass.h>
#include <UnitClass.h>
#include <AircraftClass.h>
#include <BuildingClass.h>
#include <MapClass.h>
#include <RulesClass.h>

// ===== Helpers replicated from Otamaa's approach =====
static inline bool BingoChance(double chance) {
    if (chance <= 0.0) { return false; }
    if (chance >= 1.0) { return true; }
    return ScenarioClass::Instance->Random.RandomDouble() < chance;
}

static inline bool BingoIndex(const ValueableVector<double>& chances, int index) {
    if (chances.empty() || index < 0 || (size_t)index >= chances.size()) {
        return true;
    }
    return BingoChance(chances[index]);
}

static void GetGifts(GiftBoxData& nData, std::vector<TechnoTypeClass*>& out, const char* pSection)
{
    Debug::Log("GetGifts: Called for section '%s', Enable=%d\n", pSection, nData.Enable.Get());
    
    // Get fresh data from INI every time - no cached state
    std::vector<TechnoTypeClass*> gifts;
    std::vector<int> nums;
    nData.GetGiftsFromINI(pSection, gifts, nums);
    
    const size_t giftCount = gifts.size();
    Debug::Log("GetGifts: Got %zu gifts from INI\n", giftCount);
    if (!giftCount) { return; }
    
    // Defensive bounds checking to prevent memory disasters
    constexpr int MAX_SAFE_GIFTS = 1000;  // Reasonable upper bound
    constexpr int MAX_SAFE_TIMES = 100;   // Reasonable upper bound per gift type
    constexpr int MAX_SAFE_OUTPUT = 10000;  // Maximum output vector size

    // Safety check for gift count
    if (giftCount > MAX_SAFE_GIFTS) {
        Debug::Log("GetGifts: Gift count %zu exceeds safe limit %d, aborting\n", giftCount, MAX_SAFE_GIFTS);
        return;
    }

    if (nData.UseChancesAndWeight.Get()) {
        const int numsCount = (int)nums.size();

        if (nData.RandomType.Get()) {
            // number of rolls is sum(Nums) or 1
            int times = 1;
            if (numsCount > 0) {
                times = 0;
                for (auto n : nums) { 
                    // Safety check for individual Nums values
                    if (n < 0 || n > MAX_SAFE_TIMES) {
                        Debug::Log("GetGifts: Invalid Nums value %d, clamping to safe range\n", n);
                        n = std::max(0, std::min(n, MAX_SAFE_TIMES));
                    }
                    times += n;
                    // Safety check for cumulative times
                    if (times > MAX_SAFE_TIMES) {
                        Debug::Log("GetGifts: Cumulative times %d exceeds safe limit, capping at %d\n", times, MAX_SAFE_TIMES);
                        times = MAX_SAFE_TIMES;
                        break;
                    }
                }
            }

            // build weighted ranges
            const size_t weightCount = nData.RandomWeights.size();
            std::vector<std::pair<std::pair<int,int>, int>> ranges;
            int acc = 0;
            for (size_t i = 0; i < giftCount; ++i) {
                int w = 1;
                if (weightCount > 0 && i < weightCount) {
                    int wset = nData.RandomWeights[i];
                    if (wset > 0) { w = wset; }
                }
                auto lo = acc;
                acc += w;
                auto hi = acc;
                ranges.emplace_back(std::make_pair(lo, hi), (int)i);
            }

            for (int i = 0; i < times; ++i) {
                // Safety check for output vector size
                if (out.size() >= MAX_SAFE_OUTPUT) {
                    Debug::Log("GetGifts: Output vector size %zu reached safe limit, stopping\n", out.size());
                    break;
                }
                
                const int p = ScenarioClass::Instance->Random.Random() % (acc == 0 ? 1 : acc);
                int idx = 0;
                for (auto const& it : ranges) {
                    auto pr = it.first;
                    if (p >= pr.first && p < pr.second) { idx = it.second; break; }
                }
                if (BingoIndex(nData.Chances, idx)) {
                    // Safety check for idx bounds
                    if (idx >= 0 && (size_t)idx < gifts.size()) {
                        out.push_back(gifts[idx]);
                    } else {
                        Debug::Log("GetGifts: Invalid gift index %d, skipping\n", idx);
                    }
                }
            }
        } else {
            // not random-type: iterate each, repeat by Nums, pass Bingo per index
            for (size_t i = 0; i < giftCount; ++i) {
                int times = 1;
                if (numsCount > 0 && (int)i < numsCount) { 
                    times = nums[i];
                    // Safety check for times value
                    if (times < 0 || times > MAX_SAFE_TIMES) {
                        Debug::Log("GetGifts: Invalid times value %d for gift %zu, clamping to safe range\n", times, i);
                        times = std::max(0, std::min(times, MAX_SAFE_TIMES));
                    }
                }
                for (int t = 0; t < times; ++t) {
                    // Safety check for output vector size
                    if (out.size() >= MAX_SAFE_OUTPUT) {
                        Debug::Log("GetGifts: Output vector size %zu reached safe limit, stopping\n", out.size());
                        return;
                    }
                    
                    if (BingoIndex(nData.Chances, (int)i)) {
                        out.push_back(gifts[i]);
                    }
                }
            }
        }
    } else {
        // legacy simple path
        if (nData.RandomType) {
            const size_t idx = ScenarioClass::Instance->Random.Random() % giftCount;
            int times = (int)((idx < nums.size()) ? nums[idx] : 1);
            // Safety check for times value
            if (times < 0 || times > MAX_SAFE_TIMES) {
                Debug::Log("GetGifts: Invalid times value %d in legacy random path, clamping to safe range\n", times);
                times = std::max(0, std::min(times, MAX_SAFE_TIMES));
            }
            for (int t = 0; t < times; ++t) {
                // Safety check for output vector size
                if (out.size() >= MAX_SAFE_OUTPUT) {
                    Debug::Log("GetGifts: Output vector size %zu reached safe limit, stopping\n", out.size());
                    break;
                }
                out.push_back(gifts[idx]);
            }
        } else {
            for (size_t i = 0; i < giftCount; ++i) {
                int times = (int)((i < nums.size()) ? nums[i] : 1);
                // Safety check for times value
                if (times < 0 || times > MAX_SAFE_TIMES) {
                    Debug::Log("GetGifts: Invalid times value %d for gift %zu in legacy path, clamping to safe range\n", times, i);
                    times = std::max(0, std::min(times, MAX_SAFE_TIMES));
                }
                for (int t = 0; t < times; ++t) {
                    // Safety check for output vector size
                    if (out.size() >= MAX_SAFE_OUTPUT) {
                        Debug::Log("GetGifts: Output vector size %zu reached safe limit, stopping\n", out.size());
                        return;
                    }
                    out.push_back(gifts[i]);
                }
            }
        }
    }
}

// Helpers: spawn & place a techno safely
static TechnoClass* CreateTechno(TechnoTypeClass* pType, HouseClass* pHouse) {
    if(!pType || !pHouse) { return nullptr; }
    switch (pType->WhatAmI()) {
        case AbstractType::InfantryType: return GameCreate<InfantryClass>((InfantryTypeClass*)pType, pHouse);
        case AbstractType::UnitType:     return GameCreate<UnitClass>((UnitTypeClass*)pType, pHouse);
        case AbstractType::AircraftType: return GameCreate<AircraftClass>((AircraftTypeClass*)pType, pHouse);
        case AbstractType::BuildingType: return GameCreate<BuildingClass>((BuildingTypeClass*)pType, pHouse);
        default: return nullptr;
    }
}

static bool FindClearCellNear(const CoordStruct& at, CellClass*& outCell, CoordStruct& outCoords, int maxRange = 3) {
    outCell = nullptr;
    outCoords = at;
    for (int range = 0; range <= maxRange; ++range) {
        for (int dx = -range; dx <= range; ++dx) {
            for (int dy = -range; dy <= range; ++dy) {
                if (range==0 && dx==0 && dy==0) continue;
                auto base = CellClass::Coord2Cell(at);
                CellStruct ofs = { (short)dx, (short)dy };
                if (auto c = MapClass::Instance.TryGetCellAt(base + ofs)) {
                    if (!c->GetContent() && c->IsClearToMove(SpeedType::Foot, false, false, -1, MovementZone::Normal, -1, false)) {
                        outCell = c;
                        outCoords = c->GetCoordsWithBridge();
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

void GiftBox::Release(TechnoClass* pOwner, GiftBoxData& nData)
{
    if(!pOwner) { return; }

    auto pHouse = pOwner->GetOwningHouse();
    auto location = pOwner->GetCoords();

    Debug::Log("GiftBox::Release called for %s at %d,%d,%d\n", pOwner->GetTechnoType()->ID, location.X, location.Y, location.Z);

    // inherit destination/target from owner
    AbstractClass* pDest = nullptr;
    AbstractClass* pFocus = nullptr;
    if (auto pFoot = abstract_cast<FootClass*>(pOwner)) {
        pDest  = pFoot->Destination;
        pFocus = pOwner->ArchiveTarget; // rally
    } else {
        pFocus = pOwner->ArchiveTarget; // buildings use rally target
    }

    std::vector<TechnoTypeClass*> gifts;
    const char* pSection = pOwner->GetTechnoType()->ID;
    GetGifts(nData, gifts, pSection);
    Debug::Log("GiftBox: GetGifts returned %d gifts\n", (int)gifts.size());

    for (auto* pTech : gifts) {
        if(!pTech || !pHouse) { 
            Debug::Log("GiftBox: Skipping null tech (%p) or house (%p)\n", pTech, pHouse);
            continue; 
        }
        
        // Extra safety: Skip obviously invalid pointers
        if ((uintptr_t)pTech < 0x10000 || (uintptr_t)pTech > 0x7FFFFFFF) {
            Debug::Log("GiftBox: Skipping invalid tech pointer (%p)\n", pTech);
            continue;
        }
        
        // buildings need special placement -> skip
        if (pTech->WhatAmI() == AbstractType::BuildingType) { 
            Debug::Log("GiftBox: Skipping building type %s\n", pTech->ID);
            continue; 
        }

        Debug::Log("GiftBox: Creating gift %s\n", pTech->ID);
        TechnoClass* pGift = CreateTechno(pTech, pHouse);
        if(!pGift) { 
            Debug::Log("GiftBox: Failed to create gift %s\n", pTech->ID);
            continue; 
        }

        // pick spawn cell
        CoordStruct spawn = location;
        CellClass*  spawnCell = nullptr;
        FindClearCellNear(location, spawnCell, spawn);

        Debug::Log("GiftBox: Attempting to place gift %s at %d,%d,%d\n", pTech->ID, spawn.X, spawn.Y, spawn.Z);
        if(!pGift->Unlimbo(spawn, DirType::North)) {
            Debug::Log("GiftBox: Failed to place gift %s even in clear cell\n", pTech->ID);
            pGift->UnInit();
            continue;
        }
        Debug::Log("GiftBox: Successfully placed gift %s\n", pTech->ID);

        // house bookkeeping
        if (auto own = pGift->GetOwningHouse()) {
            if(!own->IsNeutral() && !pGift->GetTechnoType()->Insignificant) {
                own->RegisterGain(pGift, false);
                own->AddTracking(pGift);
                own->RecheckTechTree = true;
            }
        }

        if (pOwner->IsSelected) { pGift->Select(); }

        // movement / rally inheritance
        if (!pDest && !pFocus && pTech->Speed != 0) {
            pGift->Scatter(CoordStruct::Empty, true, false);
        } else if (auto pFootGift = abstract_cast<FootClass*>(pGift)) {
            if (pTech->Speed != 0) {
                CoordStruct des = pDest ? pDest->GetCoords() : spawn;
                if (pFocus) {
                    pGift->SetArchiveTarget(pFocus);
                    if (pGift->WhatAmI() != BuildingClass::AbsID) {
                        des = pFocus->GetCoords();
                    }
                }
                if (auto pDestCell = MapClass::Instance.TryGetCellAt(des)) {
                    pGift->SetDestination(pDestCell, true);
                    pGift->QueueMission(Mission::Move, false);
                    pFootGift->NextMission();
                }
            }
        }
    }
}
