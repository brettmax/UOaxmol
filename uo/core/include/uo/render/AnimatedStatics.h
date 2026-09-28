// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Managers/AnimatedStaticsManager.cs).
//
// Animated item art (water, fires, water wheels, fields): every graphic whose
// tiledata has TF_Animation cycles through the frame offsets in animdata.mul.
// ClassicUO keeps one offset per graphic (the art index's AnimOffset), so all
// copies of a graphic animate in step; the draw list adds it to the graphic of
// statics and ground items.

#pragma once

#include <cstdint>
#include <vector>

#include "uo/render/WorldSource.h"

namespace uo::assets
{
class AnimData;
}

namespace uo::render
{

class AnimatedStatics
{
public:
    // Item art ticks every 100 ms times the entry's frame interval
    // (ITEM_EFFECT_ANIMATION_DELAY * 2).
    static constexpr uint32_t kDelayMs = 100;

    AnimatedStatics(const ITileData& tiles, const assets::AnimData& animData);

    // Advances the animations to `nowMs`. True when any graphic's offset changed,
    // so the draw list needs rebuilding.
    bool update(uint32_t nowMs);

    // Frame offset for `graphic` (0 when it does not animate).
    int offset(uint16_t graphic) const { return graphic < _offsets.size() ? _offsets[graphic] : 0; }
    uint16_t animated(uint16_t graphic) const { return static_cast<uint16_t>(graphic + offset(graphic)); }

    size_t count() const { return _infos.size(); }

private:
    struct Info
    {
        uint32_t time     = 0;
        uint16_t index    = 0;
        uint8_t animIndex = 0;
    };

    const assets::AnimData& _animData;
    std::vector<Info> _infos;
    std::vector<int8_t> _offsets;
    uint32_t _processTime = 0;
};

}  // namespace uo::render
