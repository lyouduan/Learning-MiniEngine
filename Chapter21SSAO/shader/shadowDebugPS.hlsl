#include "common.hlsli"

float4 main(VertexOut pin) : SV_Target
{
    return float4(gShadowMap.SampleLevel(gsamLinearClamp, pin.tex, 1.0).rrr, 1.0f);
}