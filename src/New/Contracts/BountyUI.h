#pragma once
#include <array>
#include <string>
#include "BountyContracts.h"

namespace Bounty { namespace UI {

void Banner_AnnounceRotate(const ContractDef& c);
void Ticker_UpdateProgress(const ContractDef& c,
                           const int* groupProgress,
                           const int* groupGoals,
                           const int* houseToGroup,
                           int groupCount);

}} // ns
