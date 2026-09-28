// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Client: Game/GameObjects/Mobile.cs
// (ProcessAnimation, NoIterateAnimIndex, IsWalking, EnqueueStep's animation reset) and the
// animation packets of Network/PacketHandlers.cs (0x6E, 0xE2, 0xAF, 0xBF 0x19).
#include "uo/anim/WorldMobiles.h"

#include "uo/render/HueVector.h"

namespace uo::anim
{

namespace
{

// ClassicUO's animation clock runs on 32-bit Time.Ticks.
uint32_t ticks(uint64_t nowMs)
{
    return static_cast<uint32_t>(nowMs);
}

bool recentStep(const movement::MotionClock* motion, uint64_t nowMs)
{
    return motion && motion->lastStepTime + MobileAnimClock::WALKING_DELAY > nowMs;
}

} // namespace

WorldMobileAnimator::WorldMobileAnimator(AnimationCache& cache, world::World& world, ItemLookup items,
                                         MotionLookup motion)
    : _cache(cache), _actions(cache), _renderer(cache, _actions), _world(world), _items(std::move(items)),
      _motion(std::move(motion))
{
}

MobileEquipment WorldMobileAnimator::equipment(const world::Mobile& mobile) const
{
    MobileEquipment eq;

    for (const world::Serial s : mobile.contents())
    {
        const world::Item* item = _world.item(s);
        if (!item)
        {
            continue;
        }

        const auto layer = static_cast<size_t>(item->layer);
        if (layer == 0 || layer >= LAYER_COUNT)
        {
            continue;
        }

        EquippedItem& slot = eq.layers[layer];
        const ItemInfo info = _items ? _items(item->graphic) : ItemInfo{};
        slot.present        = true;
        slot.graphic        = item->graphic;
        slot.animId         = info.animId;
        slot.hue            = item->hue;
        slot.partialHue     = info.partialHue;
        slot.isLight        = info.isLight;
    }

    return eq;
}

MobileAnimState WorldMobileAnimator::animState(const world::Mobile& mobile, uint64_t nowMs) const
{
    MobileAnimState s;
    const ClientVersion version = _cache.loader().version();
    const movement::MotionClock* motion = _motion ? _motion(mobile.serial) : nullptr;

    s.graphic   = mobile.graphic;
    s.isWalking = !mobile.steps.empty() || recentStep(motion, nowMs);
    s.isRunning = mobile.steps.empty() ? mobile.isRunning : mobile.steps.front().run;
    s.inWarMode = mobile.inWarMode();
    s.isDead    = mobile.isDead();
    s.isFlying  = mobile.isFlying(version);
    s.isGargoyle = mobile.race == world::Race::Gargoyle || isGargoyleBody(mobile.graphic, version);

    const MobileEquipment eq = equipment(mobile);
    if (const EquippedItem* mount = eq.find(Layer::Mount))
    {
        s.isMounted = mount->graphic != BOAT_MOUNT_GRAPHIC;
    }

    auto hand = [&](Layer layer, MobileAnimState::Hand& out) {
        if (const EquippedItem* item = eq.find(layer))
        {
            out.present = true;
            out.animId  = item->animId;
            out.isLight = item->isLight;
        }
    };
    hand(Layer::OneHanded, s.oneHanded);
    hand(Layer::TwoHanded, s.twoHanded);
    return s;
}

void WorldMobileAnimator::refreshState(Track& track, const world::Mobile& mobile, uint64_t nowMs) const
{
    const uint8_t group     = track.state.animationGroup;
    const bool fromServer   = track.state.animationFromServer;
    track.state             = animState(mobile, nowMs);
    track.state.animationGroup      = group;
    track.state.animationFromServer = fromServer;
}

void WorldMobileAnimator::applyServerAnimation(Track& track, const world::Mobile& mobile, uint32_t now)
{
    const world::ServerAnimation& a = mobile.animation;
    MobileAnimState& s              = track.state;

    // ClassicUO never turns ExecuteAnimation back on; any later server animation does here.
    track.frozen = a.source == world::ServerAnimation::Source::FrameSet;

    switch (a.source)
    {
    case world::ServerAnimation::Source::None:
        break;

    case world::ServerAnimation::Source::Legacy:
        // 0x6E CharacterAnimation.
        track.clock.setAnimation(s, _actions.replacedObjectAnimation(mobile.graphic, a.action), now, a.delay,
                                 static_cast<uint8_t>(a.frameCount), static_cast<uint8_t>(a.repeatCount), a.repeat,
                                 a.forward, true);
        break;

    case world::ServerAnimation::Source::New:
        // 0xE2 NewCharacterAnimation.
        track.clock.setAnimation(s, _actions.objectNewAnimation(s, a.type, a.action, a.mode), now, 0, 0, 1,
                                 (a.type == 1 || a.type == 2) && mobile.graphic == 0x0015, true, true);
        break;

    case world::ServerAnimation::Source::Death:
    {
        // 0xAF DisplayDeath.
        uint16_t gfx = mobile.graphic;
        _cache.convertBodyIfNeeded(gfx);
        const uint8_t group = _cache.loader().getDeathAction(gfx, _cache.getAnimFlags(gfx), _cache.getAnimType(gfx),
                                                             a.running, true);
        track.clock.setAnimation(s, group, now, 0, 5, 1);
        track.clock.animIndex = 0;
        break;
    }

    case world::ServerAnimation::Source::FrameSet:
        // 0xBF 0x19 v5: hold a frame of an action (ExecuteAnimation = false).
        track.clock.setAnimation(s, _actions.replacedObjectAnimation(mobile.graphic, a.action), now);
        track.clock.animIndex = static_cast<uint8_t>(a.frameCount);
        break;
    }
}

int WorldMobileAnimator::frameCount(const Track& track, const world::Mobile& mobile)
{
    const uint16_t id = graphicForAnimation(mobile.graphic);
    if (id >= _cache.maxAnimationCount())
    {
        return 0;
    }

    uint8_t dir = static_cast<uint8_t>(static_cast<uint8_t>(mobile.direction) & 7);
    bool mirror = false;
    AnimationCache::getAnimDirection(dir, mirror);
    if (dir >= MAX_DIRECTIONS)
    {
        return 0;
    }

    const uint8_t action = _actions.groupForAnimation(track.state, id);
    return static_cast<int>(_cache.getAnimationFrames(id, action, dir).frames.size());
}

bool WorldMobileAnimator::update(uint64_t nowMs)
{
    bool changed = false;

    // Bodyconv.def applies per expansion; the server can change the flags mid-session. This
    // clears the frame cache, so any draw list holding its frames must be rebuilt.
    if (_world.bodyConversionFlags != _bodyConversionFlags)
    {
        _bodyConversionFlags = _world.bodyConversionFlags;
        _cache.updateAnimationTable(_bodyConversionFlags);
        _tracks.clear();
        changed = true;
    }

    for (auto& [serial, track] : _tracks)
    {
        track.seen = false;
    }

    const uint32_t now = ticks(nowMs);

    _world.forEachMobile([&](world::Mobile& mobile) {
        auto [it, inserted] = _tracks.try_emplace(mobile.serial);
        Track& track        = it->second;
        track.seen          = true;

        const bool wasWalking = track.state.isWalking;
        refreshState(track, mobile, nowMs);

        if (inserted)
        {
            // The animation already playing when a mobile appears is not replayed.
            track.sequence = mobile.animation.sequence;
        }
        else if (mobile.animation.sequence != track.sequence)
        {
            track.sequence = mobile.animation.sequence;
            applyServerAnimation(track, mobile, now);
            changed = true;
        }

        // Mobile.EnqueueStep: a walk starting from rest drops the server action.
        if (!mobile.steps.empty() && !wasWalking)
        {
            track.clock.setAnimation(track.state, 0xFF, now);
            changed = true;
        }

        const movement::MotionClock* motion = _motion ? _motion(mobile.serial) : nullptr;
        if (motion && (motion->offsetX != 0 || motion->offsetY != 0 || motion->offsetZ != 0))
        {
            changed = true;
        }

        // Mobile.NoIterateAnimIndex.
        const bool noIterate = track.frozen || (recentStep(motion, nowMs) && mobile.steps.empty());

        const auto result = track.clock.tick(track.state, now, frameCount(track, mobile), noIterate);
        if (result == MobileAnimClock::Result::Advanced || result == MobileAnimClock::Result::LoopedToStart)
        {
            changed = true;
        }
    });

    std::erase_if(_tracks, [](const auto& kv) { return !kv.second.seen; });
    return changed;
}

uint8_t WorldMobileAnimator::animIndex(uint32_t serial) const
{
    const auto it = _tracks.find(serial);
    return it == _tracks.end() ? 0 : it->second.clock.animIndex;
}

void WorldMobileAnimator::appendMobile(const render::WorldObject& obj, const render::DrawItem& base,
                                       std::vector<render::DrawItem>& out)
{
    world::Mobile* mobile = _world.mobile(obj.serial);
    if (!mobile)
    {
        return;
    }

    Track& track = _tracks[obj.serial];

    MobileDrawInput in;
    in.body        = mobile->graphic;
    in.hue         = mobile->hue;
    in.direction   = static_cast<uint8_t>(static_cast<uint8_t>(mobile->direction) & 7);
    in.animIndex   = track.clock.animIndex;
    in.isDead      = mobile->isDead();
    in.isHidden    = mobile->isHidden();
    in.isFemale    = mobile->isFemale;
    in.shadows     = _shadows;
    in.overrideHue = defaultOverrideHue(in.body, in.isDead, in.isHidden);
    in.anim        = track.state;
    in.equipment   = equipment(*mobile);

    int offX = 0, offY = 0, offZ = 0;
    if (const movement::MotionClock* motion = _motion ? _motion(obj.serial) : nullptr)
    {
        offX = motion->offsetX;
        offY = motion->offsetY;
        offZ = motion->offsetZ;
    }

    const MobileDrawList list = _renderer.build(in, track.mirror, base.screenX, base.screenY, offX, offY, offZ);

    for (const MobileDrawCommand& cmd : list.commands)
    {
        if (!cmd.frame)
        {
            continue; // an invisible sitting body only anchors the layout
        }

        render::DrawItem item = base;
        item.graphic          = cmd.graphic;
        item.screenX          = cmd.x;
        item.screenY          = cmd.y;
        item.frame            = cmd.frame;
        item.mirror           = cmd.mirror;

        if (cmd.kind == MobileDrawCommand::Kind::Shadow)
        {
            item.type = render::DrawType::AnimShadow;
            item.hue  = render::HueVector{0, render::SHADER_SHADOW, 1};
        }
        else
        {
            // Mobiles are never cut by the circle of transparency.
            item.type = render::DrawType::AnimFrame;
            item.hue  = render::makeHueVector(cmd.hue, cmd.partialHue, obj.alpha / 255.0f);
        }

        out.push_back(item);
    }
}

} // namespace uo::anim
