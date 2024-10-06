#include "Blur.h"
#include "pch.h"
#include "PipelineState.h"
#include <d3dcompiler.h>
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "GraphicsCommon.h"

Blur::Blur(UINT width, UINT height, DXGI_FORMAT format, UINT mipNums)
{
	m_Width = width;
	m_Height = height;
	m_Format = format;
	m_MipNums = mipNums;

	// create buffer
	Output0.Create(L"blur buffer", width, height, mipNums, format);
	Output1.Create(L"blur buffer", width, height, mipNums, format);

	CreateComputeRootSig();
}

Blur::~Blur()
{
	Output0.Destroy();
	Output1.Destroy();

	m_PSOs.clear();

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
	Microsoft::WRL::ComPtr<ID3DBlob> CSEsm;
	Microsoft::WRL::ComPtr<ID3DBlob> CSEvsm;

	D3DReadFileToBlob(L"shader/CSHorzBlur.cso", &csHorz);
	D3DReadFileToBlob(L"shader/CSVertBlur.cso", &csVert);
	D3DReadFileToBlob(L"shader/CSvsm.cso", &CSvsm);
	D3DReadFileToBlob(L"shader/CSEsm.cso", &CSEsm);
	D3DReadFileToBlob(L"shader/CSEvsm.cso", &CSEvsm);

	ComputePSO m_HorzPSO;
	m_HorzPSO.SetRootSignature(m_ComputeRootSig);
	m_HorzPSO.SetComputeShader(csHorz);
	m_HorzPSO.Finalize();
	m_PSOs["horz"] = m_HorzPSO;

	ComputePSO m_VertPSO;
	m_VertPSO.SetRootSignature(m_ComputeRootSig);
	m_VertPSO.SetComputeShader(csVert);
	m_VertPSO.Finalize();
	m_PSOs["vert"] = m_VertPSO;

	ComputePSO m_VsmPSO;
	m_VsmPSO.SetRootSignature(m_ComputeRootSig);
	m_VsmPSO.SetComputeShader(CSvsm);
	m_VsmPSO.Finalize();
	m_PSOs["vsm"] = m_VsmPSO;

	ComputePSO m_EsmPSO;
	m_EsmPSO.SetRootSignature(m_ComputeRootSig);
	m_EsmPSO.SetComputeShader(CSEsm);
	m_EsmPSO.Finalize();
	m_PSOs["esm"] = m_EsmPSO;

	ComputePSO m_EvsmPSO;
	m_EvsmPSO.SetRootSignature(m_ComputeRootSig);
	m_EvsmPSO.SetComputeShader(CSEvsm);
	m_EvsmPSO.Finalize();
	m_PSOs["evsm"] = m_EvsmPSO;

	//---------------------------------
	// Generate Mipmaps CS
	//---------------------------------
	m_MipmapRootSig.Reset(3, 1);
	m_MipmapRootSig[0].InitAsConstants(0, 4);
	m_MipmapRootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1, D3D12_SHADER_VISIBILITY_ALL);
	m_MipmapRootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 4, D3D12_SHADER_VISIBILITY_ALL);

	m_MipmapRootSig.InitStaticSampler(0, Graphics::SamplerLinearWrapDesc, D3D12_SHADER_VISIBILITY_ALL);
	m_MipmapRootSig.Finalize(L"mipmap CS rootSignature");

	Microsoft::WRL::ComPtr<ID3DBlob> Mipmap;
	D3DReadFileToBlob(L"shader/Mipmap.cso", &Mipmap);

	ComputePSO m_mipmapPSO;
	m_mipmapPSO.SetRootSignature(m_MipmapRootSig);
	m_mipmapPSO.SetComputeShader(Mipmap);
	m_mipmapPSO.Finalize();
	m_PSOs["mipmap"] = m_mipmapPSO;
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

	//CScontext.SetPipelineState(m_PSOs["evsm"]);
	CScontext.SetPipelineState(m_PSOs["vsm"]);
	//CScontext.SetPipelineState(m_PSOs["esm"]);
	CScontext.SetDynamicDescriptor(1, 0, input.GetDepthSRV());
	CScontext.SetDynamicDescriptor(2, 0, Output0.GetUAV());

	UINT NumGroupsX = (UINT)ceilf(input.GetWidth() / 16.0f);
	UINT NumGroupsY = (UINT)ceilf(input.GetHeight() / 16.0f);
	CScontext.Dispatch(NumGroupsX, NumGroupsY, 1);
	CScontext.TransitionResource(Output0, D3D12_RESOURCE_STATE_GENERIC_READ, true);
	
	for (int i = 0; i < BlurCount; ++i)
	{
		//---------------------------------
		// blur pass
		//---------------------------------
		auto weights = CalcGaussWeights(1.5f);
		int blurRadius = (int)weights.size() / 2;

		CScontext.SetConstant(0, 0, blurRadius);
		CScontext.SetConstantArray(0, weights.size(), weights.data(), 1);
		// horizontal pass
		CScontext.SetPipelineState(m_PSOs["horz"]);

		CScontext.TransitionResource(Output0, D3D12_RESOURCE_STATE_GENERIC_READ, true);
		CScontext.TransitionResource(Output1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);

		CScontext.SetDynamicDescriptor(1, 0, Output0.GetSRV());
		CScontext.SetDynamicDescriptor(2, 0, Output1.GetUAV());

		NumGroupsX = (UINT)ceilf(m_Width / 256.0f);

		CScontext.Dispatch(NumGroupsX, m_Height, 1);

		// vertical pass
		CScontext.SetPipelineState(m_PSOs["vert"]);

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

void Blur::GenerateMipMaps()
{
	if (m_MipNums <= 1) return;
	ComputeContext& CScontext = ComputeContext::Begin(L"mipmap CS");

	CScontext.SetRootSignature(m_MipmapRootSig);

	for (int mip = 1; mip < m_MipNums; ++mip)
	{
		uint32_t SrcWidth = m_Width >> (mip-1);
		uint32_t SrcHeight = m_Height >> (mip - 1);
		uint32_t DstWidth = SrcWidth >> 1;
		uint32_t DstHeight = SrcHeight >> 1;

		CScontext.SetPipelineState(m_PSOs["mipmap"]);

		CScontext.TransitionResource(Output1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
		CScontext.TransitionResource(Output0, D3D12_RESOURCE_STATE_GENERIC_READ, true);

		// output0 mip0写入input1的mip1
		CScontext.SetConstants(0, mip-1, m_MipNums, SrcWidth, DstWidth);

		CScontext.SetDynamicDescriptor(1, 0, Output0.GetSRV());
		CScontext.SetDynamicDescriptor(2, 0, Output1.GetUAV(mip));

		UINT NumGroupsX = (UINT)ceilf(DstWidth / 16.0f);
		UINT NumGroupsY = (UINT)ceilf(DstHeight / 16.0f);
		CScontext.Dispatch(NumGroupsX, NumGroupsY, 1);

		CScontext.TransitionResource(Output0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, true);
		CScontext.TransitionResource(Output1, D3D12_RESOURCE_STATE_GENERIC_READ, true);
		
		// input1 mip 1 写入 input0 mip 1
		CScontext.SetConstants(0, mip, m_MipNums, SrcWidth, DstWidth);
		CScontext.SetDynamicDescriptor(1, 0, Output1.GetSRV());
		CScontext.SetDynamicDescriptor(2, 0, Output0.GetUAV(mip));
		
		NumGroupsX = (UINT)ceilf(DstWidth / 16.0f);
		NumGroupsY = (UINT)ceilf(DstHeight / 16.0f);
		CScontext.Dispatch(NumGroupsX, NumGroupsY, 1);

		CScontext.TransitionResource(Output0, D3D12_RESOURCE_STATE_GENERIC_READ, true);
		CScontext.TransitionResource(Output1, D3D12_RESOURCE_STATE_GENERIC_READ, true);
	}

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
