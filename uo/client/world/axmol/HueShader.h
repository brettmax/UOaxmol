// SPDX-License-Identifier: BSD-2-Clause
//
// GPU hueing shared by the world renderer and UI sprites. Owns the hue lookup
// texture (and the optional coloured-light table) and hands sprites a program
// state running uo_hue_fs.hlsl.
//
// Create once after the Director is up:
//   HueShader::instance().init(packHueTexture(installation.hues().buildHueTexture()));

#pragma once

#include <cstdint>
#include <span>

#include "uo/render/HueVector.h"

namespace ax
{
class Sprite;
class Texture2D;
}  // namespace ax

namespace uo::render
{

class HueShader
{
public:
    static HueShader& instance();

    // packed must be kHueTextureWidth x kHueTextureHeight (packHueTexture output).
    bool init(std::span<const std::uint32_t> packed);
    bool ready() const { return _hueTexture != nullptr; }

    // Releases the GPU textures (call before the Director shuts down).
    void shutdown();

    ax::Texture2D* hueTexture() const { return _hueTexture; }

    // Coloured light lookup (32 x 63) for the LIGHTS mode; hueTexture() when unset.
    void setLightTexture(ax::Texture2D* lights);
    ax::Texture2D* lightTexture() const;

    // Hues a sprite with the classic client's rules: hue 0 restores the
    // sprite's default program; partialHue recolours only grey pixels; gump
    // leaves near-black pixels unhued, as ClassicUO does for gumps. The hue
    // survives setTexture/setSpriteFrame on the same sprite.
    void applyHue(ax::Sprite* sprite, std::uint16_t hue, bool partialHue, bool gump = true);

    // Full control, e.g. text modes (SHADER_TEXT_HUE) or spectral hues.
    void applyHueVector(ax::Sprite* sprite, const HueVector& hue);

private:
    HueShader() = default;

    ax::Texture2D* _hueTexture   = nullptr;
    ax::Texture2D* _lightTexture = nullptr;
};

}  // namespace uo::render
