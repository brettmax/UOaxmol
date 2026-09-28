// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.Renderer/ShaderHueTranslator.cs, LandView.cs).

#include "uo/render/HueVector.h"

namespace uo::render
{

HueVector makeHueVector(int hue, bool partial, float alpha, bool gump, bool effect, bool circleTrans)
{
    uint8_t type;

    if ((hue & kPartialHueFlag) != 0)
    {
        partial = true;
        hue &= 0x7FFF;
    }

    if (hue == 0)
    {
        partial = false;
    }

    if ((hue & kSpectralColorFlag) != 0)
    {
        type = SHADER_SPECTRAL;
    }
    else if (hue != 0)
    {
        hue -= 1;

        // The effect mode reads the hue index from the G channel instead of R,
        // so partial hueing wins over it (same as ClassicUO).
        type = effect && !partial ? SHADER_EFFECT_HUED : partial ? SHADER_PARTIAL_HUED : SHADER_HUED;

        if (gump && !effect)
        {
            type += kGumpShaderOffset;
        }
    }
    else
    {
        type = SHADER_NONE;
    }

    HueVector v;
    v.hue   = static_cast<float>(hue);
    v.mode  = static_cast<float>(type);
    v.alpha = circleTrans ? alpha + 1.0f : alpha;
    return v;
}

HueVector makeLandHueVector(uint16_t hue, bool stretched)
{
    HueVector v;

    if (hue != 0)
    {
        v.hue  = static_cast<float>(hue - 1);
        v.mode = stretched ? SHADER_LAND_HUED : SHADER_HUED;
    }
    else
    {
        v.hue  = 0;
        v.mode = stretched ? SHADER_LAND : SHADER_NONE;
    }

    v.alpha = 1.0f;
    return v;
}

}  // namespace uo::render
