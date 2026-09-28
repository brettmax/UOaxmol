// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.Renderer/ShaderHueTranslator.cs).
//
// Every world vertex carries a hue vector (hue index, shader mode, alpha) that
// uo_world_fs.hlsl turns into the final colour. The mode values must match the
// #defines at the top of that shader.

#pragma once

#include <cstdint>

namespace uo::render
{

enum ShaderMode : uint8_t
{
    SHADER_NONE              = 0,
    SHADER_HUED              = 1,
    SHADER_PARTIAL_HUED      = 2,
    SHADER_TEXT_HUE_NO_BLACK = 3,
    SHADER_TEXT_HUE          = 4,
    SHADER_LAND              = 5,
    SHADER_LAND_HUED         = 6,
    SHADER_SPECTRAL          = 7,
    SHADER_SHADOW            = 8,
    SHADER_LIGHTS            = 9,
    SHADER_EFFECT_HUED       = 10,
};

inline constexpr uint8_t kGumpShaderOffset    = 20;
inline constexpr uint16_t kSpectralColorFlag  = 0x4000;
inline constexpr uint16_t kPartialHueFlag     = 0x8000;

struct HueVector
{
    float hue   = 0;  // zero-based hue index into the hue texture
    float mode  = 0;  // ShaderMode (+ kGumpShaderOffset for gumps)
    float alpha = 1;  // 0..1; +1 marks the pixel as subject to the circle of transparency

    bool operator==(const HueVector&) const = default;
};

// hue is the 1-based UO hue (0 = none). 0x8000 forces partial hueing, 0x4000
// selects the spectral (ghost) shader.
HueVector makeHueVector(int hue,
                        bool partial,
                        float alpha,
                        bool gump         = false,
                        bool effect       = false,
                        bool circleTrans  = false);

inline HueVector makeHueVector(int hue)
{
    return makeHueVector(hue, false, 1.0f);
}

// Land uses its own modes: stretched tiles are lit by their normals.
HueVector makeLandHueVector(uint16_t hue, bool stretched);

}  // namespace uo::render
