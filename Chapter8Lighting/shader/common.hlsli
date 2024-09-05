#ifndef COMMON_HLSLI
#define COMMON_HLSLI

#define MaxLights 16

#ifndef NUM_DIR_LIGHTS
#define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
#define NUM_SPOT_LIGHTS 0
#endif

struct ObjConstants
{
    float4x4 gWorld;
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
    float4 gAmbientLight;
    Light Lights[MaxLights];
};

struct MaterialConstants
{
    float4 gDiffuseAlbedo;
    float3 gFresnelR0;
    float gRoughness;
};

ConstantBuffer<ObjConstants> objConstants : register(b0);
ConstantBuffer<PassConstants> passConstants : register(b1);
ConstantBuffer<MaterialConstants> materialConstants : register(b2);

struct VertexIn
{
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct VertexOut
{
    float3 normal : NORMAL;
    float3 positionW : POSITION;
    float4 positionH : SV_Position; // only omit at last one
};

#endif // COMMON_HLSLI