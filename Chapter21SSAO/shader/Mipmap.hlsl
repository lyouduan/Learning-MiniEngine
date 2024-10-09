cbuffer cbSettings : register(b0)
{
    int srcMipLevel;
    int NumMipLevels;
    int SrcDimension;
    int DestDimension;
};

Texture2D<float4> gInput : register(t0);
RWTexture2D<float4> gOutput : register(u0);

SamplerState gsamLinearClamp : register(s0);

// Each group processes a tile of 8x8 pixels
[numthreads(16, 16, 1)]
void main(int3 dispatchThreadID : SV_DispatchThreadID)
{
    // 线程ID映射到纹理坐标
  
    //float TexelSize = 1.0 / (float) width;
    float2 TexelSize = float2(1.0 / SrcDimension, 1.0 / SrcDimension);
    float2 uv = TexelSize * (dispatchThreadID.xy + float2(0.5, 0.5)) * 2.0;
    float2 Off = TexelSize * 0.5;
    
    float4 color = float4(0,0,0,0);
    color += gInput.SampleLevel(gsamLinearClamp, uv, srcMipLevel);
    color += gInput.SampleLevel(gsamLinearClamp, uv + float2(Off.x , 0.0  ), srcMipLevel);
    color += gInput.SampleLevel(gsamLinearClamp, uv + float2(0.0   , Off.y), srcMipLevel);
    color += gInput.SampleLevel(gsamLinearClamp, uv + float2(Off.x, Off.y), srcMipLevel);
    color *= 0.25;

    gOutput[dispatchThreadID.xy] = color;
}
