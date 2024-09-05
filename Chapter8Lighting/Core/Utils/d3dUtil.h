#pragma once
#include <DirectXMath.h>
#include <string>

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
	DirectX::XMFLOAT4 ambientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	Light Lights[MaxLights];
};