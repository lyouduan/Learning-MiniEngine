#ifndef SSAOCOMMON_HLSLI
#define SSAOCOMMON_HLSLI



struct cbSSAO
{
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gProjTex;
	
    float gOcclusionRadius;
    float gOcclusionFadeStart;
    float gOcclusionFadeEnd;
    float gSurfaceEpsilon;
};

ConstantBuffer<cbSSAO> cbssao : register(b0);

struct VertexIn
{
    float3 PosL : POSITION;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_POSITION;
    float3 PosV : POSITION;
    float2 TexC : TEXCOORD;
};

Texture2D NormalMap : register(t0);
Texture2D DepthMap : register(t1);

SamplerState gsamLinearClamp : register(s0);

#endif //SSAOCOMMON_HLSLI