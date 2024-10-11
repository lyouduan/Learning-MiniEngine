#pragma once
#include "ColorBuffer.h"
#include "CommandContext.h"
#include "Camera.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "../d3dUtil.h"

class SSAO
{
public:
	SSAO(UINT width, UINT height, DXGI_FORMAT normal_format, DXGI_FORMAT ssao_format);

	SSAO(const SSAO& rhs) = delete;
	SSAO& operator=(const SSAO& rhs) = delete;

	~SSAO();

	ColorBuffer& GetNormalMap() { return m_NormalMap; }
	ColorBuffer& GetPosMAP() { return m_PosMap; }
	ColorBuffer& GetSSAOMAP() { return m_SSAOMap; }

	UINT GetSSAOWidth() const { return m_Width/2; }
	UINT GetSSAOHeight() const { return m_Height/2; }

	UINT GetRTVWidth() const { return m_Width; }
	UINT GetRTVHeight() const { return m_Height; }

	DXGI_FORMAT GetSSAOFormat() const { return m_SSAOFormat; }
	DXGI_FORMAT GetNormalFormat() const { return m_NormalFormat; }

	const D3D12_CPU_DESCRIPTOR_HANDLE& GetPosSRV() const { return m_PosMap.GetSRV(); }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetNormalSRV() const { return m_NormalMap.GetSRV(); }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetSSAOSRV() const { return m_SSAOMap.GetSRV(); }

	const D3D12_CPU_DESCRIPTOR_HANDLE& GetPosRTV() const { return m_PosMap.GetRTV(); }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetNormalRTV() const { return m_NormalMap.GetRTV(); }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetSSAORTV() const { return m_SSAOMap.GetRTV(); }


	const GraphicsPSO& GetPSO() const { return m_PSO; }
	const RootSignature& GetRootSig() const { return m_RootSig; }
	const SsaoPassConstants GetSsaoCB() const { return m_ssaoCB; }

	D3D12_VIEWPORT Viewport() const { return m_Viewport; }
	D3D12_RECT ScissorRect() const { return m_ScissorRect; }

	void ComputeSSAO(GraphicsContext& gfxContext,DepthBuffer& depthMap);

	void UpdateCB(Math::Camera& camera);

private:

	UINT m_Width;
	UINT m_Height;
	DXGI_FORMAT m_SSAOFormat;
	DXGI_FORMAT m_NormalFormat;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_ScissorRect;

	ColorBuffer m_NormalMap;
	ColorBuffer m_PosMap;
	ColorBuffer m_SSAOMap;

	SsaoPassConstants m_ssaoCB;

	RootSignature m_RootSig;
	GraphicsPSO m_PSO;

};

