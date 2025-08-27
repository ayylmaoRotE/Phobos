#include "BountyContracts.h"
#include "BountyRewards.h"
#include "BountyUI.h"

// Include your actual engine headers here:
// #include <YRpp/HouseClass.h>
// #include <YRpp/TechnoClass.h>
// #include <YRpp/TechnoTypeClass.h>
// #include <YRpp/BuildingClass.h>
// #include <YRpp/BuildingTypeClass.h>
// #include <YRpp/SuperWeaponTypeClass.h>
// #include <YRpp/ScenarioClass.h>
// #include <Utilities/Parser.h>
// #include <Ext/Scenario/Body.h>
// #include <Ext/Rules/Body.h>

using namespace Bounty;

// ---- Singleton ----
Manager& Manager::Instance() {
    static Manager g;
    return g;
}

const ContractDef* Manager::GetActiveContract() const {
    if (DeckIndex < 0 || DeckIndex >= (int)Deck.size()) { return nullptr; }
    int idx = Deck[DeckIndex];
    if (idx < 0 || idx >= (int)Contracts.size()) { return nullptr; }
    return &Contracts[idx];
}

// ---- Lifecycle ----

void Manager::OnRulesLoaded(INIClass* rules) {
    ParseRules(rules);
}

void Manager::OnGameStart(ScenarioClass* scen) {
    // Seed RNG deterministically
    // Prefer scenario seed if you expose one; fallback to house count + map UID patterns
    uint32_t seed = 0xB00B1Eu;
    // Example (replace with your actual seed source):
    // seed ^= (uint32_t)ScenarioClass::Instance()->Random.RandomSeed;
    // seed ^= (uint32_t)ScenarioClass::Instance()->UniqueID();
    RNG.Seed(seed);

    // Pre-roll deck order and per-contract reward selection
    PreRollDeckAndRewards(scen);

    // Frame cadence
    FramesPerRotation = (Cfg.RotationMinutes > 0 ? Cfg.RotationMinutes : 3) * 900; // 900 = 15*60

    // Build groups (lobby teams or allied clusters)
    BuildGroups();

    // Initialize progress and money snapshots
    ResetProgressForActive();
    for (int i = 0; i < MaxHouses; ++i) { LastMoney[i] = 0; }

    // Set first rotation point
    NextSwitchFrame = FramesPerRotation;
    DeckIndex = 0;
    if (Cfg.BannerOnRotate) { AnnounceRotate(); }
}

void Manager::Save(void* /*stream*/) {
    // Integrate with your ScenarioExt stream (binary). Example layout:
    // write(Cfg, DeckIndex, NextSwitchFrame, GroupProgress, GroupGoals, HouseToGroup, LastMoney, RNG.s, Deck order)
}

void Manager::Load(void* /*stream*/) {
    // Read back the same layout
}

// ---- Events ----

void Manager::OnFrame(int currentFrame) {
    if (!Cfg.Enabled) { return; }
    if (Deck.empty()) { return; }
    if (currentFrame >= NextSwitchFrame) {
        AdvanceDeck();
        NextSwitchFrame += FramesPerRotation;
        if (Cfg.BannerOnRotate) { AnnounceRotate(); }
    }

    if (Cfg.ShowTeamProgress) {
        PushProgressUI();
    }
}

void Manager::OnKill(TechnoClass* victim, TechnoClass* killer, HouseClass* killerHouse) {
    if (!Cfg.Enabled) { return; }
    const auto* c = GetActiveContract();
    if (!c) { return; }

    if (c->Kind != ContractKind::KILL && c->Kind != ContractKind::KILL_LIST) {
        return;
    }

    // Get victim type membership
    TechnoTypeClass* vtt = victim ? (TechnoTypeClass*)victim->GetTechnoType() : nullptr;
    if (!vtt) { return; }

    bool match = false;
    if (c->Kind == ContractKind::KILL) {
        // Tier-based
        auto it = TechnoTypeToTier.find((int)(uintptr_t)vtt);
        match = (it != TechnoTypeToTier.end()) && (it->second == c->Tier);
    } else {
        // Explicit list
        for (auto* t : c->TypeList) { if (t == vtt) { match = true; break; } }
    }

    if (!match) { return; }

    // Which group gains progress?
    int g = -1;
    if (Cfg.ProgressSharedWithAllies) {
        g = killerHouse ? GetGroupForHouse(killerHouse) : -1;
    } else {
        g = killerHouse ? (int)killerHouse->ArrayIndex : -1;
    }
    if (g < 0 || g >= MaxGroups) { return; }

    GroupProgress[g] += 1;

    // Completed?
    TryCompleteByGroup(g, killerHouse);
}

void Manager::OnMoneyTick() {
    if (!Cfg.Enabled) { return; }
    const auto* c = GetActiveContract();
    if (!c || c->Kind != ContractKind::INCOME) { return; }

    // Iterate houses and sum positive deltas
    // (Replace with your engine's house iteration)
    for (int h = 0; h < MaxHouses; ++h) {
        int moneyNow = 0; // TODO: read from HouseClass by index h
        int delta = moneyNow - LastMoney[h];
        LastMoney[h] = moneyNow;
        if (delta > 0) {
            int g = Cfg.ProgressSharedWithAllies ? HouseToGroup[h] : h;
            if (g >= 0 && g < MaxGroups) {
                GroupProgress[g] += delta;
                TryCompleteByGroup(g, nullptr);
            }
        }
    }
}

void Manager::OnBuildingCompleted(BuildingClass* b) {
    if (!Cfg.Enabled || !b) { return; }
    const auto* c = GetActiveContract();
    if (!c) { return; }

    HouseClass* owner = b->Owner; // replace with actual field

    auto checkAndBump = [&](bool predicate) {
        if (!predicate) { return; }
        int g = Cfg.ProgressSharedWithAllies ? GetGroupForHouse(owner) : (int)owner->ArrayIndex;
        if (g < 0 || g >= MaxGroups) { return; }
        GroupProgress[g] += 1;
        TryCompleteByGroup(g, owner);
    };

    if (c->Kind == ContractKind::BUILD_TYPE) {
        checkAndBump(IsBuildingInList((BuildingTypeClass*)b->Type, *c));
    } else if (c->Kind == ContractKind::FIRST_TO_BUILD_TYPE) {
        // Only first group to perform the action should complete
        if (IsBuildingInList((BuildingTypeClass*)b->Type, *c)) {
            int g = Cfg.ProgressSharedWithAllies ? GetGroupForHouse(owner) : (int)owner->ArrayIndex;
            if (g >= 0 && g < MaxGroups && GroupProgress[g] < c->Goal) {
                GroupProgress[g] = c->Goal; // complete immediately
                TryCompleteByGroup(g, owner);
            }
        }
    } else if (c->Kind == ContractKind::BUILD_SUPER) {
        // Building that provides SW
        bool hasSW = false; // query your BuildingTypeClass fields (SuperWeapon / SuperWeapon2 != -1)
        checkAndBump(hasSW && IsBuildingInList((BuildingTypeClass*)b->Type, *c));
    }
}

void Manager::OnInfiltrate(BuildingClass* victim, FootClass* intruder) {
    if (!Cfg.Enabled) { return; }
    const auto* c = GetActiveContract();
    if (!c || c->Kind != ContractKind::INFILTRATE) { return; }

    HouseClass* atk = intruder ? intruder->Owner : nullptr;
    HouseClass* def = victim ? victim->Owner : nullptr;
    if (!atk || !def) { return; }
    if (atk == def) { return; }
    if (/* def->IsAlliedWith(atk) */ false) { return; } // replace with real call

    int g = Cfg.ProgressSharedWithAllies ? GetGroupForHouse(atk) : (int)atk->ArrayIndex;
    if (g < 0 || g >= MaxGroups) { return; }
    GroupProgress[g] += 1;
    TryCompleteByGroup(g, atk);
}

// ---- UI helpers ----

void Manager::AnnounceRotate() {
    const auto* c = GetActiveContract();
    if (!c) { return; }
    UI::Banner_AnnounceRotate(*c);
}

void Manager::PushProgressUI() {
    const auto* c = GetActiveContract();
    if (!c) { return; }
    UI::Ticker_UpdateProgress(*c, GroupProgress.data(), GroupGoals.data(), HouseToGroup.data(), GroupCount);
}

// ---- Internal ----

void Manager::BuildGroups() {
    // Default behavior: one group per house
    GroupCount = MaxHouses;
    for (int i = 0; i < MaxHouses; ++i) {
        HouseToGroup[i] = i;
    }

    // If lobby teams are available: remap HouseToGroup accordingly (deterministic)
    if (Cfg.UseLobbyTeamsIfAvailable) {
        // TODO: query MP team info and collapse to groups
    }

    // Else: optional allied-cluster collapse (union-find by IsAlliedWith)
    // TODO: compute allied clusters with your API and set HouseToGroup accordingly

    // Clamp GroupCount
    // TODO: recompute GroupCount = 1 + max(HouseToGroup)
}

void Manager::ResetProgressForActive() {
    std::fill(GroupProgress.begin(), GroupProgress.end(), 0);
    const auto* c = GetActiveContract();
    if (!c) { std::fill(GroupGoals.begin(), GroupGoals.end(), 0); return; }
    for (int i = 0; i < MaxGroups; ++i) {
        GroupGoals[i] = c->Goal;
    }
}

void Manager::AdvanceDeck() {
    if (Deck.empty()) { return; }
    DeckIndex = (DeckIndex + 1) % (int)Deck.size();
    ResetProgressForActive();
}

void Manager::TryCompleteByGroup(int groupIdx, HouseClass* instigator) {
    const auto* c = GetActiveContract();
    if (!c) { return; }
    if (GroupProgress[groupIdx] < GroupGoals[groupIdx]) { return; }

    // Winner groups receive reward (either shared with allies or per-house instigator)
    if (Cfg.ShareRewardWithAllies) {
        // Iterate houses in this group and grant
        for (int h = 0; h < MaxHouses; ++h) {
            if (HouseToGroup[h] == groupIdx) {
                Rewards::GrantReward(Rewards, c->RewardIndex, /*house=*/h);
            }
        }
    } else if (instigator) {
        Rewards::GrantReward(Rewards, c->RewardIndex, /*house=*/(int)instigator->ArrayIndex);
    }

    // Immediately move to the next contract
    AdvanceDeck();
    if (Cfg.BannerOnRotate) { AnnounceRotate(); }
}

bool Manager::IsTypeInCurrentMask(TechnoTypeClass* /*tt*/) const {
    // Implemented inline in OnKill for clarity. Keep this if you switch to bitmasks.
    return true;
}

bool Manager::IsBuildingInList(BuildingTypeClass* bt, const ContractDef& c) const {
    for (auto* t : c.TypeList) {
        if ((BuildingTypeClass*)t == bt) { return true; }
    }
    return false;
}

// ---- Parsing & preroll ----

void Manager::ParseRules(INIClass* rules) {
    // Read booleans/ints; replace with your parser calls
    // Cfg.Enabled = RulesExt::ReadBool(rules, "BountyContracts", "Enabled", true);
    // ... fill Cfg ...
    ParseTiers(rules);
    ParseRewards(rules);
    ParseContracts(rules);
}

void Manager::ParseTiers(INIClass* /*rules*/) {
    // Example: fill Tier1Types and TechnoTypeToTier map from [BountyContracts] Tier1Types=...
    // for (auto* tt : parsedList) { TechnoTypeToTier[(int)(uintptr_t)tt] = 1; }
}

void Manager::ParseRewards(INIClass* /*rules*/) {
    // Iterate Reward1..RewardN, push into Rewards vector
    // Fill Kind/Weight/data
}

void Manager::ParseContracts(INIClass* /*rules*/) {
    // Iterate Contract1..ContractN
    // Parse kind/goal, Tier or Types, BuildType, FirstTo flags, etc.
    // Push into Contracts with a DebugName
}

void Manager::PreRollDeckAndRewards(ScenarioClass* /*scen*/) {
    // Build deck indices 0..Contracts.size-1
    Deck.resize(Contracts.size());
    for (int i = 0; i < (int)Contracts.size(); ++i) { Deck[i] = i; }

    // Shuffle with deterministic RNG
    for (int i = (int)Deck.size() - 1; i > 0; --i) {
        int j = RNG.Range(0, i);
        std::swap(Deck[i], Deck[j]);
    }

    // Pick reward index for each contract (weighted)
    for (auto& c : Contracts) {
        int totalW = 0;
        for (auto& r : Rewards) totalW += (r.Weight > 0 ? r.Weight : 1);
        int pick = RNG.Range(1, totalW);
        int acc = 0, idx = 0;
        for (auto& r : Rewards) {
            acc += (r.Weight > 0 ? r.Weight : 1);
            if (pick <= acc) { c.RewardIndex = idx; break; }
            ++idx;
        }
        if (c.RewardIndex < 0 && !Rewards.empty()) { c.RewardIndex = 0; }
    }
}
