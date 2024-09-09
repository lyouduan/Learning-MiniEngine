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
ConstantBuffer<MaterialConstants> matConstants : register(b2);

struct VertexIn
{
    float3 position : POSITION;
    float3 size : SIZE;
};

struct VertexOut
{
    float3 centerW : POSITION;
    float2 sizeW : SIZE;
};

struct GeoOut
{
    float4 posH : SV_POSITION;
    float3 posW : POSITION;
    float3 normalW : NORMAL;
    float2 tex  : TEXCOORD;
    uint PrimID : SV_PrimitiveID;
};

#endif // COMMON_HLSLI