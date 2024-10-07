#include "CSM.h"

using namespace DirectX;

CSM::CSM(UINT width, UINT height, DXGI_FORMAT format, UINT nums)
{
	m_Width = width;
	m_Height = height;
	m_Format = format;

	m_Viewport = { 0.0, 0.0, (float)width, (float)height, 0.0, 1.0 };
	m_ScissorRect = { 1, 1, (int)width - 2, (int)height - 2 };

	for (int i = 0; i < nums; i++)
	{
		DepthBuffer sm;
		sm.Create(L"shadow map", width, height, format);

		m_ShadowMaps.push_back(std::move(sm));
	}
	
	for (auto& m : m_ShadowMaps)
	{
		m_DSV.push_back(m.GetDSV());
		m_SRV.push_back(m.GetDepthSRV());
	}

}

CSM::~CSM()
{
	for (auto& m : m_ShadowMaps)
	{
		m.Destroy();
	}

	m_ShadowMaps.clear();
	m_DSV.clear();
	m_SRV.clear(); 
}

std::vector<XMVECTOR> CSM::setFrustumCornersWorldSpace(DirectX::XMMATRIX proj, DirectX::XMMATRIX view)
{
	auto viewProj = view * proj;
	auto det = XMMatrixDeterminant(viewProj);
	const XMMATRIX invViewProj = XMMatrixInverse(&det, viewProj);

	std::vector<XMVECTOR> frustumCorners;
	for (unsigned int x = 0; x < 2; ++x)
	{
		for (unsigned int y = 0; y < 2; ++y)
		{
			for (unsigned int z = 0; z < 2; ++z)
			{
				XMVECTOR pt = XMVector4Transform(XMVectorSet(
						2.0f * x - 1.0f,
						2.0f * y - 1.0f,
						2.0f * z - 1.0f,
						1.0f), invViewProj);

				pt /= pt[3];

				frustumCorners.push_back(pt);
			}
		}
	}

	return frustumCorners;
}

XMMATRIX CSM::setLightSpaceMatrix(
	float NearZ,
	float FarZ)
{
	// set projection matrix
	const auto proj = XMMatrixPerspectiveFovLH(m_FOV, m_AspectRatio, NearZ, FarZ);

	// get frustum corners
	const auto frustumCorners = setFrustumCornersWorldSpace(proj, m_CameraView);

	XMVECTOR center = XMVectorSet(0.0, 0.0, 0.0, 0.0);
	for (const auto& v : frustumCorners)
		center += v;

	// 求视锥体中心
	int size = frustumCorners.size();
	center /= XMVectorSet(size, size, size, size);

	const auto lightView = XMMatrixLookAtLH(center - 10 * m_LightDir, center, XMVectorSet(0.0, 1.0, 0.0, 0.0));

	float minX = std::numeric_limits<float>::max();
	float maxX = std::numeric_limits<float>::lowest();
	float minY = std::numeric_limits<float>::max();
	float maxY = std::numeric_limits<float>::lowest();
	float minZ = std::numeric_limits<float>::max();
	float maxZ = std::numeric_limits<float>::lowest();

	for (const auto& v : frustumCorners)
	{
		const auto trf = XMVector4Transform(v, lightView);
		minX = std::min(minX, trf[0]);
		maxX = std::max(maxX, trf[0]);
		minY = std::min(minY, trf[1]);
		maxY = std::max(maxY, trf[1]);
		minZ = std::min(minZ, trf[2]);
		maxZ = std::max(maxZ, trf[2]);
	}

	// Tune this parameter according to the scene
	constexpr float zMult = 10.0f;
	if (minZ < 0)
	{
		minZ *= zMult;
	}
	else
	{
		minZ /= zMult;
	}
	if (maxZ < 0)
	{
		maxZ /= zMult;
	}
	else
	{
		maxZ *= zMult;
	}

	const XMMATRIX lightProjection = XMMatrixOrthographicOffCenterLH(minX, maxX, minY, maxY, minZ, maxZ);

	//Transform NDC space[-1, +1] ^ 2 to texture space[0, 1] ^ 2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	m_ShadowTransform.push_back(lightView * lightProjection * T);

	m_LightView.push_back(lightView);
	
	return lightProjection;
}

void CSM::divideFrustums(float NearZ, float FarZ)
{

	std::vector<float> shadowCascadeLevels{ FarZ / 100.0f, FarZ / 50.0f, FarZ / 20.0f, FarZ / 4.0f };
	
	for (size_t i = 0; i < shadowCascadeLevels.size() + 1; ++i)
	{
		if (i == 0)
		{
			m_LightProjection.push_back(setLightSpaceMatrix(NearZ, shadowCascadeLevels[i]));
		}
		else if (i < shadowCascadeLevels.size())
		{
			m_LightProjection.push_back(setLightSpaceMatrix(shadowCascadeLevels[i-1], shadowCascadeLevels[i]));
		}
		else
		{
			m_LightProjection.push_back(setLightSpaceMatrix(shadowCascadeLevels[i - 1], FarZ));
		}
	}
	
}

void CSM::setToLightDir(DirectX::XMFLOAT3 _lightDir, float fov, float aspect, DirectX::XMMATRIX cameraView)
{
	m_LightDir = XMLoadFloat3(&_lightDir);
	m_FOV = fov;
	m_AspectRatio = aspect;
	m_CameraView = cameraView;
}
