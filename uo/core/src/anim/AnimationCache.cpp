// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.Renderer/Animations/Animations.cs).
#include "uo/anim/AnimationCache.h"

namespace uo::anim
{

bool Frame::hitTest(int x, int y) const
{
    if (x < 0 || y < 0 || x >= width || y >= height || hitMask.empty())
    {
        return false;
    }
    const size_t bit = size_t(y) * size_t(width) + size_t(x);
    return (hitMask[bit >> 6] >> (bit & 63)) & 1u;
}

AnimationCache::AnimationCache(AnimationsLoader& loader) : _loader(loader)
{
    _dataIndex.resize(MAX_ANIMATION_COUNT);
}

void AnimationCache::clear()
{
    for (auto& p : _dataIndex)
    {
        p.reset();
    }
    ++_generation;
}

void AnimationCache::updateAnimationTable(uint32_t bodyConvFlags)
{
    _loader.processBodyConvDef(bodyConvFlags);
    clear();
}

void AnimationCache::getAnimDirection(uint8_t& dir, bool& mirror)
{
    switch (dir)
    {
    case 2:
    case 4:
        mirror = dir == 2;
        dir    = 1;
        break;

    case 1:
    case 5:
        mirror = dir == 1;
        dir    = 2;
        break;

    case 0:
    case 6:
        mirror = dir == 0;
        dir    = 3;
        break;

    case 3: dir = 0; break;

    case 7: dir = 4; break;

    default: break;
    }
}

AnimationCache::IndexAnimation* AnimationCache::getIndexAnim(uint16_t id)
{
    if (id >= MAX_ANIMATION_COUNT)
    {
        return nullptr;
    }

    auto& slot = _dataIndex[id];
    if (slot)
    {
        return slot.get();
    }

    slot           = std::make_unique<IndexAnimation>();
    auto& index    = *slot;
    auto indices   = _loader.getIndices(id);
    index.flags    = indices.flags;
    index.fileIndex = indices.fileIndex;
    index.type     = indices.type;
    index.isUop    = (index.flags & AnimationFlags::UseUopAnimation) != 0;

    if (indices.directions.empty())
    {
        return slot.get();
    }

    if (index.isUop)
    {
        index.groups.resize(indices.directions.size());
        for (size_t i = 0; i < index.groups.size(); ++i)
        {
            index.groups[i].uop = indices.directions[i];
        }
        return slot.get();
    }

    index.groups.resize(indices.directions.size() / MAX_DIRECTIONS);
    for (size_t i = 0; i < index.groups.size(); ++i)
    {
        for (int d = 0; d < MAX_DIRECTIONS; ++d)
        {
            const auto& src     = indices.directions[i * MAX_DIRECTIONS + d];
            auto& dst           = index.groups[i].directions[d];
            dst.address         = src.position;
            dst.size            = src.size;
            dst.isVerdata       = src.isVerdata;
        }
    }

    return slot.get();
}

AnimationGroupsType AnimationCache::getAnimType(uint16_t graphic)
{
    // ClassicUO answers Monster (0) until the body's frames were requested once; resolving
    // the index here gives the real type from the first frame on.
    auto* index = getIndexAnim(graphic);
    return index ? index->type : AnimationGroupsType::Monster;
}

uint32_t AnimationCache::getAnimFlags(uint16_t graphic)
{
    auto* index = getIndexAnim(graphic);
    return index ? index->flags : 0;
}

void AnimationCache::convertBodyIfNeeded(uint16_t& graphic, bool isCorpse)
{
    auto* index = getIndexAnim(graphic);
    if (!index || index->fileIndex != 0 || index->isUop)
    {
        return;
    }

    uint16_t hue = 0;
    if (isCorpse)
    {
        _loader.replaceCorpse(graphic, hue);
    }
    else
    {
        _loader.replaceBody(graphic, hue);
    }
}

Frame AnimationCache::toFrame(FrameInfo&& info)
{
    Frame f;
    if (info.empty() || info.pixels.size() != size_t(info.width) * size_t(info.height))
    {
        return f;
    }

    f.centerX = info.centerX;
    f.centerY = info.centerY;
    f.width   = info.width;
    f.height  = info.height;
    f.pixels  = std::move(info.pixels);

    f.hitMask.assign((f.pixels.size() + 63) / 64, 0);
    for (size_t i = 0; i < f.pixels.size(); ++i)
    {
        if (f.pixels[i] != 0)
        {
            f.hitMask[i >> 6] |= uint64_t(1) << (i & 63);
        }
    }
    return f;
}

void AnimationCache::loadDirection(uint16_t id, uint8_t action, uint8_t dir, IndexAnimation& index, Group& group)
{
    if (index.isUop)
    {
        // One UOP block holds every direction of the action: decode it once for all five.
        auto all = _loader.readUopAnimationFramesAllDirections(id, action, index.type, index.fileIndex, group.uop);
        for (int d = 0; d < MAX_DIRECTIONS; ++d)
        {
            auto& target = group.directions[d];
            if (target.loaded)
            {
                continue;
            }
            target.frames.clear();
            target.frames.reserve(all[d].size());
            for (auto& info : all[d])
            {
                target.frames.push_back(toFrame(std::move(info)));
            }
            target.loaded = true;
        }
        return;
    }

    auto& target = group.directions[dir];
    AnimationDirectionIndex ff;
    ff.position = target.address;
    ff.size     = target.size;

    auto infos = _loader.readMulAnimationFrames(index.fileIndex, ff, target.isVerdata);
    target.frames.clear();
    target.frames.reserve(infos.size());
    for (auto& info : infos)
    {
        target.frames.push_back(toFrame(std::move(info)));
    }
    target.loaded = true;
}

AnimationCache::FramesResult AnimationCache::getAnimationFrames(uint16_t id, uint8_t action, uint8_t dir, bool isEquip,
                                                                bool isCorpse)
{
    FramesResult result;

    if (action >= MAX_ACTIONS || dir >= MAX_DIRECTIONS)
    {
        return result;
    }

    auto* index = getIndexAnim(id);
    if (!index)
    {
        return result;
    }

    uint16_t hue      = 0;
    const bool useUop = index->isUop;

    // Body.def / Corpse.def chains. ClassicUO loops until the chain ends; a cyclic table
    // would hang it, so the walk is bounded here.
    for (int guard = 0; !useUop && index->fileIndex == 0 && guard < 32; ++guard)
    {
        const bool replaced = isCorpse ? _loader.replaceCorpse(id, hue) : _loader.replaceBody(id, hue);
        if (!replaced)
        {
            break;
        }

        index = getIndexAnim(id);
        if (!index)
        {
            return result;
        }
    }

    index->hue   = hue;
    result.hue   = hue;
    result.isUop = useUop;

    if (useUop)
    {
        _loader.replaceUopGroup(id, action);
    }

    // When we are searching for an equipment item we must ignore any other animation
    // which is not equipment.
    if (isEquip)
    {
        const auto type = index->type;
        if (type != AnimationGroupsType::Equipment && type != AnimationGroupsType::Human)
        {
            return result;
        }
    }

    if (action >= index->groups.size())
    {
        return result;
    }

    auto& group = index->groups[action];
    auto& animDir = group.directions[dir];

    if (!useUop && animDir.address == 0xFFFFFFFFu)
    {
        return result;
    }

    if (!animDir.loaded)
    {
        loadDirection(id, action, dir, *index, group);
    }

    result.frames = std::span<Frame>(animDir.frames);
    return result;
}

bool AnimationCache::animationExists(uint16_t graphic, uint8_t group, bool isCorpse)
{
    if (group >= MAX_ACTIONS)
    {
        return false;
    }

    auto frames = getAnimationFrames(graphic, group, 0, false, isCorpse).frames;
    return !frames.empty() && !frames[0].empty();
}

AnimationCache::Dimensions AnimationCache::getAnimationDimensions(uint8_t animIndex, uint16_t graphic, uint8_t dir,
                                                                  uint8_t animGroup, bool isMounted, uint8_t frameIndex)
{
    dir &= 0x7F;
    bool mirror = false;
    getAnimDirection(dir, mirror);

    if (frameIndex == 0xFF)
    {
        frameIndex = animIndex;
    }

    auto frames = getAnimationFrames(graphic, animGroup, dir).frames;
    if (frameIndex < frames.size() && !frames[frameIndex].empty())
    {
        const auto& f = frames[frameIndex];
        return {f.centerX, f.centerY, f.width, f.height};
    }

    return {0, 0, 0, isMounted ? 100 : 60};
}

bool AnimationCache::pixelCheck(uint16_t animId, uint8_t group, uint8_t direction, int frame, int x, int y, bool isCorpse)
{
    auto frames = getAnimationFrames(animId, group, direction, false, isCorpse).frames;
    if (frames.empty() || frame < 0)
    {
        return false;
    }
    return frames[static_cast<size_t>(frame) % frames.size()].hitTest(x, y);
}

} // namespace uo::anim
