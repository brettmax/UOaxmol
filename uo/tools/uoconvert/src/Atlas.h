// SPDX-License-Identifier: BSD-2-Clause
// Sprite-sheet packing and output in the formats Axmol loads natively.
#pragma once

#include "uo/assets/Image.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace uoconvert
{

struct Sprite
{
    Sprite() = default;
    Sprite(std::uint32_t id_, int w_, int h_, std::string name_) : id(id_), w(w_), h(h_), name(std::move(name_)) {}

    std::uint32_t id = 0;
    int w = 0;
    int h = 0;
    std::string name;          // SpriteFrameCache frame name, e.g. "static/0x0EED"
    bool hasAnchor = false;    // written as the format-3 "anchor" key when set
    float ax = 0, ay = 0;
    std::string extraJson;     // extra members for this frame's index entry, e.g. "\"hue\":3"
};

struct Placement
{
    std::size_t sprite = 0;  // index into the sprite list
    int x = 0;
    int y = 0;
};

struct Page
{
    int w = 0;
    int h = 0;
    std::vector<Placement> placements;
};

struct AtlasOptions
{
    int maxSize = 2048;       // page edge; sprites larger than this get a page to themselves
    int padding = 1;          // transparent gap between sprites
    int extrude = 0;          // edge pixels repeated around each sprite (for stretched textures)
    int jobs    = 1;          // pages rendered in parallel
    std::string indexExtraJson;  // extra top-level members for <base>.json, e.g. "\"aliases\":{}"
};

struct AtlasResult
{
    std::size_t frames = 0;
    std::size_t sheets = 0;
    std::vector<std::uint32_t> missing;  // planned but failed to decode
};

// Must be safe to call from several threads at once (uocore's readers are: they only read
// memory-mapped files).
using Decoder = std::function<uo::assets::Image(const Sprite&)>;

// Skyline bottom-left packing, tallest first. Page sizes are trimmed to the used area and
// rounded up to a multiple of 4 (so block-compressed formats such as ASTC 4x4 fit).
std::vector<Page> planPages(const std::vector<Sprite>& sprites, const AtlasOptions& options);

// Plans, decodes and writes <dir>/<base>-NNN.png + .plist (TexturePacker format 3, which
// ax::SpriteFrameCache::addSpriteFramesWithFile reads) and one <dir>/<base>.json index.
AtlasResult writeAtlas(const std::string& dir, const std::string& base, const std::vector<Sprite>& sprites,
                       const Decoder& decode, const AtlasOptions& options);

}  // namespace uoconvert
