#pragma once
#include <vector>
#include <unordered_map>
#include <string>
#include <windows.h>   // COLORREF

#include <CCINIClass.h>
#include <ScenarioClass.h>
#include <RulesClass.h>
#include <Surface.h>
#include <Drawing.h>
#include <HouseClass.h>
#include <TechnoClass.h>
#include <BuildingClass.h>
#include <SuperClass.h>
#include <SuperWeaponTypeClass.h>
#include <TechnoTypeClass.h>
#include <GeneralDefinitions.h>


class MessageListClass; 

namespace Contracts
{

	enum class ContractKind : unsigned char { KillUnits, KillBuildings, FirstBuild, Infiltrate, EarnMoney };
	enum class RewardKind : unsigned char { Money, Unit, SuperWeapon };

	struct ContractDef
	{
		ContractKind kind;
		int required = 0;
		std::vector<TechnoTypeClass*> types;
		int  timerSeconds = 600;
		bool perTeam = false;
		double teamMultiplier = 1.0;
		std::wstring textTemplate;
		int orderIndex = 0;
		std::string id;
	};

	struct RewardDef
	{
		RewardKind kind;
		int weight = 1;
		int moneyAmount = 0;
		TechnoTypeClass* unitType = nullptr;
		int unitCount = 0;
		SuperWeaponTypeClass* swType = nullptr;
		int orderIndex = 0;
		std::string id;
		std::wstring rewardText;
	};

	struct Competitor
	{
		std::vector<HouseClass*> members;
		int progress = 0;
	};

	class Manager
	{
	public:
		static Manager& Instance();

		bool SeedsReady = false;   // NEW: set true once salts are derived
		void EnsureSeeds();        // NEW: makes sure seeds exist before any roll
		void StartContractAtFrame(int64_t anchorFrame);

		struct TransientSW { SuperClass* SW {}; int ExpireFrame {}; };
		std::vector<TransientSW> TransientSWs;
		void TickTransientSWs();
		const wchar_t* tryHouseName(HouseClass* h) const;

		//RNG
		// --- deterministic roll state (per match) ---
		uint32_t GameSyncSeed = 0;
		uint32_t RollSaltContracts = 0;   // set from Scenario->UniqueID
		uint32_t RollSaltRewards = 0;   // set from Scenario->UniqueID ^ constant
		uint32_t NextContractID = 0;   // increments once per *new* contract
		uint32_t ActiveContractID = 0;   // ID of the currently active contract
		uint32_t MatchSeed = 0;
		bool     MatchSeedCaptured = false;
		void CaptureMatchSeedIfDue(int64_t anchorFrame);


		// Deterministic start/sync from a shared anchor (handles late defs gracefully).
		void StartOrSyncFromAnchor(int64_t anchorFrame);
	
		static uint32_t Hash32(uint32_t x);
		static uint32_t HashStr32(uint32_t h, const char* s);

		// inclusive ranged: [min, max]; no UB on min>max callers should guard
		static int      DeterministicRanged(int min, int max, uint32_t salt, uint32_t index);
		int             DeterministicWeightedPick(int totalWeight, uint32_t salt, uint32_t index) const;

		//font
		MessageListClass* HeaderML { nullptr };
		int  uiHeaderHeight { 20 };
		int  uiHeaderWidth { 640 };
		int  uiHeaderColor { -1 };
		int  uiHeaderStyle { 0x4046 };
		bool uiShadow { true };
		int  uiOutlinePx { 1 };
		bool uiOutlineDiag { true };

		// lifecycle
		void LoadFromRules(CCINIClass* ini);
		void LoadFromScenario(CCINIClass* ini);
		void StartContract();
		void OnFrame();
		void DrawUI();

		// events
		void OnKill(TechnoClass* victim, HouseClass* killerOwner, bool victimIsBuilding);
		void OnBuild(BuildingClass* built);
		void OnInfiltration(HouseClass* infiltrator, BuildingClass* target);
		void OnMoney(HouseClass* house, int amount);
		void NormalizeDefinitions();
		void InitMatchSeed();
		
	private:
		// parsing
		void parseContracts(CCINIClass* ini);
		void parseRewards(CCINIClass* ini);
		std::vector<TechnoTypeClass*> parseTechnoList(CCINIClass* ini, const char* csv);
		void ensureHeaderMessageList();       // init-once helper

		// runtime
		void rebuildCompetitors();
		void startNewContract();
		int  requiredFor(const ContractDef& c) const;

		// rewards
		const RewardDef* pickRewardSync() const;
		void applyReward(const RewardDef* rwd, const Competitor& winner);

		// helpers
		static COLORREF colorOfHouse(HouseClass* h);
		

		// data
		public:std::vector<ContractDef> Contracts;
		std::vector<RewardDef>   Rewards;
		int totalRewardWeight = 0;

		std::vector<Competitor> Competitors;
		public:int  activeContractIndex = -1;
		public:int  activeRequired = 0;
		bool activeIsPerTeam = false;
		int64_t timerEndFrame = 0;

		int64_t initialAnchorFrame = -1;        // only used for the *first* contract
		int64_t currentContractStartFrame = -1; // start frame of the current contract epoch

		// EarnMoney accumulator
		std::unordered_map<const HouseClass*, int> moneyDuringContract;

		// UI config
		int  uiX = 10, uiY = 10;
		bool uiShowTimer = true;

		// deterministic timing + banner (FIX: these were missing in your header)
		int64_t      localFrame = 0;
		public:int   bannerFramesLeft = 0;
		std::wstring bannerText;


		bool HasDefinitions() const { return !Contracts.empty() && !Rewards.empty(); }
		bool HasStarted() const { return activeContractIndex >= 0 || initialAnchorFrame >= 0; }


	};

} // namespace Contracts
