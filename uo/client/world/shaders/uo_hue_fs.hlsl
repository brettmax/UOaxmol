// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.Renderer/shaders/IsometricWorld.fx).
//
// Sprite variant of uo_world_fs: pairs with Axmol's positionTextureColor_vs so
// any ax::Sprite (gumps, text, paperdoll) can be hued on the GPU. u_hue is a
// uo::render::HueVector (hue index, mode, alpha). Output is straight alpha,
// matching Sprite's default blend for non-premultiplied textures.

#include "base.hlsli"

#define NONE 0
#define HUED 1
#define PARTIAL_HUED 2
#define HUE_TEXT_NO_BLACK 3
#define HUE_TEXT 4
#define SPECTRAL 7
#define SHADOW 8
#define EFFECT_HUED 10
#define GUMP 20

#define HUE_WIDTH 32.0
#define HUE_COLUMNS 16.0
#define HUE_ROWS 1024.0

struct PS_IN {
    float4 v_color : COLOR0;
    float2 v_texCoord : TEXCOORD0;
};

Texture2D u_tex0;
Texture2D u_hueTex;

cbuffer fs_ub {
    float4 u_hue;  // xyz = hue index, mode, alpha
};

float3 get_rgb(float gray, float hue)
{
    float column = fmod(hue, HUE_COLUMNS);
    float row = fmod(floor(hue / HUE_COLUMNS), HUE_ROWS);
    float texel = clamp(gray * HUE_WIDTH, 0.5, HUE_WIDTH - 0.5);
    float2 uv = float2((column * HUE_WIDTH + texel) / (HUE_WIDTH * HUE_COLUMNS), (row + 0.5) / HUE_ROWS);
    return u_hueTex.Sample(PointClamp, uv).rgb;
}

float4 main(PS_IN input) : SV_Target0
{
    float4 color = u_tex0.Sample(PointClamp, input.v_texCoord);

    if (color.a == 0.0)
        discard;

    int mode = int(u_hue.y + 0.5);
    float alpha = u_hue.z;
    float hue = u_hue.x;

    if (mode >= GUMP)
    {
        mode -= GUMP;

        if (color.r < 0.02)
            hue = 0.0;
    }

    if (mode == HUED || (mode == PARTIAL_HUED && color.r == color.g && color.r == color.b))
    {
        color.rgb = get_rgb(color.r, hue);
    }
    else if (mode == HUE_TEXT_NO_BLACK)
    {
        if (color.r > 0.04 || color.g > 0.04 || color.b > 0.04)
            color.rgb = get_rgb(1.0, hue);
    }
    else if (mode == HUE_TEXT)
    {
        color.rgb = get_rgb(1.0, hue);
    }
    else if (mode == SPECTRAL)
    {
        alpha = 1.0 - (color.r * 1.5);
        color.rgb = float3(0.0, 0.0, 0.0);
    }
    else if (mode == SHADOW)
    {
        alpha = 0.4;
        color.rgb = float3(0.0, 0.0, 0.0);
    }
    else if (mode == EFFECT_HUED)
    {
        color.rgb = get_rgb(color.g, hue);
    }

    color.a *= alpha;
    return color * input.v_color;
}
