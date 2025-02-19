#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#define MaxLights 16


struct ObjConstants
{
    float4x4 gWorld;
    float4x4 gTexTransform;
};

struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    float SpotPower;
};

struct PassConstants
{
    float4x4 gViewProj;
    float3 gEyePosW;
    float pad0;
    float4 gAmbientLight;
    Light Lights[MaxLights];
    float4 gFogColor;
    float gFogStart;
    float gFogRange;
    float pad1;
    float pad2;
};

struct MaterialConstants
{
    float4x4 gMatTransform;
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
};

ConstantBuffer<ObjConstants> objConstants : register(b0);
ConstantBuffer<PassConstants> passConstants : register(b1);

struct VertexIn
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 tex : TEXCOORD;
};

struct VertexOut
{
    float3 normal : NORMAL;
    float3 positionW : POSITION;
    float2 tex : TEXCOORD;
    float4 positionH : SV_Position; // only omit at last one
};

#endif // COMMON_HLSLI