#include "GiftBoxFunctional.h"

#include <Ext/TechnoType/Body.h>
#include <Ext/Techno/Body.h>
#include "GiftBox.h"
#include "GiftBoxData.h"

// Disable GiftBox debug logging
#define GIFTBOX_DEBUG_ENABLED false

static bool OpenDisallowed(TechnoClass* const pTechno)
{
    if(!pTechno) { return true; }
    if(pTechno->InLimbo) { return false; } // not spawned yet

    const bool bIsOnWarfactory = false; // adapt if available in your fork
    return pTechno->Absorbed
        || pTechno->InOpenToppedTransport
        || bIsOnWarfactory
        || pTechno->TemporalTargetingMe;
}

void GiftBoxFunctional::Init(TechnoExt::ExtData* pExt, TechnoTypeExt::ExtData* pTypeExt)
{
    if (GIFTBOX_DEBUG_ENABLED) {
        Debug::Log("GiftBoxFunctional::Init called, Enable=%d\n", 
            pTypeExt->MyGiftBoxData.Enable.Get());
    }
    
    // Simple check - if not enabled, nothing to do
    if (!pTypeExt->MyGiftBoxData.Enable) {
        return;
    }

    const auto delay = (pTypeExt->MyGiftBoxData.DelayMax == 0)
        ? pTypeExt->MyGiftBoxData.Delay
        : (ScenarioClass::Instance->Random.Random()
           % (pTypeExt->MyGiftBoxData.DelayMax - pTypeExt->MyGiftBoxData.DelayMin + 1))
          + pTypeExt->MyGiftBoxData.DelayMin;

    if (GIFTBOX_DEBUG_ENABLED) {
        Debug::Log("GiftBox: Creating with delay %d\n", delay);
    }
    pExt->MyGiftBox = std::make_unique<GiftBox>(delay);
    if (GIFTBOX_DEBUG_ENABLED) {
        Debug::Log("GiftBox: Created successfully\n");
    }
}

void GiftBoxFunctional::Destroy(TechnoExt::ExtData* pExt, TechnoTypeExt::ExtData* pTypeExt)
{
    if(!pExt->MyGiftBox || OpenDisallowed(pExt->OwnerObject())) { return; }

    if(pTypeExt->MyGiftBoxData.OpenWhenDestoryed && !pExt->MyGiftBox->IsOpen) {
        pExt->MyGiftBox->Release(pExt->OwnerObject(), pTypeExt->MyGiftBoxData);
        pExt->MyGiftBox->IsOpen = true;
    }
}

void GiftBoxFunctional::AI(TechnoExt::ExtData* pExt, TechnoTypeExt::ExtData* pTypeExt)
{
    if(!pExt->MyGiftBox || OpenDisallowed(pExt->OwnerObject())) { return; }

    if(!pTypeExt->MyGiftBoxData.Enable) {
        pExt->MyGiftBox.reset(nullptr);
        return;
    }

    const bool canOpen = pExt->MyGiftBox->CanOpen();
    const bool timeup  = pExt->MyGiftBox->Timeup();
    if (GIFTBOX_DEBUG_ENABLED) {
        Debug::Log("GiftBox AI: CanOpen=%d, Timeup=%d, IsOpen=%d, Delay=%d\n",
            canOpen, timeup, pExt->MyGiftBox->IsOpen, pExt->MyGiftBox->Delay);
    }

    // open by timer when not configured for death or health trigger
    if(!pExt->MyGiftBox->IsOpen && pExt->MyGiftBox->Timeup()) {
        if(!pTypeExt->MyGiftBoxData.OpenWhenDestoryed
           && !pTypeExt->MyGiftBoxData.OpenWhenHealthPercent.isset())
        {
            if (GIFTBOX_DEBUG_ENABLED) {
                Debug::Log("GiftBox: Timer expired, opening and releasing gifts\n");
            }
            pExt->MyGiftBox->Release(pExt->OwnerObject(), pTypeExt->MyGiftBoxData);
            pExt->MyGiftBox->IsOpen = true;
        }
    }

    if(pExt->MyGiftBox->IsOpen) {
        if (GIFTBOX_DEBUG_ENABLED) {
            Debug::Log("GiftBox: Is open, checking remove/destroy\n");
        }

        if(pTypeExt->MyGiftBoxData.Remove) {
            if (GIFTBOX_DEBUG_ENABLED) {
                Debug::Log("GiftBox: Removing original unit\n");
            }
            pExt->OwnerObject()->Limbo();
            pExt->OwnerObject()->UnInit();
            return;
        }

        if(pTypeExt->MyGiftBoxData.Destroy) {
            if (GIFTBOX_DEBUG_ENABLED) {
                Debug::Log("GiftBox: Destroying original unit\n");
            }
            auto dmg = (pExt->OwnerObject()->GetTechnoType()->Strength);
            pExt->OwnerObject()->ReceiveDamage(&dmg, 0,
                RulesClass::Instance->C4Warhead, nullptr, false,
                !pExt->OwnerObject()->GetTechnoType()->Crewed, nullptr);
            return;
        }

        if(pExt->OwnerObject()->IsAlive) {
            const auto delay = (pTypeExt->MyGiftBoxData.DelayMax == 0)
                ? pTypeExt->MyGiftBoxData.Delay
                : (ScenarioClass::Instance->Random.Random()
                   % (pTypeExt->MyGiftBoxData.DelayMax - pTypeExt->MyGiftBoxData.DelayMin + 1))
                  + pTypeExt->MyGiftBoxData.DelayMin;
            if (GIFTBOX_DEBUG_ENABLED) {
                Debug::Log("GiftBox: Resetting with delay %d\n", delay);
            }
            pExt->MyGiftBox->Reset(delay);
        }
    }
}

void GiftBoxFunctional::TakeDamage(TechnoExt::ExtData* pExt, TechnoTypeExt::ExtData* pTypeExt, WarheadTypeClass*, DamageState nState)
{
    if(!pExt->MyGiftBox) { return; }

    if(nState != DamageState::NowDead
       && !OpenDisallowed(pExt->OwnerObject())
       && pTypeExt->MyGiftBoxData.OpenWhenHealthPercent.isset())
    {
        const double hp = pExt->OwnerObject()->GetHealthPercentage();
        if(hp <= pTypeExt->MyGiftBoxData.OpenWhenHealthPercent.Get()) {
            pExt->MyGiftBox->Release(pExt->OwnerObject(), pTypeExt->MyGiftBoxData);
            pExt->MyGiftBox->IsOpen = true;
        }
    }
}
