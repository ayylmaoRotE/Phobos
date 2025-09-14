#pragma once
#include <vector>
#include <utility>
#include <string>
#include <Utilities/Savegame.h>
#include <Utilities/SavegameDef.h>

// Forward decls
class HouseClass;
class SuperWeaponTypeClass;

namespace Contracts
{

	struct ContractsSave
	{
		static constexpr uint32_t Tag = 'CTRB'; // block tag
		static constexpr uint32_t Version = 2;      // v2 = relative time

		// toggles / seeds
		bool     enabled {};
		bool     seedsReady {};
		bool     matchSeedCaptured {};
		uint32_t matchSeed {}, gameSyncSeed {}, rollSaltContracts {}, rollSaltRewards {};

		// selection / epoch
		int32_t  activeContractIndex { -1 };
		uint32_t nextContractID {};
		uint32_t activeContractID {};
		bool     activeIsPerTeam {};
		int32_t  activeRequired {};

		// timing (relative!)
		int32_t  remainingContractFrames { 0 };       // >=0 if active
		int32_t  remainingIntermissionFrames { -1 };  // -1 = no intermission
		int32_t  contractSpanFrames { 0 };            // span frames for the active contract

		// UI
		int32_t      bannerFramesLeft { 0 };
		std::wstring bannerText;

		// competitors (by indices) + progress
		struct Team
		{
			std::vector<int32_t> members; // HouseClass::ArrayIndex
			int32_t progress {};
		};
		std::vector<Team> teams;

		// money during contract (houseIdx, amount)
		std::vector<std::pair<int32_t, int32_t>> moneyByHouse;

		// transient SWs (owner idx, SW type idx, remaining frames)
		struct TransientSW
		{
			int32_t ownerIdx { -1 };
			int32_t typeIdx { -1 };
			int32_t expireFramesLeft { 0 };
		};
		std::vector<TransientSW> transients;

		// Serialize via Phobos helpers
		bool Save(PhobosStreamWriter& w) const;
		bool Load(PhobosStreamReader& r, bool registerForChange);
	};
}

namespace Savegame
{
	template<>
	struct PhobosStreamObject<Contracts::ContractsSave::Team>
	{
		bool WriteToStream(PhobosStreamWriter& w, const Contracts::ContractsSave::Team& v) const
		{
			return WritePhobosStream(w, v.members) && WritePhobosStream(w, v.progress);
		}
		bool ReadFromStream(PhobosStreamReader& r, Contracts::ContractsSave::Team& v, bool reg) const
		{
			return ReadPhobosStream(r, v.members, reg) && ReadPhobosStream(r, v.progress, reg);
		}
	};

	template<>
	struct PhobosStreamObject<Contracts::ContractsSave::TransientSW>
	{
		bool WriteToStream(PhobosStreamWriter& w, const Contracts::ContractsSave::TransientSW& v) const
		{
			return WritePhobosStream(w, v.ownerIdx)
				&& WritePhobosStream(w, v.typeIdx)
				&& WritePhobosStream(w, v.expireFramesLeft);
		}
		bool ReadFromStream(PhobosStreamReader& r, Contracts::ContractsSave::TransientSW& v, bool reg) const
		{
			return ReadPhobosStream(r, v.ownerIdx, reg)
				&& ReadPhobosStream(r, v.typeIdx, reg)
				&& ReadPhobosStream(r, v.expireFramesLeft, reg);
		}
	};

	// (Optional but nice) be explicit for pairs of ints:
	template<>
	struct PhobosStreamObject<std::pair<int32_t, int32_t>>
	{
		bool WriteToStream(PhobosStreamWriter& w, std::pair<int32_t, int32_t> const& p) const
		{
			return WritePhobosStream(w, p.first) && WritePhobosStream(w, p.second);
		}
		bool ReadFromStream(PhobosStreamReader& r, std::pair<int32_t, int32_t>& p, bool reg) const
		{
			return ReadPhobosStream(r, p.first, reg) && ReadPhobosStream(r, p.second, reg);
		}
	};
}
