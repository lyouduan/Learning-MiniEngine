#include "BlurFilter.h"
#include "CommandListManager.h"
#include "GraphicsCore.h"
#include "CommandContext.h"

void BlurFilter::Initialize(const std::wstring& name, UINT width, UINT height, DXGI_FORMAT format, const Microsoft::WRL::ComPtr<ID3DBlob>& VertBinary, const Microsoft::WRL::ComPtr<ID3DBlob>& HorzBinary)
{
	m_Width  = width;
	m_Height = height;
	m_Format = format;

	// create color buffer
	m_BlurMap0.Create(name, width, height, 1, format);
	m_BlurMap1.Create(name, width, height, 1, format);

	// compute root signature
	m_ComputeRootSig.Reset(3, 0);
	m_ComputeRootSig[0].InitAsConstants(0, 12);
	m_ComputeRootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	m_ComputeRootSig[2].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
	m_ComputeRootSig.Finalize(L"blur root signature");

	// pso
	m_PSOVert.SetRootSignature(m_ComputeRootSig);
	m_PSOVert.SetComputeShader(VertBinary);
	m_PSOVert.Finalize();

	m_PSOHorz.SetRootSignature(m_ComputeRootSig);
	m_PSOHorz.SetComputeShader(HorzBinary);
	m_PSOHorz.Finalize();
}

// blur filter
void BlurFilter::Execute(GpuResource& input, int blurCount)
{
	auto weights = CalcGaussWeights(2.5f);
	int blurRadius = (int)weights.size() / 2;

	ComputeContext& context = ComputeContext::Begin(L"blur fiter");
	
	// copy buffer
	context.CopyBuffer(m_BlurMap0, input);

	// set compute root signature
	context.SetRootSignature(m_ComputeRootSig);

	// set the pipeline state
	context.SetConstant(0, 0, blurRadius);
	context.SetConstantArray(0, (UINT)weights.size(), weights.data(), 1);

	for (int i = 0; i < blurCount; ++i)
	{
		//
		// Horizontal Blur pass.
		//
		
		// set pso
		context.SetPipelineState(m_PSOHorz);

		// transition the  state
		context.TransitionResource(m_BlurMap0, D3D12_RESOURCE_STATE_GENERIC_READ);
		context.TransitionResource(m_BlurMap1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

		context.SetDynamicDescriptor(1, 0, m_BlurMap0.GetSRV());
		context.SetDynamicDescriptor(2, 0, m_BlurMap1.GetUAV());

		// how many groups do we need to dispatch to cover a row of pixels, where each
		// grop covers 256 pixels (the 256 is defined in the ComputeShader).

		UINT numGroupX = (UINT)ceilf(m_Width / 256.0f);
		context.Dispatch(numGroupX, m_Height, 1);

		context.TransitionResource(m_BlurMap0, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
		context.TransitionResource(m_BlurMap1, D3D12_RESOURCE_STATE_GENERIC_READ);
		
		//
		// Vertical Blur pass.
		//
		context.SetPipelineState(m_PSOVert);
		context.SetDynamicDescriptor(1, 0, m_BlurMap1.GetSRV());
		context.SetDynamicDescriptor(2, 0, m_BlurMap0.GetUAV());

		UINT numGroupY = (UINT)ceilf(m_Height / 256.0f);
		context.Dispatch(m_Width, numGroupY, 1);

		context.TransitionResource(m_BlurMap0, D3D12_RESOURCE_STATE_GENERIC_READ);
		context.TransitionResource(m_BlurMap1, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	}

	context.Finish(true);
}

std::vector<float> BlurFilter::CalcGaussWeights(float sigma)
{
	float twoSigma2 = 2.0f * sigma * sigma;

	// Estimate the blur radius based on sigma since sigma controls the "width" of the bell curve.
	// For example, for sigma = 3, the width of the bell curve is 
	int blurRadius = (int)ceil(2.0f * sigma);

	assert(blurRadius <= MaxBlurRadius);

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
