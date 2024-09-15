#pragma once
#include "pch.h"
#include "DescriptorHeap.h"
#define SWAP_CHAIN_BUFFER_COUNT 3

class CommandListManager;
class ContextManager;
class ColorBuffer;

namespace Graphics
{
    using namespace Microsoft::WRL;

    void Initialize(bool RequireDXRSupport = false);
    void Shutdown(void);

    extern ID3D12Device* g_Device;
    extern CommandListManager g_CommandManager;
    extern ContextManager g_ContextManager;;

    extern DescriptorAllocator g_DescriptorAllocator[];
    inline D3D12_CPU_DESCRIPTOR_HANDLE AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT Count = 1)
    {
        return g_DescriptorAllocator[Type].Allocate(Count);
    }

    extern ColorBuffer g_DisplayPlane[SWAP_CHAIN_BUFFER_COUNT];
    extern UINT g_CurrentBuffer;
}
