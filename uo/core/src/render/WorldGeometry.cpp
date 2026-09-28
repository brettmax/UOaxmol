// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.Renderer/Batcher2D.cs).

#include "uo/render/WorldGeometry.h"

namespace uo::render
{

namespace
{

struct UvRect
{
    float x, y, w, h;
};

// Batcher2D.CalculateHalfPixelUVs: sample texel centres so neighbouring atlas
// entries never bleed in.
UvRect halfPixelUvs(const TextureRegion& r)
{
    const float invW = 1.0f / static_cast<float>(r.textureWidth);
    const float invH = 1.0f / static_cast<float>(r.textureHeight);
    return {(r.x + 0.5f) * invW, (r.y + 0.5f) * invH, (r.width - 1.0f) * invW, (r.height - 1.0f) * invH};
}

// Corner order 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right (the
// batcher's _cornerOffsetX/Y), triangles 0-1-2 and 1-3-2.
constexpr float kCornerU[4] = {0, 1, 0, 1};
constexpr float kCornerV[4] = {0, 0, 1, 1};

class Emitter
{
public:
    explicit Emitter(WorldGeometry& g) : _g(g) {}

    void quad(const void* texture, const float px[4], const float py[4], const UvRect& uv, const Vec3 normals[4], const HueVector& hue)
    {
        const uint32_t base = static_cast<uint32_t>(_g.vertices.size());

        for (int i = 0; i < 4; ++i)
        {
            WorldVertex v;
            v.x     = px[i];
            v.y     = py[i];
            v.z     = 0;
            v.nx    = normals[i].x;
            v.ny    = normals[i].y;
            v.nz    = normals[i].z;
            v.u     = kCornerU[i] * uv.w + uv.x;
            v.v     = kCornerV[i] * uv.h + uv.y;
            v.hue   = hue.hue;
            v.mode  = hue.mode;
            v.alpha = hue.alpha;
            _g.vertices.push_back(v);
        }

        const uint32_t first = static_cast<uint32_t>(_g.indices.size());
        const uint32_t idx[6] = {base, base + 1, base + 2, base + 1, base + 3, base + 2};
        _g.indices.insert(_g.indices.end(), idx, idx + 6);

        if (_g.batches.empty() || _g.batches.back().texture != texture)
        {
            _g.batches.push_back({texture, first, 0});
        }

        _g.batches.back().indexCount += 6;
    }

private:
    WorldGeometry& _g;
};

constexpr Vec3 kUp{0, 0, 1};
const Vec3 kFlatNormals[4] = {kUp, kUp, kUp, kUp};

void sprite(Emitter& e, const TextureRegion& r, float x, float y, const HueVector& hue)
{
    const float w     = static_cast<float>(r.width);
    const float h     = static_cast<float>(r.height);
    const float px[4] = {x, x + w, x, x + w};
    const float py[4] = {y, y, y + h, y + h};
    e.quad(r.texture, px, py, halfPixelUvs(r), kFlatNormals, hue);
}

}  // namespace

void buildWorldGeometry(const std::vector<DrawItem>& items,
                        const ITileData& tiles,
                        ITextureSource& textures,
                        WorldGeometry& out)
{
    out.clear();
    out.vertices.reserve(items.size() * 4);
    out.indices.reserve(items.size() * 6);

    Emitter e(out);
    TextureRegion r;

    for (const DrawItem& item : items)
    {
        const float sx = static_cast<float>(item.screenX);
        const float sy = static_cast<float>(item.screenY);

        switch (item.type)
        {
        case DrawType::LandFlat:
            if (textures.landArt(item.graphic, r))
            {
                sprite(e, r, sx, sy, item.hue);
            }
            break;

        case DrawType::LandStretched:
        {
            const LandStretch& land = item.object->land;

            if (!textures.texmap(tiles.land(item.graphic).texId, r))
            {
                // LandView falls back to the art tile when the texmap is missing.
                if (textures.landArt(item.graphic, r))
                {
                    sprite(e, r, sx, sy - (item.object->z << 2), item.hue);
                }
                break;
            }

            // Batcher2D.DrawStretchedLand: top, right, left, bottom corners.
            const YOffsets& o = land.offsets;
            const float px[4] = {sx + 22, sx + 44, sx, sx + 22};
            const float py[4] = {sy - o.top, sy + (22 - o.right), sy + (22 - o.left), sy + (44 - o.bottom)};
            const Vec3 n[4]   = {land.normalTop, land.normalRight, land.normalLeft, land.normalBottom};
            e.quad(r.texture, px, py, halfPixelUvs(r), n, item.hue);
            break;
        }

        case DrawType::Static:
            if (textures.itemArt(item.graphic, r))
            {
                int x, y;
                staticDrawOrigin(item.screenX, item.screenY, r.width, r.height, x, y);
                sprite(e, r, static_cast<float>(x), static_cast<float>(y), item.hue);
            }
            break;

        case DrawType::Mobile:
        {
            int cx = 0, cy = 0;
            bool mirrored = false;
            if (!textures.mobileFrame(*item.object, r, cx, cy, mirrored))
            {
                break;
            }

            // MobileView.DrawInternal: frames hang from the tile centre by their Center.
            const float w  = static_cast<float>(r.width);
            const float h  = static_cast<float>(r.height);
            const float x  = mirrored ? sx + 22 - (w - cx) : sx + 22 - cx;
            const float y  = sy + 22 - h - cy;
            const float px[4] = {x, x + w, x, x + w};
            const float py[4] = {y, y, y + h, y + h};
            UvRect uv = halfPixelUvs(r);
            if (mirrored)
            {
                uv.x += uv.w;
                uv.w = -uv.w;
            }
            e.quad(r.texture, px, py, uv, kFlatNormals, item.hue);
            break;
        }

        case DrawType::Shadow:
            if (textures.itemArt(item.graphic, r))
            {
                int x, y;
                staticDrawOrigin(item.screenX, item.screenY, r.width, r.height, x, y);

                // Batcher2D.DrawShadow (unflipped): a sheared, half-height copy.
                const float width      = static_cast<float>(r.width);
                const float height     = r.height * 0.5f;
                const float translated = y + height - 10;
                const float ratio      = height / width;
                const float px[4]      = {x + width * ratio, x + width * (ratio + 1.0f), static_cast<float>(x), x + width};
                const float py[4]      = {translated, translated, translated + height, translated + height};
                e.quad(r.texture, px, py, halfPixelUvs(r), kFlatNormals, item.hue);
            }
            break;
        }
    }
}

}  // namespace uo::render
