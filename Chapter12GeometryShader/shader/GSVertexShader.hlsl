#include "GScommon.hlsli"

VertexOut main(VertexIn vin)
{
    VertexOut vout;
    
    vout.centerW = vin.position;
    vout.sizeW = vin.size;
    
    return vout;
}