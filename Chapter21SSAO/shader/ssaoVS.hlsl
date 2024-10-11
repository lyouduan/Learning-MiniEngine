#include "ssaoCommon.hlsli"

static const float2 gTexCoords[4] =
{
    float2(0.0, 1.0),
    float2(1.0, 1.0),
    float2(1.0, 0.0),
    float2(0.0, 0.0),
};

VertexOut main(uint vid : SV_VertexID)
{
    VertexOut vout;
    
    vout.TexC = gTexCoords[vid];
    
    // transfer to DNC
    vout.PosH = float4(2.0f * vout.TexC.x - 1.0, 1.0 - 2.0f * vout.TexC.y, 0.0, 1.0);
    
    // tranform to View space
    float4 ph = mul(vout.PosH, cbssao.gInvProj);
    
    vout.PosV = ph.xyz / ph.w;
    
    return	vout;
}