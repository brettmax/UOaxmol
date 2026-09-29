// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (Game/SelectedObject.cs, GameObjects/*.CheckMouseSelection).
//
// Screen-point picking over a built draw list: the last item drawn under the
// point wins, as in ClassicUO where every object that passes its mouse check
// during the draw pass overwrites SelectedObject.
//
// - Flat land: the point must fall inside the 44x44 diamond.
// - Stretched land: the point must fall inside the stretched quad.
// - Statics, items and multi components: the art pixel under the point must be
//   opaque. Multi components carry serial 0 (their house is `owner`), so a click
//   on a wall acts on the tile like a static; placement previews are skipped.
// - Shadows and fully transparent objects are never picked.
// - An object the circle of transparency cuts through is not picked at
//   points inside the circle (the shader discards those pixels).

#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

#include "uo/render/WorldMap.h"

namespace uo::assets
{
class Art;
struct Image;
}  // namespace uo::assets

namespace uo::render
{

// Pixel access for item art hit tests.
class IArtHitTest
{
public:
    virtual ~IArtHitTest() = default;

    // Size of the item art for `graphic`; false when it has none.
    virtual bool itemSize(uint16_t graphic, int& width, int& height) = 0;

    // True when the art pixel at (x, y), relative to the art's top-left, is opaque.
    virtual bool itemOpaque(uint16_t graphic, int x, int y) = 0;
};

// IArtHitTest over art.mul, caching decoded art (ClassicUO's ArtLoader.PixelCheck).
class ArtHitTest final : public IArtHitTest
{
public:
    explicit ArtHitTest(const assets::Art& art);
    ~ArtHitTest() override;

    bool itemSize(uint16_t graphic, int& width, int& height) override;
    bool itemOpaque(uint16_t graphic, int x, int y) override;

private:
    const assets::Image* image(uint16_t graphic);

    const assets::Art& _art;
    std::unordered_map<uint16_t, std::unique_ptr<assets::Image>> _cache;
};

// Circle of transparency as the shader applies it, in the same screen space
// as the draw list (y down, view offset already subtracted).
struct PickCircle
{
    int centerX = 0;
    int centerY = 0;
    int radius  = 0;  // 0 disables
};

// The topmost draw item under (x, y), in draw-list screen space, or nullptr.
// Its `object` carries kind, graphic, tile position, z and serial (0 for land
// and statics). The pointer is valid until the draw list is rebuilt.
const DrawItem* pick(const std::vector<DrawItem>& drawList,
                     const ITileData& tiles,
                     IArtHitTest& art,
                     int x,
                     int y,
                     const PickCircle& circle = {});

// Hit test for one draw item (exposed for tests and custom pick loops).
bool hitTest(const DrawItem& item, const ITileData& tiles, IArtHitTest& art, int x, int y, const PickCircle& circle = {});

}  // namespace uo::render
