// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.Renderer/shaders/IsometricWorld.fx).
//
// v_hue = (hue index, mode, alpha). Modes match uo::render::ShaderMode
// (uo/render/HueVector.h); modes >= GUMP are gump draws that leave near-black pixels
// unhued. alpha > 1 marks a pixel the circle of transparency may cut.

#include "base.hlsli"

#define NONE 0
#define HUED 1
#define PARTIAL_HUED 2
#define HUE_TEXT_NO_BLACK 3
#define HUE_TEXT 4
#define LAND 5
#define LAND_COLOR 6
#define SPECTRAL 7
#define SHADOW 8
#define LIGHTS 9
#define EFFECT_HUED 10
#define GUMP 20

// Hue texture: 16 hues per row, 32 texels per hue, 1024 rows (HueTexture.h).
#define HUE_WIDTH 32.0
#define HUE_COLUMNS 16.0
#define HUE_ROWS 1024.0
#define LIGHT_ROWS 63.0

static const float3 LIGHT_DIRECTION = float3(0.0, 1.0, 1.0);

struct PS_IN {
    float2 v_texCoord : TEXCOORD0;
    float3 v_normal : TEXCOORD1;
    float3 v_hue : TEXCOORD2;
    float2 v_world : TEXCOORD3;  // node pixels, y-up
};

Texture2D u_tex0;
Texture2D u_hueTex;
Texture2D u_lightTex;

cbuffer fs_ub {
    float2 u_cotCenter;     // circle of transparency centre, node pixels (y-up)
    float u_brightlight;    // terrain shadows level * 0.1
    float u_cotRadius;      // pixels; 0 disables the circle
};

float3 get_rgb(float gray, float hue)
{
    float column = fmod(hue, HUE_COLUMNS);
    float row = fmod(floor(hue / HUE_COLUMNS), HUE_ROWS);
    float texel = clamp(gray * HUE_WIDTH, 0.5, HUE_WIDTH - 0.5);
    float2 uv = float2((column * HUE_WIDTH + texel) / (HUE_WIDTH * HUE_COLUMNS), (row + 0.5) / HUE_ROWS);
    return u_hueTex.Sample(PointClamp, uv).rgb;
}

float get_light(float3 norm)
{
    float3 light = normalize(LIGHT_DIRECTION);
    float3 normal = normalize(norm);
    float base = (max(dot(normal, light), 0.0) / 2.0) + 0.5;

    // At 45 degrees (the angle flat tiles are lit at) this is
    // (cos(45) / 2) + 0.5 = 0.85355339.
    return base + ((u_brightlight * (base - 0.85355339)) - (base - 0.85355339));
}

float3 get_colored_light(float shader, float gray)
{
    float2 uv = float2(gray, (shader - 0.5) / LIGHT_ROWS);
    return u_lightTex.Sample(PointClamp, uv).rgb;
}

float4 main(PS_IN input) : SV_Target0
{
    float4 color = u_tex0.Sample(PointClamp, input.v_texCoord);

    // Every cut sets `kill` and one discard runs at the end: with early discards the D3D11
    // backend's compiler (d3dcompiler_47) fails on this shader with "internal error: argument
    // pulled into unrelated predicate", and the world does not draw at all.
    bool kill = color.a == 0.0;

    int mode = int(input.v_hue.y + 0.5);
    float alpha = input.v_hue.z;
    bool useTrans = false;

    if (alpha > 1.0)
    {
        useTrans = true;
        alpha -= 1.0;
    }

    if (alpha == 0.0)
        kill = true;

    float hue = input.v_hue.x;

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
    else if (mode == LAND)
    {
        color.rgb *= get_light(input.v_normal);
    }
    else if (mode == LAND_COLOR)
    {
        color.rgb = get_rgb(color.r, hue) * get_light(input.v_normal);
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
    else if (mode == LIGHTS)
    {
        color.rgb = get_colored_light(input.v_hue.x - 1.0, color.r);
    }
    else if (mode == EFFECT_HUED)
    {
        color.rgb = get_rgb(color.g, hue);
    }

    if (useTrans && u_cotRadius > 0.0)
    {
        // ClassicUO measures from the screen centre in NDC; measuring in node
        // pixels lets the circle follow the player anywhere in the viewport.
        float ratio = length(input.v_world - u_cotCenter) / u_cotRadius;

        if (ratio < 0.85)
        {
            kill = true;
        }
        else if (ratio < 1.0)
        {
            float t = (ratio - 0.85) / 0.15;
            alpha *= t * t * t;

            if (alpha < 0.02)
                kill = true;
        }
    }

    if (kill)
        discard;

    // Premultiplied output; draw with ONE, ONE_MINUS_SRC_ALPHA.
    return color * alpha;
}
