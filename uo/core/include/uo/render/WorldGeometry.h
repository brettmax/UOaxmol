// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.Renderer/Batcher2D.cs: Draw, DrawStretchedLand,
// DrawShadow).
//
// Turns a sorted draw list into quads for uo_world_vs/fs. Engine-free: textures
// are opaque handles supplied by ITextureSource, so this is unit-tested without
// a GPU and the Axmol node only uploads the result.

#pragma once

#include <cstdint>
#include <vector>

#include "uo/render/WorldMap.h"

namespace uo::render
{

// Must match VS_IN in uo_world_vs.hlsl and the layout WorldRenderer declares.
struct WorldVertex
{
    float x, y, z;
    float nx, ny, nz;
    float u, v;
    float hue, mode, alpha;
};
static_assert(sizeof(WorldVertex) == 44);

// A sub-rectangle of a texture (atlas page or standalone texture).
struct TextureRegion
{
    const void* texture = nullptr;  // opaque handle, e.g. ax::Texture2D*
    int textureWidth    = 0;
    int textureHeight   = 0;
    int x = 0, y = 0, width = 0, height = 0;
};

class ITextureSource
{
public:
    virtual ~ITextureSource() = default;

    // 44x44 land art for a land graphic.
    virtual bool landArt(uint16_t graphic, TextureRegion& out) = 0;
    // Texmap (64x64 or 128x128) for a land tile's texId.
    virtual bool texmap(uint16_t texId, TextureRegion& out) = 0;
    // Item art for a static/item graphic. Implementations resolve animated art
    // (the art index AnimOffset) themselves.
    virtual bool itemArt(uint16_t graphic, TextureRegion& out) = 0;
    // Texture for a decoded animation frame (AnimFrame/AnimShadow items). Sources that
    // draw no animations keep the default.
    virtual bool animFrame(const anim::Frame& frame, TextureRegion& out)
    {
        (void)frame;
        (void)out;
        return false;
    }
};

// Consecutive quads that share a texture; one draw call each.
struct DrawBatch
{
    const void* texture = nullptr;
    uint32_t firstIndex = 0;
    uint32_t indexCount = 0;
};

struct WorldGeometry
{
    std::vector<WorldVertex> vertices;
    std::vector<uint32_t> indices;
    std::vector<DrawBatch> batches;

    void clear()
    {
        vertices.clear();
        indices.clear();
        batches.clear();
    }
};

// Screen coordinates stay y-down in world pixels relative to the view offset;
// the renderer's projection flips them. Items whose texture is missing are
// skipped. `out` is cleared first.
void buildWorldGeometry(const std::vector<DrawItem>& items,
                        const ITileData& tiles,
                        ITextureSource& textures,
                        WorldGeometry& out);

}  // namespace uo::render
