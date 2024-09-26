#pragma once
#include "GpuBuffer.h"
#include "ColorBuffer.h"
#include "Color.h"

class CubeMapBuffer : public PixelBuffer
{
public:
	CubeMapBuffer(Color ClearColor = Color(0.0f, 0.0f, 0.0f, 0.0f)) :
		m_ClearColor(ClearColor), m_NumMipMaps(0),m_SamleCount(1)
	{
		m_SRVHandle.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
		for(int i =0; i < _countof(m_RTVHandle); ++i)
			m_RTVHandle[i].ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
	}

	void Create(const std::wstring& Name, uint32_t Width, uint32_t Height, uint32_t NumMips,
		DXGI_FORMAT Format, D3D12_GPU_VIRTUAL_ADDRESS VidMemPtr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN);


	// Get pre-created CPU-visible descriptor handles
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetSRV(void) const { return m_SRVHandle; }
	const D3D12_CPU_DESCRIPTOR_HANDLE& GetRTV(int i) const { return m_RTVHandle[i]; }

	void SetClearColor(Color ClearColor) { m_ClearColor = ClearColor; }

	Color GetClearColor(void) const { return m_ClearColor; }

private:

	void CreateDerivedViews(ID3D12Device* Device, DXGI_FORMAT Format, uint32_t ArraySize, uint32_t NumMips = 1);

	Color m_ClearColor;

	D3D12_CPU_DESCRIPTOR_HANDLE m_SRVHandle;
	D3D12_CPU_DESCRIPTOR_HANDLE m_RTVHandle[6];
	uint32_t m_NumMipMaps;
	uint32_t m_SamleCount;
};

