#include "BountyRewards.h"
// #include <YRpp/HouseClass.h>
// #include <YRpp/SuperClass.h>
// #include <YRpp/TechnoTypeClass.h>
// #include <Misc/FlyingStrings.h>

using namespace Bounty;

namespace Bounty { namespace Rewards {

void GrantReward(const std::vector<RewardDef>& pool, int rewardIndex, int houseIndex) {
    if (rewardIndex < 0 || rewardIndex >= (int)pool.size()) { return; }
    const auto& r = pool[rewardIndex];

    // Translate house index to HouseClass*
    HouseClass* h = nullptr; // TODO: index -> house

    switch (r.Kind) {
        case RewardKind::MONEY:
            GiveMoney(h, r.Money, /*floatingText*/true);
            break;
        case RewardKind::SUPER_ONE_USE:
            GrantOneTimeSuper(h, r.SWType);
            break;
        case RewardKind::SPAWN_UNIT:
            SpawnUnitsNearBase(h, r.UnitType, r.UnitCount > 0 ? r.UnitCount : 1);
            break;
        case RewardKind::CRATE:
            DropCrateAtBase(h, r.CrateKind);
            break;
    }
}

void GiveMoney(HouseClass* /*h*/, int /*amount*/, bool /*floatingText*/) {
    // h->TransactMoney(amount);
    // FlyingStrings::AddMoneyString(amount, h, hqCoords, ...);
}

void GrantOneTimeSuper(HouseClass* /*h*/, SuperWeaponTypeClass* /*swType*/) {
    // SuperClass* sw = h->FindOrCreateSuper(swType);
    // sw->SetOneTime(true);
    // sw->Enable(true);
}

void SpawnUnitsNearBase(HouseClass* /*h*/, TechnoTypeClass* /*unitType*/, int /*count*/) {
    // Use your crate/giftbox helpers or a CreateUnit utility to spawn near ConYard / MCV rally cell
}

void DropCrateAtBase(HouseClass* /*h*/, const std::string& /*crateKind*/) {
    // Use your crate system to drop a specific or random crate near base center
}

}} // namespaces
