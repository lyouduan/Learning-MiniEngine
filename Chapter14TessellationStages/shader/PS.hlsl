#include "CS.hlsli"
#include "LightingUtil.hlsli"

Texture2D gDiffuseMap : register(t0);

SamplerState gsamLinearClamp : register(s0);

float4 main(VertexOut input) : SV_TARGET
{
    return float4(1.0, 1.0, 1.0, 1.0);
}