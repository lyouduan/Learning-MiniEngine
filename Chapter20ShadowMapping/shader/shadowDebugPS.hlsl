#include "common.hlsli"

float4 main(VertexOut pin) : SV_Target
{
    return float4(gShadowMap.Sample(gsamLinearClamp, pin.tex).ggg, 1.0f);
}