cbuffer cbSettings : register(b0)
{
	
};

Texture2D<float> gInput : register(t0);
RWTexture2D<float4> gOutput : register(u0);

[numthreads(16, 16, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    float Depth = gInput[dispatchThreadID.xy];
    //Depth = Depth * 2.0 - 1.0f;
    
    // evsm exp(cz)和-exp(-cz)偏移
    float e1 = exp(30 * Depth);
    float e2 = -exp(-30 * Depth);
    
    gOutput[dispatchThreadID.xy] = float4(e1, e1 * e1, e2, e2 * e2);
}