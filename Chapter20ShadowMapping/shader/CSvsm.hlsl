#include "CSCommon.hlsli"

[numthreads(16, 16, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    float Depth = gInput[dispatchThreadID.xy];
    gOutput[dispatchThreadID.xy] = float2(Depth, Depth * Depth);
}