// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Client (Game/GameObjects/MobileAnimation.cs).
//
// Picks which action (animation group) a mobile plays: idle, walk, run, mounted, war mode,
// flying gargoyle, and the server-driven actions of packets 0x6E (GetGroupForAnimation) and
// 0xE2 (GetObjectNewAnimation), including the body-type conversions between the monster,
// animal, sea monster and people groups.
#pragma once

#include "uo/anim/AnimationCache.h"

#include <cstdint>
#include <functional>

namespace uo::anim
{

// Everything the action logic reads from a mobile. The world layer fills it; nothing here
// depends on how mobiles are stored.
struct MobileAnimState
{
    uint16_t graphic = 0; // body as sent by the server (Mobile.Graphic)

    // Current action set by the server, 0xFF for none (Mobile._animationGroup).
    uint8_t animationGroup   = 0xFF;
    bool animationFromServer = false;

    // Walking/running: true while a step is queued or the last step is recent. When a step
    // is queued, isRunning is that step's run flag.
    bool isWalking = false;
    bool isRunning = false;

    bool inWarMode = false;
    bool isDead    = false;
    bool isMounted = false; // has a Mount-layer item that is not a boat
    bool isFlying  = false; // gargoyle flight (7.0+ clients reuse the poisoned flag)
    bool isGargoyle = false;

    struct Hand
    {
        bool present    = false;
        uint16_t animId = 0;     // tiledata AnimID of the held item
        bool isLight    = false; // tiledata LightSource
    };
    Hand oneHanded;
    Hand twoHanded;
};

// Mobile.GetGraphicForAnimation: bodies drawn with another body's frames.
uint16_t graphicForAnimation(uint16_t body);

// Mobile.IsHuman: bodies that wear equipment.
bool isHumanBody(uint16_t body);

// Mobile.IsGargoyle, including its operator precedence: 0x029B counts on every version.
bool isGargoyleBody(uint16_t body, ClientVersion version);

class MobileAnimation
{
public:
    explicit MobileAnimation(AnimationCache& cache);

    // Replaces RandomHelper for the one random idle choice in LABEL_222 (tests pin it).
    void setRandom(std::function<uint32_t()> random) { _random = std::move(random); }

    // GetGroupForAnimation. checkGraphic 0 means "the mobile's own body".
    uint8_t groupForAnimation(const MobileAnimState& mobile, uint16_t checkGraphic = 0);

    // GetObjectNewAnimation for packet 0xE2 (type, action, mode). 0xFF means "no change".
    uint8_t objectNewAnimation(const MobileAnimState& mobile, uint16_t type, uint16_t action, uint8_t mode);

    bool isReplacedObjectAnimation(uint8_t anim, uint16_t v13) const;
    uint8_t replacedObjectAnimation(uint16_t graphic, uint16_t index);

private:
    void label190(uint32_t flags, uint16_t& v13);
    void label222(uint32_t flags, uint16_t& v13);
    void calculateHeight(uint16_t graphic, const MobileAnimState& mobile, uint32_t flags, bool isRun, bool isWalking,
                         uint8_t& result);

    AnimationsLoader& loader() { return _cache.loader(); }

    AnimationCache& _cache;
    std::function<uint32_t()> _random;
};

} // namespace uo::anim
