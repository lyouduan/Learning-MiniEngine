#pragma once
#include <DirectXMath.h>
#include <string>
#include "GpuBuffer.h"



struct Material
{
	std::string Name;

	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 1.0f, 1.0f, 1.0f };
	float Roughness = 0.25f; // align with float3

	DirectX::XMMATRIX MatTransform = DirectX::XMMatrixIdentity();

	UINT DiffuseMapIndex = 0;
	UINT NormalMapIndex = 0;
	UINT MaterialPad[2];

};

struct Vertex {
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 normal;
	DirectX::XMFLOAT2 tex;
	DirectX::XMFLOAT3 tangent;
};

struct SubmeshGeometry
{
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	INT BaseVertexLocation = 0;
	
};

struct MeshGeometry
{
	std::string name;

	// buffer
	StructuredBuffer m_VertexBuffer;
	ByteAddressBuffer m_IndexBuffer;

	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;
};

// shader constants

__declspec(align(16)) struct MaterialConstants
{
	DirectX::XMFLOAT4X4 MatTransform;
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 1.0f, 1.0f, 1.0f };
	float Roughness = 0.25f; 
	UINT DiffuseMapIndex = 0;
	UINT NormalMapIndex = 0;
	UINT MaterialPad[2];
};

__declspec(align(16)) struct Light
{
	DirectX::XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };  // light color
	float FalloffStart = 1.0f;							// point light\spot light
	DirectX::XMFLOAT3 Direction = { 0.5f, 0.5f, 0.5f }; // parallel light\spot light
	float FalloffEnd = 10.0f;							// point light\spot light
	DirectX::XMFLOAT3 Position = { 0.5f, 0.5f, 0.5f };  // point light\spot light
	float SpotPower = 64.0f;							// spot light
};

__declspec(align(16)) struct ObjConstants
{
	DirectX::XMFLOAT4X4 World;
	DirectX::XMFLOAT4X4 TexTransform;
	DirectX::XMFLOAT4X4 MatTransform;
	UINT MaterialIndex = 0;
	UINT ObjPad[3];
};

#define MaxLights 16
__declspec(align(16)) struct PassConstants
{
	DirectX::XMFLOAT4X4 View;
	DirectX::XMFLOAT4X4 Proj;
	DirectX::XMFLOAT4X4 ShadowTransform;
	DirectX::XMFLOAT3 eyePosW = {0.0, 0.0, 0.0};
	float pad0 = 0.0;
	DirectX::XMFLOAT4 ambientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	Light Lights[MaxLights];
	DirectX::XMFLOAT4 fogColor = { 0.7f, 0.7f, 0.7f, 1.0f };
	float fogStart = 5.0f;
	float fogRange = 100.0;
	float pad1;
	float pad2;
};

__declspec(align(16)) struct SsaoPassConstants
{
	DirectX::XMFLOAT4X4 gProj;
	DirectX::XMFLOAT4X4 gInvProj;
	DirectX::XMFLOAT4X4 gProjTex;
	float gOcclusionRadius = 1.0;
	float gOcclusionFadeStart = 1.0;
	float gOcclusionFadeEnd = 1.0;
	float gSurfaceEpsilon = 1.0;
};
