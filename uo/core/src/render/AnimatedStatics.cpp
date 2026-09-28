// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/Managers/AnimatedStaticsManager.cs).

#include "uo/render/AnimatedStatics.h"

#include <algorithm>

#include "uo/assets/AnimData.h"

namespace uo::render
{

AnimatedStatics::AnimatedStatics(const ITileData& tiles, const assets::AnimData& animData) : _animData(animData)
{
    const int count = std::min<int>(tiles.itemCount(), static_cast<int>(animData.count()));
    _offsets.assign(static_cast<size_t>(std::max(tiles.itemCount(), 0)), 0);

    for (int i = 0; i < count; ++i)
    {
        if (tiles.item(static_cast<uint16_t>(i)).is(assets::TF_Animation))
        {
            _infos.push_back({0, static_cast<uint16_t>(i), 0});
        }
    }
}

bool AnimatedStatics::update(uint32_t nowMs)
{
    if (_infos.empty() || _processTime >= nowMs)
    {
        return false;
    }

    bool changed      = false;
    uint32_t nextTime = nowMs + 250;

    for (Info& o : _infos)
    {
        if (o.time < nowMs)
        {
            const assets::AnimDataEntry* info = _animData.get(o.index);
            if (!info)
            {
                continue;
            }

            uint8_t index = o.animIndex;
            o.time        = info->frameInterval > 0 ? nowMs + info->frameInterval * kDelayMs + 1 : nowMs + kDelayMs;

            if (index < info->frameCount && index < info->frames.size())
            {
                const int8_t offset = info->frames[index++];
                if (_offsets[o.index] != offset)
                {
                    _offsets[o.index] = offset;
                    changed           = true;
                }
            }

            if (index >= info->frameCount)
            {
                index = 0;
            }
            o.animIndex = index;
        }

        nextTime = std::min(nextTime, o.time);
    }

    _processTime = nextTime;
    return changed;
}

}  // namespace uo::render
