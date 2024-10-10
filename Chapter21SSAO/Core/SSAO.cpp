#include "SSAO.h"
#include "GraphicsCommon.h"
#include <d3dcompiler.h>

SSAO::SSAO(UINT width, UINT height, DXGI_FORMAT normal_format, DXGI_FORMAT ssao_format) :
	m_Width(width), m_Height(height), m_NormalFormat(normal_format), m_SSAOFormat(ssao_format)
{
	m_Viewport = { 0.0, 0.0, (float)width/2, (float)height/2, 0.0, 1.0 };
	m_ScissorRect = { 1, 1, (int)width/2 - 2, (int)height/2 - 2 };

	// create Normal RTV and ssao map
	m_NormalMap.Create(L"normal render target view", width, height, 1, normal_format);

	m_PosMap.Create(L"pso render target view", width, height, 1, normal_format);

	m_SSAOMap.Create(L"ssao map", (UINT)width/2, (UINT)height/2, 1, ssao_format);

	// init RootSignature
	m_RootSig.Reset(4, 1);
	m_RootSig[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL);
	m_RootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	m_RootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1);
	m_RootSig[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 1);
	m_RootSig.InitStaticSampler(0, Graphics::SamplerLinearWrapDesc, D3D12_SHADER_VISIBILITY_PIXEL);
	m_RootSig.Finalize(L"ssao root signature");


	// init PSO
	D3D12_INPUT_ELEMENT_DESC mInputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	// shader 
	//Microsoft::WRL::ComPtr<ID3DBlob> ssaoVS;
	//Microsoft::WRL::ComPtr<ID3DBlob> ssaoPS;
	//D3DReadFileToBlob(L"shader/ssaoVS.cso", &ssaoVS);
	//D3DReadFileToBlob(L"shader/ssaoPS.cso", &ssaoPS);
	//
	//m_PSO.SetRootSignature(m_RootSig);
	//m_PSO.SetBlendState(Graphics::BlendDisable);
	//m_PSO.SetDepthStencilState(Graphics::DepthStateDisabled);
	//m_PSO.SetInputLayout(_countof(mInputLayout), mInputLayout);
	//
	//m_PSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	//m_PSO.SetRenderTargetFormat(m_SSAOFormat, DXGI_FORMAT_D32_FLOAT);
	//m_PSO.SetVertexShader(ssaoVS);
	//m_PSO.SetPixelShader(ssaoPS);
	//m_PSO.Finalize();

}

SSAO::~SSAO()
{
	m_PosMap.Destroy();
	m_NormalMap.Destroy();
	m_SSAOMap.Destroy();
}

void SSAO::ComputeSSAO(GraphicsContext& gfxContext, Math::Camera camera, DepthBuffer& depthMap)
{
	gfxContext.SetViewportAndScissor(m_Viewport, m_ScissorRect);

	gfxContext.TransitionResource(m_SSAOMap, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	gfxContext.TransitionResource(m_NormalMap, D3D12_RESOURCE_STATE_GENERIC_READ, true);

	// clear ssao map
	gfxContext.ClearColor(m_SSAOMap);

	// binding render target
	gfxContext.SetRenderTarget(m_SSAOMap.GetRTV());

	// set PSO
	gfxContext.SetPipelineState(m_PSO);
	// set Root Signature
	gfxContext.SetRootSignature(m_RootSig);
	// set SRV CBV 
	gfxContext.SetDynamicDescriptor(0, 0, m_NormalMap.GetSRV());
	gfxContext.SetDynamicDescriptor(1, 0, depthMap.GetDepthSRV());

	// draw fullscreen quad
	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfxContext.SetVertexBuffers(0, 0, nullptr);
	gfxContext.SetIndexBuffer(nullptr);
	gfxContext.DrawInstanced(6, 1, 0, 0);

	gfxContext.TransitionResource(m_SSAOMap, D3D12_RESOURCE_STATE_GENERIC_READ, true);
}

