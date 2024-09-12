#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#define MaxLights 16

struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    float SpotPower;
};

cbuffer objConstants : register(b0)
{
    float4x4 gWorld;
    float4x4 gTexTransform;
}
cbuffer passConstants : register(b1)
{
    float4x4 gViewProj;
    float3   gEyePosW;
    float    pad0;
    float4   gAmbientLight;
    Light    Lights[MaxLights];
    float4   gFogColor;
    float    gFogStart;
    float    gFogRange;
    float    pad1;
    float    pad2;
}
cbuffer matConstants : register(b2)
{
    float4x4 gMatTransform;
    float4   gDiffuseAlbedo;
    float3   gFresnelR0;
    float    gRoughness;
}

struct VertexIn
{
    float3 posL : POSITION;
};

struct VertexOut
{
    float3 posL : POSITION;
};

struct PatchTess
{
    float EdgeTess[4] : SV_TessFactor;
    float InsideTess[2] : SV_InsideTessFactor;
};

struct HullOut
{
    float3 PosL : POSITION;
};

struct DomainOut
{
    float4 PosH : SV_POSITION;
};

#define NUM_CONTROL_POINTS 4

#endif // COMMON_HLSLI