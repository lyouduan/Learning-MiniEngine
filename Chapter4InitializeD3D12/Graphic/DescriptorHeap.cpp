#include "DescriptorHeap.h"
#include "GraphicsCore.h"


using namespace Graphics;

//
// DescriptorAllocator implementation
//
// staic outer declare
std::mutex DescriptorAllocator::sm_AllocationMutex;
std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> DescriptorAllocator::sm_DescriptorHeapPool;

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::Allocate(uint32_t Count)
{
    // 
    if (m_CurrentHeap == nullptr || m_RemaingFreeHandles < Count)
    {
        m_CurrentHeap = RequestNewHeap(m_Type);
        m_CurrentHandle = m_CurrentHeap->GetCPUDescriptorHandleForHeapStart();
        m_RemaingFreeHandles = sm_NumDescritporsPerHeap;
        if (m_DescriptorSize == 0)
            m_DescriptorSize = Graphics::g_Device->GetDescriptorHandleIncrementSize(m_Type);
    }

    // caclucate current descriptor info
    D3D12_CPU_DESCRIPTOR_HANDLE ret = m_CurrentHandle;
    m_CurrentHandle.ptr += Count * m_DescriptorSize;
    m_RemaingFreeHandles -= Count;

    return ret;
}

void DescriptorAllocator::DestroyAll(void)
{
    sm_DescriptorHeapPool.clear();
}

ID3D12DescriptorHeap* DescriptorAllocator::RequestNewHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
    // make multi-threads safe
    std::lock_guard<std::mutex> LockGuard(sm_AllocationMutex);

    D3D12_DESCRIPTOR_HEAP_DESC Desc;
    Desc.Type = Type;
    Desc.NumDescriptors = sm_NumDescritporsPerHeap;
    Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    Desc.NodeMask = 0;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pHeap;
    ASSERT_SUCCEEDED(Graphics::g_Device->CreateDescriptorHeap(&Desc, MY_IID_PPV_ARGS(&pHeap)));
    
    sm_DescriptorHeapPool.emplace_back(pHeap);
    
    return pHeap.Get();
}

void DescriptorHeap::Create(const std::wstring& Name, D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t MaxCount)
{
    m_HeapDesc.Type = Type;
    m_HeapDesc.NumDescriptors = MaxCount;
    m_HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    m_HeapDesc.NodeMask = 0;

    ASSERT_SUCCEEDED(Graphics::g_Device->CreateDescriptorHeap(&m_HeapDesc, MY_IID_PPV_ARGS(m_Heap.ReleaseAndGetAddressOf())));
#ifdef RELEASE
    (void)Name;
#else
    m_Heap->SetName(Name.c_str());
#endif

    m_DescriptorSize = Graphics::g_Device->GetDescriptorHandleIncrementSize(Type);
    m_NumFreeDescriptors = m_HeapDesc.NumDescriptors;
    m_FristHandle = DescriptorHandle(m_Heap->GetCPUDescriptorHandleForHeapStart(), m_Heap->GetGPUDescriptorHandleForHeapStart());

    m_NextFreeHandle = m_FristHandle;
}

DescriptorHandle DescriptorHeap::Alloc(uint32_t Count)
{
    ASSERT(HasAvailableSpace(Count), "Descriptor Heap out of space.  Increase heap size.");
    DescriptorHandle ret = m_NextFreeHandle;
    m_NextFreeHandle += Count * m_DescriptorSize;
    m_NumFreeDescriptors -= Count;

    return ret;
}

bool DescriptorHeap::ValidateHandle(const DescriptorHandle& DHandle) const
{
    if (DHandle.GetCpuPtr() < m_FristHandle.GetCpuPtr() ||
        DHandle.GetCpuPtr() >= m_FristHandle.GetCpuPtr() + m_HeapDesc.NumDescriptors * m_DescriptorSize)
        return false;

    if (DHandle.GetGpuPtr() - m_FristHandle.GetGpuPtr() != DHandle.GetCpuPtr() - m_FristHandle.GetCpuPtr())
        return false;

    return true;
}
