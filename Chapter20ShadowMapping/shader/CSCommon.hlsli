#ifndef CSCOMMON_HLSLI
#define CSCOMMON_HLSLI
cbuffer cbSettings : register(b0)
{
	// We cannot have an array entry in a constant buffer that gets mapped onto
	// root constants, so list each element.  
	
    int gBlurRadius;

	// Support up to 11 blur weights.
    float w0;
    float w1;
    float w2;
    float w3;
    float w4;
    float w5;
    float w6;
    float w7;
    float w8;
    float w9;
    float w10;
};

static const int gMaxBlurRadius = 5;

Texture2D<float2> gInput : register(t0);
RWTexture2D<float2> gOutput : register(u0);

#define N 256
#define CacheSize (N + 2*gMaxBlurRadius)
groupshared float2 gCache[CacheSize];

#endif // CSCOMMON_HLSLI 