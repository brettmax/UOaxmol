// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Client: the per-mobile animation
// state of Game/GameObjects/Mobile.cs (ProcessAnimation, SetAnimation, IsWalking) and the
// equipment gathering of Game/GameObjects/Views/MobileView.cs.
//
// Connects uo::world mobiles to the animation port: keeps each mobile's frame clock and
// mirror flag, turns world state (body, equipment, steps, server animations) into
// MobileDrawInput, and expands a mobile into render::DrawItems for WorldMap's draw list.
#pragma once

#include "uo/anim/MobileRenderer.h"
#include "uo/movement/MobileMotion.h"
#include "uo/render/WorldMap.h"
#include "uo/world/World.h"

#include <cstdint>
#include <functional>
#include <unordered_map>

namespace uo::anim
{

class WorldMobileAnimator final : public render::IMobileDrawSource
{
public:
    // Tiledata of an item graphic, as far as worn items need it.
    struct ItemInfo
    {
        uint16_t animId = 0;
        bool partialHue = false;
        bool isLight    = false;
    };
    using ItemLookup   = std::function<ItemInfo(uint16_t graphic)>;
    // Step offsets of a mobile (MovementSystem::motion); null when it is not moving.
    using MotionLookup = std::function<const movement::MotionClock*(uint32_t serial)>;

    WorldMobileAnimator(AnimationCache& cache, world::World& world, ItemLookup items, MotionLookup motion);

    // Advances every mobile's frame clock to `nowMs` and forgets mobiles that left the world.
    // Returns true when anything a mobile draws changed: a frame advanced, a mobile is part-way
    // through a step, or a new server animation started.
    bool update(uint64_t nowMs);

    // Mobile state as the action logic sees it (exposed for tests).
    MobileAnimState animState(const world::Mobile& mobile, uint64_t nowMs) const;
    MobileEquipment equipment(const world::Mobile& mobile) const;

    // Current frame of a tracked mobile, 0 when unknown (exposed for tests).
    uint8_t animIndex(uint32_t serial) const;

    void setShadows(bool enabled) { _shadows = enabled; }

    // render::IMobileDrawSource
    void appendMobile(const render::WorldObject& obj, const render::DrawItem& base,
                      std::vector<render::DrawItem>& out) override;

private:
    struct Track
    {
        MobileAnimClock clock;
        MobileAnimState state; // animationGroup / animationFromServer persist between updates
        bool mirror       = false;
        uint32_t sequence = 0; // last world::ServerAnimation::sequence applied
        bool seen         = false;
        bool frozen       = false; // 0xBF 0x19 froze it on a frame (ExecuteAnimation = false)
    };

    void refreshState(Track& track, const world::Mobile& mobile, uint64_t nowMs) const;
    void applyServerAnimation(Track& track, const world::Mobile& mobile, uint32_t now);
    int frameCount(const Track& track, const world::Mobile& mobile);

    AnimationCache& _cache;
    MobileAnimation _actions;
    MobileRenderer _renderer;
    world::World& _world;
    ItemLookup _items;
    MotionLookup _motion;
    std::unordered_map<uint32_t, Track> _tracks;
    uint8_t _bodyConversionFlags = 0;
    bool _shadows                = true;
};

} // namespace uo::anim
