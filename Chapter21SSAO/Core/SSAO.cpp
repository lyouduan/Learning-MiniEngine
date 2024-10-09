#include "SSAO.h"

SSAO::SSAO(UINT width, UINT height, DXGI_FORMAT normal_format, DXGI_FORMAT ssao_format) :
	m_Width(width), m_Height(height), m_NormalFormat(normal_format), m_SSAOFormat(ssao_format)
{
	m_Viewport = { 0.0, 0.0, (float)width, (float)height, 0.0, 1.0 };
	m_ScissorRect = { 1, 1, (int)width - 2, (int)height - 2 };

	// create Normal RTV and ssao map
	m_NormalMap.Create(L"normal render target view", width, height, 1, normal_format);

	m_SSAOMap.Create(L"ssao map", (UINT)width/2, (UINT)height/2, 1, ssao_format);
}

SSAO::~SSAO()
{
	m_NormalMap.Destroy();
	m_SSAOMap.Destroy();
}

void SSAO::ComputeSSAO()
{
}
