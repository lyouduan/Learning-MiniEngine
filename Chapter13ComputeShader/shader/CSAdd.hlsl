
Texture2D gInput : register(t0);
RWTexture2D<float4> gOutput : register(u0);

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    gOutput[dispatchThreadID.xy] = gOutput[dispatchThreadID.xy] * gInput[dispatchThreadID.xy];

}