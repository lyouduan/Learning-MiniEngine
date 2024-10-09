#include "common.hlsli"

VertexOut main(VertexIn vin)
{
    VertexOut vout;
    
    vout.positionH = float4(vin.position, 1.0);
 
    vout.tex = vin.tex;
    
    return vout;
}