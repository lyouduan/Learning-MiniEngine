#include "ShadowMap.h"

using namespace DirectX;

ShadowMap::ShadowMap(UINT width, UINT height, DXGI_FORMAT format)
{
	m_Width = width;
	m_Height = height;
	m_Format = format;

	m_Viewport = { 0.0, 0.0, (float)width, (float)height, 0.0, 1.0 };
	m_ScissorRect = { 1, 1, (int)width-2, (int)height-2 };

	// create depth buffer
	CreateBuffer();
}

ShadowMap::~ShadowMap()
{
	m_ShadowMap.Destroy();
}

void ShadowMap::SetToLightSpaceView(DirectX::XMFLOAT3 _lightDir, DirectX::BoundingSphere mSceneBounds)
{
	// parallel light

	XMVECTOR lightDir = XMLoadFloat3(&_lightDir);

	XMVECTOR lightPos = 1.0f * mSceneBounds.Radius * lightDir;
	XMVECTOR target = XMLoadFloat3(&mSceneBounds.Center);
	XMVECTOR lightUp = XMVectorSet(0.0, 1.0, 0.0, 0.0);

	m_LightView = DirectX::XMMatrixLookAtLH(lightPos, target, lightUp);

	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(target, m_LightView));

	// Ortho frustum in light space encloses scene.
	float l = sphereCenterLS.x - mSceneBounds.Radius;
	float b = sphereCenterLS.y - mSceneBounds.Radius;
	float n = sphereCenterLS.z - mSceneBounds.Radius;
	float r = sphereCenterLS.x + mSceneBounds.Radius;
	float t = sphereCenterLS.y + mSceneBounds.Radius;
	float f = sphereCenterLS.z + mSceneBounds.Radius;

	// ????
	m_LightProjection = XMMatrixOrthographicOffCenterLH(l, r, b, t, f, n );

	//Transform NDC space[-1, +1] ^ 2 to texture space[0, 1] ^ 2
	XMMATRIX T(
			0.5f, 0.0f, 0.0f, 0.0f,
			0.0f, -0.5f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.5f, 0.5f, 0.0f, 1.0f);

	m_ShadowTransform = m_LightView * m_LightProjection * T;
}

void ShadowMap::CreateBuffer()
{
	m_ShadowMap.Create(L"Shadow Depth Buffer", m_Width, m_Height, m_Format);
}
