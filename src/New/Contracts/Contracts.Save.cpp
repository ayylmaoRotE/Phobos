#include "Contracts.Save.h"
#include "HouseClass.h"
#include "SuperWeaponTypeClass.h"
#include "Utilities/Savegame.h"
#include "Utilities/SavegameDef.h"
#include <Unsorted.h>

using Savegame::WritePhobosStream;
using Savegame::ReadPhobosStream;

namespace Contracts
{

	bool ContractsSave::Save(PhobosStreamWriter& w) const
	{
		// header
		if (!WritePhobosStream(w, Tag))     return false;
		if (!WritePhobosStream(w, Version)) return false;

		// data (Phobos already supports STL, ints, bools, wstrings)
		return
			WritePhobosStream(w, enabled) &&
			WritePhobosStream(w, seedsReady) &&
			WritePhobosStream(w, matchSeedCaptured) &&
			WritePhobosStream(w, matchSeed) &&
			WritePhobosStream(w, gameSyncSeed) &&
			WritePhobosStream(w, rollSaltContracts) &&
			WritePhobosStream(w, rollSaltRewards) &&
			WritePhobosStream(w, activeContractIndex) &&
			WritePhobosStream(w, nextContractID) &&
			WritePhobosStream(w, activeContractID) &&
			WritePhobosStream(w, activeIsPerTeam) &&
			WritePhobosStream(w, activeRequired) &&
			WritePhobosStream(w, remainingContractFrames) &&
			WritePhobosStream(w, remainingIntermissionFrames) &&
			WritePhobosStream(w, contractSpanFrames) &&
			WritePhobosStream(w, bannerFramesLeft) &&
			WritePhobosStream(w, bannerText) &&
			WritePhobosStream(w, teams) &&
			WritePhobosStream(w, moneyByHouse) &&
			WritePhobosStream(w, transients);
	}

	bool ContractsSave::Load(PhobosStreamReader& r, bool /*registerForChange*/)
	{
		uint32_t tag = 0, ver = 0;
		if (!ReadPhobosStream(r, tag) || tag != Tag) return false;
		if (!ReadPhobosStream(r, ver) || ver > Version) return false;

		// read common (v1) fields up to activeRequired
		bool ok =
			ReadPhobosStream(r, enabled) &&
			ReadPhobosStream(r, seedsReady) &&
			ReadPhobosStream(r, matchSeedCaptured) &&
			ReadPhobosStream(r, matchSeed) &&
			ReadPhobosStream(r, gameSyncSeed) &&
			ReadPhobosStream(r, rollSaltContracts) &&
			ReadPhobosStream(r, rollSaltRewards) &&
			ReadPhobosStream(r, activeContractIndex) &&
			ReadPhobosStream(r, nextContractID) &&
			ReadPhobosStream(r, activeContractID) &&
			ReadPhobosStream(r, activeIsPerTeam) &&
			ReadPhobosStream(r, activeRequired);

		if (!ok) return false;

		// v2 extras (these three ONLY exist since v2)
		if (ver >= 2)
		{
			ok =
				ReadPhobosStream(r, remainingContractFrames) &&
				ReadPhobosStream(r, remainingIntermissionFrames) &&
				ReadPhobosStream(r, contractSpanFrames);
		}
		else
		{
			// sane defaults for old savefiles
			remainingContractFrames = 0;
			remainingIntermissionFrames = -1;
			contractSpanFrames = 0;
		}
		if (!ok) return false;

		// rest of v1 fields (order must match Save)
		ok =
			ReadPhobosStream(r, bannerFramesLeft) &&
			ReadPhobosStream(r, bannerText) &&
			ReadPhobosStream(r, teams) &&
			ReadPhobosStream(r, moneyByHouse) &&
			ReadPhobosStream(r, transients);

		return ok;
	}

}
