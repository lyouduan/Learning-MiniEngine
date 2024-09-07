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
};

struct Vertex {
	DirectX::XMFLOAT3 position;
	DirectX::XMFLOAT3 normal;
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
	DirectX::XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	DirectX::XMFLOAT3 FresnelR0 = { 1.0f, 1.0f, 1.0f };
	float Roughness = 0.25f; 
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
};

#define MaxLights 16
__declspec(align(16)) struct PassConstants
{
	DirectX::XMFLOAT4X4 ViewProj;
	DirectX::XMFLOAT3 eyePosW = {0.0, 0.0, 0.0};
	DirectX::XMFLOAT4 ambientLight = { 0.1f, 0.1f, 0.1f, 1.0f };
	Light Lights[MaxLights];
};