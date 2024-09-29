cbuffer cbSettings : register(b0)
{
	
};

Texture2D<float2> gInput : register(t0);
RWTexture2D<float2> gOutput : register(u0);

[numthreads(16, 16, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    float Depth = gInput[dispatchThreadID.xy];
    gOutput[dispatchThreadID.xy] = float2(Depth, Depth* Depth);
}