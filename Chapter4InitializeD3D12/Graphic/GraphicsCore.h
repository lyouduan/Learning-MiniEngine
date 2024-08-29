#pragma once
#include "pch.h"
#include "DescriptorHeap.h"

class CommandListManager;

namespace Graphics
{
#ifndef RELEASE
	extern const GUID WKPDID_D3DDebugObjectName;
#endif

	using namespace Microsoft::WRL;

	void Initialize(void);

	extern ID3D12Device* g_Device;
	extern CommandListManager g_CommandManager;

	extern DescriptorAllocator g_DescriptorAllocator[];
	inline D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1)
	{
		return g_DescriptorAllocator[Type].Allocate(Count);
	}
}
