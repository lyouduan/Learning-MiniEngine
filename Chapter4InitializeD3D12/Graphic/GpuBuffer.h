#pragma once
#include "pch.h"
#include "GpuResource.h"

class GpuBuffer : public GpuResource
{
public:

protected:
	
	GpuBuffer(void) :m_BufferSize(0), m_ElementCount(0), m_ElementSize(0)
	{
		m_ResourceFlags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
		m_UAV.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
		m_SRV.ptr = D3D12_GPU_VIRTUAL_ADDRESS_UNKNOWN;
	}

	//D3D12_RESOURCE_DESC DescribeBuffer(void);
	virtual void CreateDerivedViews(void) = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE m_UAV;
	D3D12_CPU_DESCRIPTOR_HANDLE m_SRV;

	size_t m_BufferSize;
	uint32_t m_ElementCount;
	uint32_t m_ElementSize;
	D3D12_RESOURCE_FLAGS m_ResourceFlags;
};

