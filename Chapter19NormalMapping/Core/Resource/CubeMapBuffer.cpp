#include "CubeMapBuffer.h"
#include "GraphicsCore.h"

void CubeMapBuffer::Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips, DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr)
{
	D3D12_RESOURCE_DESC ResourceDesc = DescribeTex2D(Width, Height, 6, NumMips, Format, D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET); // six face

	ResourceDesc.SampleDesc.Count = 1;
	ResourceDesc.SampleDesc.Quality = 0;

	D3D12_CLEAR_VALUE ClearValue = {};
	ClearValue.Format = Format;
	ClearValue.Color[0] = m_ClearColor.R();
	ClearValue.Color[1] = m_ClearColor.G();
	ClearValue.Color[2] = m_ClearColor.B();
	ClearValue.Color[3] = m_ClearColor.A();

	CreateTextureResource(Graphics::g_Device, Name, ResourceDesc, ClearValue, VidMemPtr);
	CreateDerivedViews(Graphics::g_Device, Format, 1, NumMips);
}

void CubeMapBuffer::CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips)
{
	ID3D12Resource* Resource = m_pResource.Get();

	D3D12_RENDER_TARGET_VIEW_DESC RTVDesc = {};
	D3D12_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};

	
	SRVDesc.Format = Format;
	SRVDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	SRVDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE; // cube map
	SRVDesc.TextureCube.MipLevels = 1;
	SRVDesc.TextureCube.MostDetailedMip = 0;

	// cpu visible descriptor heap 
	if (m_SRVHandle.ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
	{
		m_SRVHandle = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	}

	// Create the shader resource view
	Device->CreateShaderResourceView(Resource, &SRVDesc, m_SRVHandle);

	RTVDesc.Format = Format;
	RTVDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
	RTVDesc.Texture2DArray.ArraySize = 1;
	RTVDesc.Texture2DArray.MipSlice = 0;
	// Create the render target view
	for (uint32_t i = 0; i < 6; ++i)
	{
		RTVDesc.Texture2DArray.FirstArraySlice = i;
		if (m_RTVHandle[i].ptr == D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN)
			m_RTVHandle[i] = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

		Device->CreateRenderTargetView(Resource, &RTVDesc, m_RTVHandle[i]);
	}
}
