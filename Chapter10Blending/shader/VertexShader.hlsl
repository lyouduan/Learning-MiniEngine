#include "common.hlsli"

VertexOut main(VertexIn input) 
{
    VertexOut output;
    
    float4 posW = mul(float4(input.position, 1.0), objConstants.gWorld);
    output.positionW = posW.xyz;
    output.positionH = mul(posW, passConstants.gViewProj);
    output.normal = input.normal;
    float4 tex = mul(float4(input.tex, 0.0, 1.0), objConstants.gTexTransform);
    output.tex = mul(tex, matConstants.gMatTransform).xy;
    
    return output;
}

