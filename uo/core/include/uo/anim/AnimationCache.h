// SPDX-License-Identifier: BSD-2-Clause
// Part of the AxmolUO client core. Ported from ClassicUO.Renderer (Animations), minus the GPU
// atlas: this layer resolves body/action/direction to decoded frames and caches them.
// The Axmol side (uo/client/anim) turns frames into textures keyed by Frame address,
// which stays stable for the cache's lifetime.
#pragma once

#include "uo/anim/AnimationsLoader.h"

#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace uo::anim
{

struct Frame
{
    int16_t centerX = 0;
    int16_t centerY = 0;
    int16_t width   = 0;
    int16_t height  = 0;
    // RGBA8888, R in the low byte. The renderer may release these after upload; hit
    // testing uses hitMask, which is kept.
    std::vector<uint32_t> pixels;
    // One bit per pixel, row-major: set where the pixel is not transparent (PixelPicker).
    std::vector<uint64_t> hitMask;

    bool empty() const { return width <= 0 || height <= 0; }
    bool hitTest(int x, int y) const;
};

class AnimationCache
{
public:
    // Bodies are 16-bit; 0xFFFF is the "no graphic" sentinel.
    static constexpr int MAX_ANIMATION_COUNT = 0xFFFF;

    explicit AnimationCache(AnimationsLoader& loader);

    AnimationsLoader& loader() { return _loader; }
    const AnimationsLoader& loader() const { return _loader; }

    int maxAnimationCount() const { return MAX_ANIMATION_COUNT; }

    AnimationGroupsType getAnimType(uint16_t graphic);
    uint32_t getAnimFlags(uint16_t graphic);

    struct FramesResult
    {
        std::span<Frame> frames;
        uint16_t hue = 0; // hue from Body.def/Corpse.def when the body was swapped
        bool isUop   = false;
    };

    // Frames for one body/action/direction (direction already folded to 0..4).
    FramesResult getAnimationFrames(uint16_t id, uint8_t action, uint8_t dir, bool isEquip = false,
                                    bool isCorpse = false);

    bool animationExists(uint16_t graphic, uint8_t group, bool isCorpse = false);

    // Body.def / Corpse.def swap for MUL bodies in anim.mul.
    void convertBodyIfNeeded(uint16_t& graphic, bool isCorpse = false);

    struct Dimensions
    {
        int centerX = 0;
        int centerY = 0;
        int width   = 0;
        int height  = 0;
    };
    Dimensions getAnimationDimensions(uint8_t animIndex, uint16_t graphic, uint8_t dir, uint8_t animGroup,
                                      bool isMounted, uint8_t frameIndex);

    // Frame-local hit test: (x, y) relative to the frame's top-left, unmirrored.
    bool pixelCheck(uint16_t animId, uint8_t group, uint8_t direction, int frame, int x, int y, bool isCorpse = false);

    // Applies Bodyconv.def with the expansion flags from the server (packet 0xB9) and drops
    // everything resolved so far, since body conversions change which file a body reads.
    void updateAnimationTable(uint32_t bodyConvFlags);

    // Drops every decoded frame and resolved index. Invalidates Frame pointers.
    void clear();

    // Bumped whenever Frame pointers are invalidated; texture caches keyed on Frame
    // addresses compare it to know when to flush.
    uint32_t generation() const { return _generation; }

    // Folds the 8 world directions onto the 5 stored ones. `mirror` is only written for
    // directions that need it, exactly like ClassicUO, so callers keep it per object.
    static void getAnimDirection(uint8_t& dir, bool& mirror);

private:
    struct Direction
    {
        uint32_t address = 0;
        uint32_t size    = 0;
        bool isVerdata   = false;
        bool loaded      = false;
        std::vector<Frame> frames;
    };

    struct Group
    {
        std::array<Direction, MAX_DIRECTIONS> directions;
        // UOP actions store all directions in one block.
        AnimationDirectionIndex uop;
    };

    struct IndexAnimation
    {
        int fileIndex            = 0;
        uint16_t hue             = 0;
        uint32_t flags           = 0;
        AnimationGroupsType type = AnimationGroupsType::Unknown;
        bool isUop               = false;
        std::vector<Group> groups;
    };

    IndexAnimation* getIndexAnim(uint16_t id);
    void loadDirection(uint16_t id, uint8_t action, uint8_t dir, IndexAnimation& index, Group& group);
    static Frame toFrame(FrameInfo&& info);

    AnimationsLoader& _loader;
    std::vector<std::unique_ptr<IndexAnimation>> _dataIndex;
    uint32_t _generation = 1;
};

} // namespace uo::anim
