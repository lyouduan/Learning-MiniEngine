#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#define MaxLights 16


struct ObjConstants
{
    float4x4 gWorld;
    float4x4 gTexTransform;
    float4x4 gMatTransform;
    uint gMaterialIndex;
    uint gObjPad1;
    uint gObjPad2;
    uint gObjPad3;
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

struct MaterialData
{
    float4x4 gMatTransform;
    float4   gDiffuseAlbedo;
    float3   gFresnelR0;
    float    gRoughness;
    uint gDiffuseMapIndex;
    uint MatPad1;
    uint MatPad2;
    uint MatPad3;
};

ConstantBuffer<ObjConstants> objConstants : register(b0);
ConstantBuffer<PassConstants> passConstants : register(b1);

TextureCube gCubeMap : register(t0);
Texture2D gDiffuseMap[8] : register(t1);
// structured buffer
StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);

SamplerState gsamLinearClamp : register(s0);

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