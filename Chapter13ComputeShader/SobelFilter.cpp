#include "SobelFilter.h"
#include "CommandContext.h"

void SobelFilter::Initialize(const std::wstring& name, UINT width, UINT height, DXGI_FORMAT format, const Microsoft::WRL::ComPtr<ID3DBlob>& Binary)
{
	m_Width = width;
	m_Height = height;
	m_Format = format;

	// create buffer
	input.Create(name, width, height, 1, format);
	Output.Create(name, width, height, 1, format);

	// set root signature
	m_RootSig.Reset(2, 0);
	m_RootSig[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	m_RootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
	m_RootSig.Finalize(L"sobel root signature");

	// set pso
	m_PSO.SetRootSignature(m_RootSig);
	m_PSO.SetComputeShader(Binary);
	m_PSO.Finalize();
}

void SobelFilter::Execute(ColorBuffer& in)
{
	ComputeContext& context = ComputeContext::Begin(L"sobel filter");

	// set root signature
	context.SetRootSignature(m_RootSig);
	context.CopyBuffer(input, in);
	// set pso
	context.SetPipelineState(m_PSO);

	context.TransitionResource(input, D3D12_RESOURCE_STATE_GENERIC_READ);
	context.TransitionResource(Output, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	context.SetDynamicDescriptor(0, 0, input.GetSRV());
	context.SetDynamicDescriptor(1, 0, Output.GetUAV());

	UINT numGroupsX = (UINT)ceilf(m_Width / 16.0f);
	UINT numGroupsY = (UINT)ceilf(m_Height / 16.0f);
	context.Dispatch(numGroupsX, numGroupsY, 1);

	context.Finish(true);
}
