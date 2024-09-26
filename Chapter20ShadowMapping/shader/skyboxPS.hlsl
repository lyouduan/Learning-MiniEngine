#include "common.hlsli"
float4 main(VertexOut pin) : SV_TARGET
{
    return gCubeMap.Sample(gsamLinearClamp, pin.positionW);
}