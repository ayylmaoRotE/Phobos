#include "BountyUI.h"
// #include <New/Entity/BannerClass.h>
// #include <Misc/Hooks.Message.h>

using namespace Bounty;

namespace Bounty { namespace UI {

static const char* KindName(ContractKind k) {
    switch (k) {
        case ContractKind::KILL: return "Kill";
        case ContractKind::KILL_LIST: return "Kill (List)";
        case ContractKind::INCOME: return "Income";
        case ContractKind::BUILD_TYPE: return "Build";
        case ContractKind::FIRST_TO_BUILD_TYPE: return "First to Build";
        case ContractKind::INFILTRATE: return "Infiltrate";
        case ContractKind::BUILD_SUPER: return "Build Super";
        default: return "Contract";
    }
}

void Banner_AnnounceRotate(const ContractDef& c) {
    char msg[256];
    snprintf(msg, sizeof(msg), "Contract: %s — Goal: %d", KindName(c.Kind), c.Goal);
    // BannerClass::ShowCentered(msg, /*duration*/120);
    // Or push to your NewMessageList
}

void Ticker_UpdateProgress(const ContractDef& c,
                           const int* groupProgress,
                           const int* groupGoals,
                           const int* /*houseToGroup*/,
                           int groupCount) {
    // Compose a compact single-line ticker like:
    // "Contract: Kill 20 Tier1 | Team A: 7/20 | Team B: 5/20 | ..."
    // Use your ScrollClass/GScreen hooks to draw or update message list.
    (void)c; (void)groupProgress; (void)groupGoals; (void)groupCount;
}

}} // ns
