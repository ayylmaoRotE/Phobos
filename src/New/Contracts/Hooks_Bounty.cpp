// Optional: raw hooks if you prefer not to call from your existing wrappers.
// Replace addresses with your base's values. Otherwise, delete this file and wire calls directly.

#include "BountyContracts.h"

// #include <YRpp/Unsorted.h>
// #include <YRpp/TechnoClass.h>
// #include <YRpp/HouseClass.h>
// #include <YRpp/BuildingClass.h>
// #include <YRpp/FootClass.h>
// #include <Helpers/Macro.h>

using Bounty::Manager;

// Example:
/*
DEFINE_HOOK(0x55D360, MainLoop_FrameStep_Begin, 0x5) {
    Manager::Instance().OnFrame(Unsorted::CurrentFrame);
    return 0;
}

DEFINE_HOOK(0x702E4E, TechnoClass_RegisterDestruction_SaveKillerInfo, 0x6) {
    GET(TechnoClass*, pVictim, EAX);
    GET(TechnoClass*, pKiller, ESI);
    HouseClass* killerHouse = pKiller ? pKiller->Owner : nullptr;
    Manager::Instance().OnKill(pVictim, pKiller, killerHouse);
    return 0;
}

DEFINE_HOOK(0x441234, BuildingClass_Unlimbo_Done, 0x6) {
    GET(BuildingClass*, pBld, ESI);
    Manager::Instance().OnBuildingCompleted(pBld);
    return 0;
}

DEFINE_HOOK(0x51A00B, BuildingClass_InfiltratedBy_Wrapper, 0x5) {
    GET(BuildingClass*, pVictim, ESI);
    GET(FootClass*, pIntruder, EDI);
    Manager::Instance().OnInfiltrate(pVictim, pIntruder);
    return 0;
}
*/
