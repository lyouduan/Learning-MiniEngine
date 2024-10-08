#include "CSM.h"

using namespace DirectX;

CSM::CSM(UINT width, UINT height, DXGI_FORMAT format, UINT nums)
{
	m_Width = width;
	m_Height = height;
	m_Format = format;
	m_Levels = nums;

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
						z,
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
	const auto proj = SetPerspectiveMatrix(m_FOV, m_AspectRatio, NearZ, FarZ);

	// get frustum corners
	const auto frustumCorners = setFrustumCornersWorldSpace(proj, m_CameraView);

	XMVECTOR center = XMVectorSet(0.0, 0.0, 0.0, 0.0);
	for (const auto& v : frustumCorners)
		center += v;

	// 求视锥体中心
	float size = frustumCorners.size();
	center /= size;
	const auto lightView = XMMatrixLookAtLH(center + m_LightDir, center, XMVectorSet(0.0, 1.0, 0.0, 0.0));

	float minX = std::numeric_limits<float>::max();
	float maxX = std::numeric_limits<float>::lowest();
	float minY = std::numeric_limits<float>::max();
	float maxY = std::numeric_limits<float>::lowest();
	float minZ = std::numeric_limits<float>::max();
	float maxZ = std::numeric_limits<float>::lowest();

	for (const auto& v : frustumCorners)
	{
		auto trf = XMVector4Transform(v, lightView);
		minX = std::min(minX, trf[0]);
		maxX = std::max(maxX, trf[0]);
		minY = std::min(minY, trf[1]);
		maxY = std::max(maxY, trf[1]);
		minZ = std::min(minZ, trf[2]);
		maxZ = std::max(maxZ, trf[2]);
	}

	// Tune this parameter according to the scene
	constexpr float zMult = 1.0f;
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

	const XMMATRIX lightProjection = XMMatrixOrthographicOffCenterLH(minX*1.5, maxX * 1.5, minY * 1.5, maxY * 1.5, maxZ, minZ);

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

DirectX::XMMATRIX CSM::SetPerspectiveMatrix(float fov, float aspectHeightOverWidth, float nearZClip, float farZClip)
{
	// calculate X,Y by fov and z=1.0 as default
	float Y = 1.0f / std::tanf(fov * 0.5f);
	float X = Y * m_AspectRatio;

	float Q1, Q2;
	// ReverseZ puts far plane at Z=0 and near plane at Z=1.  This is never a bad idea, and it's
	// actually a great idea with F32 depth buffers to redistribute precision more evenly across
	// the entire range.  It requires clearing Z to 0.0f and using a GREATER variant depth test.
	// Some care must also be done to properly reconstruct linear W in a pixel shader from hyperbolic Z.
	
	Q1 = farZClip / (nearZClip - farZClip);
	Q2 = Q1 * nearZClip;

	XMMATRIX m_mat;
	m_mat.r[0] = XMVectorSet(X, 0.0f, 0.0f, 0.0f);
	m_mat.r[1] = XMVectorSet(0.0f, Y, 0.0f, 0.0f);
	m_mat.r[2] = XMVectorSet(0.0f, 0.0f, Q1, -1.0f);
	m_mat.r[3] = XMVectorSet(0.0f, 0.0f, Q2, 0.0f);

	return m_mat;

}

void CSM::divideFrustums(float NearZ, float FarZ)
{
	//0-10, 10-20, 20-100, 100-1000
	std::vector<float> shadowCascadeLevels{ FarZ / 100.0f, FarZ / 50.0f, FarZ / 10.0f };

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
