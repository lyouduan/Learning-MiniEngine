#include "common.hlsli"

VertexOut main(VertexIn vin)
{
    VertexOut vout;
    
    vout.positionW = vin.position;
    
    float4 posW = mul(float4(vin.position, 1.0), objConstants.gWorld);
    
    // 以相机为skybox中心
    posW.xyz += passConstants.gEyePosW;
    
    vout.positionH = mul(posW, passConstants.gViewProj);
    
    // reversed-z: far plane = 0.0f
    vout.positionH.z = 0;
    
    
    return vout;
}