// SPDX-License-Identifier: BSD-2-Clause
// Ported from ClassicUO (ClassicUO.Renderer/shaders/IsometricWorld.fx).
// Vertex layout: uo::render::WorldVertex (uo/render/WorldGeometry.h).

#include "base.hlsli"

struct VS_IN {
    float3 a_position : POSITION;
    float3 a_normal : NORMAL;
    float2 a_texCoord : TEXCOORD0;
    float3 a_hue : TEXCOORD1;
};

struct VS_OUT {
    float2 v_texCoord : TEXCOORD0;
    float3 v_normal : TEXCOORD1;
    float3 v_hue : TEXCOORD2;
    float2 v_world : TEXCOORD3;
    float4 position : SV_Position;
};

cbuffer vs_ub {
    float4x4 u_MVPMatrix;
};

VS_OUT main(VS_IN input) {
    VS_OUT output;
    output.position = mul(u_MVPMatrix, float4(input.a_position, 1.0));
    output.v_texCoord = input.a_texCoord;
    output.v_normal = input.a_normal;
    output.v_hue = input.a_hue;
    output.v_world = input.a_position.xy;
    return output;
}
