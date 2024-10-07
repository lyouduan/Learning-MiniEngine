#pragma once
#include "DepthBuffer.h"
#include <DirectXCollision.h>

class CSM
{
public:
	CSM(UINT width, UINT height, DXGI_FORMAT format, UINT nums);
	CSM(const CSM& rhs) = delete;
	CSM& operator=(const CSM& rhs) = delete;
	~CSM();

	std::vector<DirectX::XMVECTOR> CSM::setFrustumCornersWorldSpace(DirectX::XMMATRIX proj, DirectX::XMMATRIX view);

	DirectX::XMMATRIX setLightSpaceMatrix(
		float NearZ,
		float FarZ);

	void divideFrustums(float NearZ, float FarZ);

	void setToLightDir(DirectX::XMFLOAT3 _lightDir, float fov, float aspect, DirectX::XMMATRIX cameraProj);

	DepthBuffer& GetShadowBuffer(int i) { return m_ShadowMaps[i]; }

	const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSV() { return m_DSV[0]; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetDSV(int i) { return m_DSV[i]; }

	const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV() { return m_SRV[0]; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV(int i) { return m_SRV[i]; }

	DXGI_FORMAT GetFormat() { return m_Format; }

	D3D12_VIEWPORT Viewport() const { return m_Viewport; }
	D3D12_RECT ScissorRect() const { return m_ScissorRect; }

	DirectX::XMMATRIX GetLightView(int i ) const { return m_LightView[i]; }
	DirectX::XMMATRIX GetLightProj(int i) const { return m_LightProjection[i]; }
	DirectX::XMMATRIX GetShadowTransform(int i) const { return m_ShadowTransform[i]; }

private:

	// depth texture array
	std::vector<DepthBuffer> m_ShadowMaps;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_DSV;
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_SRV;

	DXGI_FORMAT m_Format;
	UINT m_Width;
	UINT m_Height;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_ScissorRect;

	DirectX::XMVECTOR m_LightDir;
	float m_FOV;
	float m_AspectRatio;
	DirectX::XMMATRIX m_CameraView;
	std::vector<DirectX::XMMATRIX> m_LightView;
	std::vector<DirectX::XMMATRIX> m_LightProjection;
	std::vector<DirectX::XMMATRIX> m_ShadowTransform;
};

