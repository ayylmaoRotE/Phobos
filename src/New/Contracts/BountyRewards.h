#pragma once
#include <vector>
#include <string>
#include <cstdint>

class HouseClass;
class TechnoTypeClass;
class SuperWeaponTypeClass;

#include "BountyContracts.h"

namespace Bounty { namespace Rewards {

// Grant reward by index from pre-parsed pool
void GrantReward(const std::vector<RewardDef>& pool, int rewardIndex, int houseIndex);

// Helpers (replace bodies with your engine calls)
void GiveMoney(HouseClass* h, int amount, bool floatingText);
void GrantOneTimeSuper(HouseClass* h, SuperWeaponTypeClass* swType);
void SpawnUnitsNearBase(HouseClass* h, TechnoTypeClass* unitType, int count);
void DropCrateAtBase(HouseClass* h, const std::string& crateKind);

}}
