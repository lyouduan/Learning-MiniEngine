#include "common.hlsli"

VertexOut main(VertexIn vin)
{
    VertexOut vout;
    float4 posW = mul(float4(vin.position, 1.0), objConstants.gWorld);
    vout.positionW = posW.xyz;
    
    vout.positionH = mul(posW, mul(passConstants.gView, passConstants.gProj));
    
    // Assumes nonuniform scaling; otherwise, need to use inverse-transpose of world matrix.
    vout.normal = mul(vin.normal, (float3x3) objConstants.gWorld);
    
    vout.depth = vout.positionH.z / vout.positionH.w;
    
    return vout;
}