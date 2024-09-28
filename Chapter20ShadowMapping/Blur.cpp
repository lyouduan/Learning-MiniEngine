#include "Blur.h"
#include "pch.h"
#include "PipelineState.h"
#include <d3dcompiler.h>
#include "GraphicsCore.h"
#include "CommandContext.h"

Blur::Blur(UINT width, UINT height, DXGI_FORMAT format)
{
	m_Width = width;
	m_Height = height;
	m_Format = format;

	// create buffer
	Output0.Create(L"blur buffer", width, height, 1, format);
	Output1.Create(L"blur buffer", width, height, 1, format);

	CreateComputeRootSig();
}

Blur::~Blur()
{
	Output0.Destroy();
	Output1.Destroy();

	m_HorzPSO.DesytroyAll();
	m_VertPSO.DesytroyAll();

	m_ComputeRootSig.DestroyAll();
}

void Blur::CreateComputeRootSig()
{
	m_ComputeRootSig.Reset(3, 0);
	m_ComputeRootSig[0].InitAsConstants(0, 12);
	m_ComputeRootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
	m_ComputeRootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
	m_ComputeRootSig.Finalize(L"Compute rootSignature");

	// shader 
	Microsoft::WRL::ComPtr<ID3DBlob> csHorz;
	Microsoft::WRL::ComPtr<ID3DBlob> csVert;
	Microsoft::WRL::ComPtr<ID3DBlob> CSvsm;
	D3DReadFileToBlob(L"shader/CSHorzBlur.cso", &csHorz);
	D3DReadFileToBlob(L"shader/CSVertBlur.cso", &csVert);
	D3DReadFileToBlob(L"shader/CSvsm.cso", &CSvsm);

	m_HorzPSO.SetRootSignature(m_ComputeRootSig);
	m_HorzPSO.SetComputeShader(csHorz);
	m_HorzPSO.Finalize();
	
	m_VertPSO.SetRootSignature(m_ComputeRootSig);
	m_VertPSO.SetComputeShader(csVert);
	m_VertPSO.Finalize();

	m_VsmPSO.SetRootSignature(m_ComputeRootSig);
	m_VsmPSO.SetComputeShader(CSvsm);
	m_VsmPSO.Finalize();
}

void Blur::Execute(DepthBuffer& input, int BlurCount)
{

	ComputeContext& CScontext = ComputeContext::Begin(L"blur compute");

	//---------------------------------
	// vsm pass
	//---------------------------------
	CScontext.SetRootSignature(m_ComputeRootSig);
	CScontext.TransitionResource(input, D3D12_RESOURCE_STATE_GENERIC_READ, true);
	CScontext.TransitionResource(Output0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

	CScontext.SetPipelineState(m_VsmPSO);
	CScontext.SetDynamicDescriptor(1, 0, input.GetDepthSRV());
	CScontext.SetDynamicDescriptor(2, 0, Output0.GetUAV());

	UINT NumGroupsX = (UINT)ceilf(input.GetWidth() / 16.0f);
	UINT NumGroupsY = (UINT)ceilf(input.GetHeight() / 16.0f);
	CScontext.Dispatch(NumGroupsX, NumGroupsY, 1);
	CScontext.TransitionResource(Output0, D3D12_RESOURCE_STATE_GENERIC_READ, true);


	//---------------------------------
	// blur pass
	//---------------------------------
	auto weights = CalcGaussWeights(1.5f);
	int blurRadius = (int)weights.size() / 2;

	CScontext.SetConstant(0, 0, blurRadius);
	CScontext.SetConstantArray(0, weights.size(), weights.data(), 1);

	for (int i = 0; i < BlurCount; ++i)
	{
		// horizontal pass
		CScontext.SetPipelineState(m_HorzPSO);

		CScontext.TransitionResource(Output0, D3D12_RESOURCE_STATE_GENERIC_READ, true);
		CScontext.TransitionResource(Output1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

		CScontext.SetDynamicDescriptor(1, 0, Output0.GetSRV());
		CScontext.SetDynamicDescriptor(2, 0, Output1.GetUAV());

		NumGroupsX = (UINT)ceilf(m_Width / 256.0f);

		CScontext.Dispatch(NumGroupsX, m_Height, 1);

		// vertical pass
		CScontext.SetPipelineState(m_VertPSO);

		CScontext.TransitionResource(Output0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
		CScontext.TransitionResource(Output1, D3D12_RESOURCE_STATE_GENERIC_READ, true);

		CScontext.SetDynamicDescriptor(1, 0, Output1.GetSRV());
		CScontext.SetDynamicDescriptor(2, 0, Output0.GetUAV());

		NumGroupsY = (UINT)ceilf(m_Height / 256.0f);

		CScontext.Dispatch(m_Width, NumGroupsY, 1);
	}

	CScontext.TransitionResource(Output0, D3D12_RESOURCE_STATE_GENERIC_READ, true);
	CScontext.TransitionResource(Output1, D3D12_RESOURCE_STATE_GENERIC_READ, true);

	CScontext.Finish(true);
}

std::vector<float> Blur::CalcGaussWeights(float sigma)
{
	float twoSigma2 = 2.0f * sigma * sigma;

	// Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
	// For example, for sigma = 3, the width of the bell curve is 
	int blurRadius = (int)ceil(2.0f * sigma);

	ASSERT(blurRadius <= MaxBlurRadius);

	std::vector<float> weights;
	weights.resize(2 * blurRadius + 1);

	float weightSum = 0.0f;

	for (int i = -blurRadius; i <= blurRadius; ++i)
	{
		float x = (float)i;

		weights[i + blurRadius] = expf(-x * x / twoSigma2);

		weightSum += weights[i + blurRadius];
	}

	// Divide by the sum so all the weights add up to 1.0.
	for (int i = 0; i < weights.size(); ++i)
	{
		weights[i] /= weightSum;
	}

	return weights;
}
