#include "common.hlsli"

float2 main(VertexOut pin) : SV_Target
{
    float depth = gShadowMap.Sample(gsamLinearClamp, pin.tex).r;
    
    return float2(depth, depth * depth);
}