#pragma once
#include <TechnoClass.h>
#include <HouseClass.h>
#include <ScenarioClass.h>
#include <Utilities/Savegame.h>
#include <Utilities/Debug.h>
#include <Utilities/TemplateDef.h>
#include <GeneralStructures.h>
#include <YRPP.h>

#include "GiftBoxData.h"

struct GiftBox {
    bool IsOpen;
    int  Delay;
    CDTimerClass DelayTimer;

    GiftBox() : IsOpen(false), Delay(0), DelayTimer{} {}
    explicit GiftBox(int delay) : IsOpen(false), Delay(delay), DelayTimer{} {
        if (Delay > 0) { DelayTimer.Start(Delay); }
    }

    inline void Reset(int delay) {
        IsOpen = false;
        Delay  = delay;
        if (Delay > 0) { DelayTimer.Start(delay); }
    }

    inline bool Timeup() const {
        return (Delay <= 0) || DelayTimer.Expired();
    }
    inline bool CanOpen() const {
        return !IsOpen && Timeup();
    }

    void Release(TechnoClass* pOwner, GiftBoxData& data);

    bool Save(PhobosStreamWriter& Stm) const {
        const_cast<GiftBox*>(this)->Serialize(Stm);
        return true;
    }
    bool Load(PhobosStreamReader& Stm, bool /*registerForChange*/) {
        this->Serialize(Stm);

        // Preserve loaded state. If a delay was deserialized, ensure the timer is running.
        // Previously we forced IsOpen = false and Delay = 0 here which discarded the serialized
        // state and could leave the timer/objects in an inconsistent state across save/load cycles.
        if (!this->IsOpen && this->Delay > 0 && !this->DelayTimer.IsTicking()) {
            this->DelayTimer.Start(this->Delay);
        }
        return true;
    }

    template <typename TStream>
    bool Serialize(TStream& Stm) {
        return Stm
            .Process(this->IsOpen)
            .Process(this->Delay)
            .Process(this->DelayTimer)
            .Success();
    }
};

template <>
struct Savegame::ObjectFactory<GiftBox> {
    std::unique_ptr<GiftBox> operator()(PhobosStreamReader&) const {
        return std::make_unique<GiftBox>();
    }
};
