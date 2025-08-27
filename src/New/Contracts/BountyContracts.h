#pragma once
#include <vector>
#include <array>
#include <string>
#include <cstdint>
#include <unordered_map>

// Forward decls from your YRpp-style headers
class HouseClass;
class TechnoClass;
class TechnoTypeClass;
class BuildingClass;
class BuildingTypeClass;
class SuperWeaponTypeClass;
class FootClass;
class ScenarioClass;
class INIClass;

namespace Bounty {

enum class ContractKind : uint8_t {
    KILL = 0,
    KILL_LIST,
    INCOME,
    BUILD_TYPE,
    FIRST_TO_BUILD_TYPE,
    INFILTRATE,
    BUILD_SUPER
};

enum class RewardKind : uint8_t {
    MONEY = 0,
    SUPER_ONE_USE,
    SPAWN_UNIT,
    CRATE
};

struct RewardDef {
    RewardKind Kind = RewardKind::MONEY;
    int Weight = 1;
    int Money = 0;
    SuperWeaponTypeClass* SWType = nullptr;
    TechnoTypeClass* UnitType = nullptr;
    int UnitCount = 1;
    std::string CrateKind; // "Random" or custom token recognized by your crate system
    bool Announce = true;
};

struct ContractDef {
    ContractKind Kind = ContractKind::KILL;
    int Goal = 0;

    // KILL / KILL_LIST helpers
    int Tier = 0; // 0 if unused
    std::vector<TechnoTypeClass*> TypeList; // explicit list

    // BUILD_TYPE / FIRST_TO_BUILD_TYPE
    BuildingTypeClass* BuildType = nullptr;

    // BUILD_SUPER: also uses TypeList of buildings that provide SWs
    bool FirstTo = false; // only for FIRST_TO_BUILD_TYPE
    int RewardIndex = -1; // pre-rolled reward id into Rewards vector
    std::string DebugName; // optional label for UI
};

struct Settings {
    bool Enabled = true;
    int RotationMinutes = 3;
    bool BannerOnRotate = true;

    bool ShareRewardWithAllies = true;
    bool ProgressSharedWithAllies = true;

    bool ShowTeamProgress = true;
    bool UseLobbyTeamsIfAvailable = true;
};

// Simple deterministic LCG for pre-rolls (seeded from Scenario)
struct DRand {
    uint32_t s = 0xC0FFEEu;
    inline void Seed(uint32_t v) { s = v ? v : 0xC0FFEEu; }
    inline uint32_t Next() {
        s = 1664525u * s + 1013904223u;
        return s;
    }
    inline int Range(int lo, int hi) { // inclusive lo..hi
        const uint32_t r = Next() & 0x7FFFFFFFu;
        const uint32_t span = (uint32_t)(hi - lo + 1);
        return lo + (int)(r % (span ? span : 1));
    }
};

class Manager {
public:
    static Manager& Instance();

    // Lifecycle
    void OnRulesLoaded(INIClass* rules);
    void OnGameStart(ScenarioClass* scen);
    void Save(void* stream);  // call from ScenarioExt::SaveToStream
    void Load(void* stream);  // call from ScenarioExt::LoadFromStream

    // Events
    void OnFrame(int currentFrame);
    void OnKill(TechnoClass* victim, TechnoClass* killer, HouseClass* killerHouse);
    void OnMoneyTick(); // track positive deltas toward INCOME contracts
    void OnBuildingCompleted(BuildingClass* b);
    void OnInfiltrate(BuildingClass* victim, FootClass* intruder);

    // UI
    void AnnounceRotate();
    void PushProgressUI();

    // Helpers
    int GetGroupForHouse(HouseClass* h) const;
    int GetActiveContractIndex() const { return DeckIndex; }
    const ContractDef* GetActiveContract() const;
    const Settings& GetSettings() const { return Cfg; }

private:
    Settings Cfg{};
    DRand RNG{};

    // Pools
    std::vector<ContractDef> Contracts;
    std::vector<RewardDef> Rewards;

    // Deck & rotation
    std::vector<int> Deck; // indices into Contracts
    int DeckIndex = -1;
    int NextSwitchFrame = 0;
    int FramesPerRotation = 900 * 3; // default 3 minutes

    // Progress
    static constexpr int MaxHouses = 8; // adjust if your mod supports more
    static constexpr int MaxGroups = 8;
    std::array<int, MaxGroups> GroupProgress{};
    std::array<int, MaxGroups> GroupGoals{};
    std::array<int, MaxHouses> HouseToGroup{};
    int GroupCount = 0;

    // Income tracking
    std::array<int, MaxHouses> LastMoney{};

    // Tier masks
    std::unordered_map<int, int> TechnoTypeToTier; // Type->Tier
    std::vector<TechnoTypeClass*> Tier1Types;

    // Internal
    void BuildGroups();
    void ResetProgressForActive();
    void AdvanceDeck();
    void TryCompleteByGroup(int groupIdx, HouseClass* instigator);
    bool IsTypeInCurrentMask(TechnoTypeClass* tt) const;
    bool IsBuildingInList(BuildingTypeClass* bt, const ContractDef& c) const;

    // Parsing
    void ParseRules(INIClass* rules);
    void ParseContracts(INIClass* rules);
    void ParseRewards(INIClass* rules);
    void ParseTiers(INIClass* rules);
    void PreRollDeckAndRewards(ScenarioClass* scen);
};

} // namespace Bounty
