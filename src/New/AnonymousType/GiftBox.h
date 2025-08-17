#pragma once

#include "GiftBoxData.h"
#include <TechnoClass.h>
#include <Utilities/SavegameDef.h> // if available in your tree

class GiftBox
{
public:
    int  Delay { 0 };
    CDTimerClass DelayTimer {};
    bool IsOpen { false };

    GiftBox() = default;
    explicit GiftBox(int delay)
        : Delay(delay)
        , DelayTimer(delay)
        , IsOpen(false)
    { }

    inline bool CanOpen() const
    {
        return (Delay <= 0) || DelayTimer.Expired();
    }

    // IMPORTANT: no side-effect (do NOT set IsOpen here)
    inline bool Timeup() const
    {
        return CanOpen();
    }

    inline void Reset(int delay)
    {
        Delay = delay;
        DelayTimer.Start(delay);
        IsOpen = false;
    }

    void Release(TechnoClass* pOwner, GiftBoxData& nData);
};

// Let save/load reconstruct a GiftBox
namespace Savegame
{
    template <>
    struct ObjectFactory<GiftBox>
    {
        std::unique_ptr<GiftBox> operator()(PhobosStreamReader&) const
        {
            return std::make_unique<GiftBox>();
        }
    };
}
