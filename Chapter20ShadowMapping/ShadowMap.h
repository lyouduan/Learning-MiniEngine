#pragma once
#include "DepthBuffer.h"
#include <DirectXCollision.h>

class ShadowMap
{
public:

	ShadowMap(UINT width, UINT height, DXGI_FORMAT format);

	void SetToLightSpaceView(DirectX::XMFLOAT3 lightDir, DirectX::XMFLOAT3 target,DirectX::BoundingSphere mSceneBounds);

	D3D12_CPU_DESCRIPTOR_HANDLE GetDSV() { return m_ShadowMap.GetDSV(); }

	D3D12_CPU_DESCRIPTOR_HANDLE GetSRV() { return m_ShadowMap.GetDepthSRV(); }

	DXGI_FORMAT GetFormat() { return m_Format; }

	D3D12_VIEWPORT Viewport() const { return m_Viewport; }
	D3D12_RECT ScissorRect() const { return m_ScissorRect; }

	DirectX::XMMATRIX GetLightView() const { return m_LightView; }
	DirectX::XMMATRIX GetLightProj() const { return m_LightProjection; }
	DirectX::XMMATRIX GetShadowTransform() const { return m_ShadowTransform; }

private:
	
	void CreateBuffer();

	DepthBuffer m_ShadowMap;
	DXGI_FORMAT m_Format;
	UINT m_Width;
	UINT m_Height;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_ScissorRect;

	DirectX::BoundingSphere mSceneBounds;

	DirectX::XMMATRIX m_LightView;
	DirectX::XMMATRIX m_LightProjection;
	DirectX::XMMATRIX m_ShadowTransform;
};

