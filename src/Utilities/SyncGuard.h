#pragma once

#if __has_include(<Always.h>)
#include <Always.h>
#elif __has_include(<GeneralDefinitions.h>)
#include <GeneralDefinitions.h>
#elif __has_include(<Helpers/Inline.h>)
#include <Helpers/Inline.h>
#elif __has_include("Always.h")
#include <Always.h>
#elif __has_include("GeneralDefinitions.h")
#include <GeneralDefinitions.h>
#elif __has_include("Helpers/Inline.h")
#include <Helpers/Inline.h>
#endif

// Some trees need Windows / limits visible for types used by Randomizer
#if __has_include(<Windows.h>)
#include <Windows.h>
#endif
#include <climits>

#include <Randomizer.h>
#include <ScenarioClass.h>
#include <cstddef>
#include <cstring>

// RAII guard that snapshots & restores Scenario RNG as raw bytes.
// No Randomizer copy/assign or internals required.
struct SyncGuard
{
	alignas(Randomizer) unsigned char saved_[sizeof(Randomizer)];
	bool ok_ = false;
	SyncGuard()
	{
		if (auto* sc = ScenarioClass::Instance)
		{
			std::memcpy(saved_, &sc->Random, sizeof(Randomizer));
			ok_ = true;
		}
	}
	~SyncGuard()
	{
		if (ok_)
		{
			if (auto* sc = ScenarioClass::Instance)
			{
				std::memcpy(&sc->Random, saved_, sizeof(Randomizer));
			}
		}
	}
};
