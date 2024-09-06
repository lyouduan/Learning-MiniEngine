#include "common.hlsli"

VertexOut main(VertexIn input) 
{
    VertexOut output;
    
    float4 posW = mul(float4(input.position, 1.0), objConstants.gWorld);
    output.positionW = posW.xyz;
    output.positionH = mul(posW, passConstants.gViewProj);
    output.normal = input.normal;
    output.tex = input.tex;
    
    return output;
}

