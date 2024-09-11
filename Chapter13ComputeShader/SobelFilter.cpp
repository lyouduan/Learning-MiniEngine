#include "SobelFilter.h"
#include "CommandContext.h"

void SobelFilter::Initialize(const std::wstring& name, UINT width, UINT height, DXGI_FORMAT format, const Microsoft::WRL::ComPtr<ID3DBlob>& sobelCS, const Microsoft::WRL::ComPtr<ID3DBlob>& addCS)
{
	m_Width = width;
	m_Height = height;
	m_Format = format;

	// create buffer
	m_AddBuffer.Create(name, width, height, 1, format);
	m_SobelBuffer.Create(name, width, height, 1, format);

	// set root signature
	m_RootSig.Reset(2, 0);
	m_RootSig[0].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	m_RootSig[1].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 0, 1);
	m_RootSig.Finalize(L"sobel root signature");

	// set pso
	m_PSOSobel.SetRootSignature(m_RootSig);
	m_PSOSobel.SetComputeShader(sobelCS);
	m_PSOSobel.Finalize();

	m_PSOAdd.SetRootSignature(m_RootSig);
	m_PSOAdd.SetComputeShader(addCS);
	m_PSOAdd.Finalize();
}

void SobelFilter::Execute(GpuResource& in)
{
	ComputeContext& context = ComputeContext::Begin(L"sobel filter");

	// set root signature
	context.SetRootSignature(m_RootSig);

	context.CopyBuffer(m_AddBuffer, in);
	// set pso
	context.SetPipelineState(m_PSOSobel);

	context.TransitionResource(m_AddBuffer, D3D12_RESOURCE_STATE_GENERIC_READ);
	context.TransitionResource(m_SobelBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);

	context.SetDynamicDescriptor(0, 0, m_AddBuffer.GetSRV());
	context.SetDynamicDescriptor(1, 0, m_SobelBuffer.GetUAV());

	UINT numGroupsX = (UINT)ceilf(m_Width / 16.0f);
	UINT numGroupsY = (UINT)ceilf(m_Height / 16.0f);
	context.Dispatch(numGroupsX, numGroupsY, 1);

	// blend together
	context.SetPipelineState(m_PSOAdd);
	context.SetRootSignature(m_RootSig);
	context.TransitionResource(m_SobelBuffer, D3D12_RESOURCE_STATE_GENERIC_READ);
	context.TransitionResource(m_AddBuffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	
	context.SetDynamicDescriptor(0, 0, m_SobelBuffer.GetSRV());
	context.SetDynamicDescriptor(1, 0, m_AddBuffer.GetUAV());
	
	numGroupsX = (UINT)ceilf(m_Width / 256.0f);
	context.Dispatch(numGroupsX, m_Height, 1);


	context.Finish(true);
}
