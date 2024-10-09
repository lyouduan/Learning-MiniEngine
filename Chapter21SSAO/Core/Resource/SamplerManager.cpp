#include "SamplerManager.h"
#include "Hash.h"
#include "GraphicsCore.h"
#include <map>

namespace
{
    std::map< size_t, D3D12_CPU_DESCRIPTOR_HANDLE > s_SamplerCache;
}

D3D12_CPU_DESCRIPTOR_HANDLE SamplerDesc::CreateDescriptor(void)
{
    size_t hashValue = Utility::HashState(this);
    auto iter = s_SamplerCache.find(hashValue);
    if (iter != s_SamplerCache.end())
    {
        return iter->second;
    }
    
    D3D12_CPU_DESCRIPTOR_HANDLE Handle = Graphics::AllocateDescriptor(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
    Graphics::g_Device->CreateSampler(this, Handle);

    return Handle;
}

void SamplerDesc::CreateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE Handle)
{
    ASSERT(Handle.ptr != 0 && Handle.ptr != -1);
    Graphics::g_Device->CreateSampler(this, Handle);
}
