#include "CS.hlsli"

VertexOut main(VertexIn input) 
{
    VertexOut output;
    
    float4 posW = mul(float4(input.position, 1.0), objConstants.gWorld);
    output.positionW = posW.xyz;
    output.positionH = mul(posW, passConstants.gViewProj);
    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    output.normal = mul(input.normal, (float3x3)objConstants.gWorld);
    
    
    return output;
}

