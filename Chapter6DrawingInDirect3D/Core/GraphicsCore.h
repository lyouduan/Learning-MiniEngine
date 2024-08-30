#pragma once
#include "pch.h"
#include "CommandListManager.h"
#pragma once

#include "DescriptorHeap.h"
#include "ColorBuffer.h"

#define SWAP_CHAIN_BUFFER_COUNT 3


class CommandListManager;
class ContextManager;

namespace Graphics
{
	using namespace Microsoft::WRL;
	// Initialize the DirectX resource required to run
	// device, swapchain, rtv, dsv, command, descriptorheap
	void Initialize(bool RequireDXRSupport = false);
	void Shutdown(void);

	extern ID3D12Device* g_Device;
	extern CommandListManager g_CommandManager;
	extern ContextManager g_ContextManager;

	extern DescriptorAllocator g_DescriptorAllocator[];
	inline D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1)
	{
		return g_DescriptorAllocator[Type].Allocate(Count);
	}

	// temp

	extern IDXGISwapChain1* s_SwapChain1;
	extern ColorBuffer g_DisplayPlane[SWAP_CHAIN_BUFFER_COUNT];
	extern UINT g_CurrentBuffer;
};

