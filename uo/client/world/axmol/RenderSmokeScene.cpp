// SPDX-License-Identifier: BSD-2-Clause

#include "RenderSmokeScene.h"

#include <cmath>
#include <cstdio>
#include <map>

#include "axmol/2d/Sprite.h"
#include "axmol/base/Director.h"
#include "axmol/base/Utils.h"
#include "axmol/renderer/Texture2D.h"

#include "HueShader.h"
#include "WorldRenderer.h"
#include "uo/render/HueTexture.h"

using namespace ax;

namespace uo::render
{

namespace
{

constexpr int kMapSize = 48;

// Grey level g (0..31) as an opaque RGBA8 texel.
std::uint32_t grey(int g)
{
    const std::uint32_t v = static_cast<std::uint32_t>(g * 255 / 31);
    return v | (v << 8) | (v << 16) | 0xFF000000u;
}

Texture2D* makeTexture(const std::vector<std::uint32_t>& px, int w, int h)
{
    auto* t = new Texture2D();
    t->initWithData(px.data(), static_cast<ssize_t>(px.size() * 4), rhi::PixelFormat::RGBA8, w, h);
    t->setAliasTexParameters();
    return t;
}

struct SmokeMap final : IMapSource, ITileData
{
    int width() const override { return kMapSize; }
    int height() const override { return kMapSize; }

    bool land(int x, int y, LandCell& out) const override
    {
        if (x < 0 || y < 0 || x >= kMapSize || y >= kMapSize)
            return false;
        // A hill in the middle (stretched, lit by normals) on flat ground.
        const float d = std::hypot(x - 24.0f, y - 24.0f);
        out.graphic   = d < 9 ? 4 : 3;
        out.z         = static_cast<std::int8_t>(d < 9 ? (9 - d) * 3 : 0);
        return true;
    }

    void statics(int bx, int by, std::vector<StaticEntry>& out) const override
    {
        // One of each kind per block: plain, hued, partial-hued.
        const int x0 = bx * 8, y0 = by * 8;
        std::int8_t z = landZ(x0 + 3, y0 + 3);
        out.push_back({0x100, 3, 3, z, 0});
        out.push_back({0x101, 5, 2, landZ(x0 + 5, y0 + 2), 1});
        out.push_back({0x102, 2, 6, landZ(x0 + 2, y0 + 6), 2});
    }

    // Every land tile has a texmap, as in real data, so slopes stretch and flat ground stays art.
    LandTileData land(std::uint16_t) const override { return {0, 1}; }

    StaticTileData item(std::uint16_t g) const override
    {
        if (g == 0x102)
            return {assets::TF_PartialHue, 20};
        return {0, 20};
    }

    int itemCount() const override { return 0x10000; }
    bool hasTexmap(std::uint16_t texId) const override { return texId == 1; }
};

struct SmokeTextures final : ITextureSource
{
    Texture2D* landTex   = nullptr;
    Texture2D* texmapTex = nullptr;
    Texture2D* itemTex   = nullptr;
    Texture2D* partialTex = nullptr;

    SmokeTextures()
    {
        // 44x44 diamond, mid grey with a light rim.
        std::vector<std::uint32_t> land(44 * 44, 0);
        for (int y = 0; y < 44; ++y)
            for (int x = 0; x < 44; ++x)
            {
                int dx = std::abs(x * 2 - 43), dy = std::abs(y * 2 - 43);
                if (dx + dy <= 44)
                    land[y * 44 + x] = grey(dx + dy > 38 ? 26 : 14);
            }
        landTex = makeTexture(land, 44, 44);

        // 64x64 checker texmap.
        std::vector<std::uint32_t> tm(64 * 64);
        for (int y = 0; y < 64; ++y)
            for (int x = 0; x < 64; ++x)
                tm[y * 64 + x] = ((x / 8 + y / 8) & 1) ? 0xFF2F8F3Fu : 0xFF1F5F2Fu;
        texmapTex = makeTexture(tm, 64, 64);

        // 20x60 grey pillar with a gradient, so hues show their ramp.
        std::vector<std::uint32_t> item(20 * 60);
        for (int y = 0; y < 60; ++y)
            for (int x = 0; x < 20; ++x)
                item[y * 20 + x] = grey(8 + (x * 20) / 19);
        itemTex = makeTexture(item, 20, 60);

        // Same pillar with a coloured (non-grey) band that partial hues must leave alone.
        for (int y = 20; y < 30; ++y)
            for (int x = 0; x < 20; ++x)
                item[y * 20 + x] = 0xFF00C0FFu;
        partialTex = makeTexture(item, 20, 60);
    }

    ~SmokeTextures() override
    {
        AX_SAFE_RELEASE(landTex);
        AX_SAFE_RELEASE(texmapTex);
        AX_SAFE_RELEASE(itemTex);
        AX_SAFE_RELEASE(partialTex);
    }

    static bool fill(Texture2D* t, TextureRegion& out)
    {
        out = {t, t->getWidth(), t->getHeight(), 0, 0, t->getWidth(), t->getHeight()};
        return true;
    }

    bool landArt(std::uint16_t, TextureRegion& out) override { return fill(landTex, out); }
    bool texmap(std::uint16_t, TextureRegion& out) override { return fill(texmapTex, out); }
    bool itemArt(std::uint16_t g, TextureRegion& out) override { return fill(g == 0x102 ? partialTex : itemTex, out); }
};

}  // namespace

struct RenderSmokeScene::Data
{
    SmokeMap map;
    SmokeTextures textures;
    std::unique_ptr<WorldMap> world;
};

RenderSmokeScene::RenderSmokeScene(std::string screenshotPath) : _screenshotPath(std::move(screenshotPath)) {}
RenderSmokeScene::~RenderSmokeScene() = default;

std::vector<std::uint32_t> RenderSmokeScene::smokeHueTexture()
{
    std::vector<std::uint32_t> ramps(3000 * kHueRampWidth);
    for (int h = 0; h < 3000; ++h)
        for (int i = 0; i < kHueRampWidth; ++i)
        {
            std::uint32_t v = static_cast<std::uint32_t>(i * 255 / 31);
            // Hue 1 (index 0) red, hue 2 (index 1) blue, others grey.
            std::uint32_t px = h == 0 ? v : h == 1 ? (v << 16) : (v | (v << 8) | (v << 16));
            ramps[static_cast<std::size_t>(h) * kHueRampWidth + i] = px | 0xFF000000u;
        }
    return packHueTexture(ramps);
}

bool RenderSmokeScene::init()
{
    if (!Scene::init())
        return false;

    if (!HueShader::instance().ready() && !HueShader::instance().init(smokeHueTexture()))
    {
        std::fprintf(stderr, "render smoke: hue shader init failed\n");
        return false;
    }

    _data        = std::make_unique<Data>();
    _data->world = std::make_unique<WorldMap>(_data->map, _data->map);

    const Size size = _director->getVisibleSize();

    ViewParams view;
    view.maxTileX = kMapSize - 1;
    view.maxTileY = kMapSize - 1;
    view.offsetX  = (24 - 24) * 22 - static_cast<int>(size.width / 2);
    view.offsetY  = (24 + 24) * 22 - static_cast<int>(size.height / 2);
    _data->world->ensureLoaded(view);
    _data->world->buildDrawList(view, _drawList);

    auto* renderer = WorldRenderer::create(_data->map, _data->textures);
    if (!renderer)
    {
        std::fprintf(stderr, "render smoke: WorldRenderer::create failed (shaders missing?)\n");
        return false;
    }
    renderer->setContentSize(size);
    renderer->setPosition(_director->getVisibleOrigin());
    renderer->setBrightlight(1.5f);
    renderer->setDrawList(&_drawList);
    addChild(renderer);

    // A plain sprite hued through HueShader::applyHue, as gumps will be.
    auto* sprite = Sprite::createWithTexture(_data->textures.itemTex);
    sprite->setPosition(_director->getVisibleOrigin() + Vec2(40, size.height - 60));
    HueShader::instance().applyHue(sprite, 2, false);
    addChild(sprite, 1);

    std::printf("render smoke: %zu draw items\n", _drawList.size());
    scheduleUpdate();
    return true;
}

void RenderSmokeScene::update(float)
{
    if (++_frames < 10 || _captured)
        return;

    _captured = true;
    utils::captureScreen(
        [this](bool ok, std::string_view path) {
            std::printf("render smoke: screenshot %s -> %.*s\n", ok ? "saved" : "FAILED", static_cast<int>(path.size()),
                        path.data());
            std::fflush(stdout);
            _director->end();
        },
        _screenshotPath);
}

}  // namespace uo::render
