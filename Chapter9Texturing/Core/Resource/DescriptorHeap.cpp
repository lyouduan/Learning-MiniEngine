#include "DescriptorHeap.h"
#include "GraphicsCore.h"

// declare the static members
std::mutex DescriptorAllocator::sm_AllocationMutex;
std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> DescriptorAllocator::sm_DescriptorHeapPool;

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorAllocator::Allocate(uint32_t Count)
{
    // m_CurrentHeap is null or not enough of heaps
    if (m_CurrentHeap == nullptr || m_RemainingFreeHandles < Count)
    {
        // allocate new Heaps to store the descriptor
        m_CurrentHeap = RequestNewHeap(m_Type);
        m_CurrentHandle = m_CurrentHeap->GetCPUDescriptorHandleForHeapStart();
        m_RemainingFreeHandles = sm_NumDescriptorsPerHeap;

        if (m_DescriptorSize == 0)
            m_DescriptorSize = Graphics::g_Device->GetDescriptorHandleIncrementSize(m_Type);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE ret = m_CurrentHandle;
    // offset the heap handle
    m_CurrentHandle.ptr += Count * m_DescriptorSize;
    m_RemainingFreeHandles -= Count;

    return ret;
}

void DescriptorAllocator::DestroyAll(void)
{
    sm_DescriptorHeapPool.clear();
}

ID3D12DescriptorHeap* DescriptorAllocator::RequestNewHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
    // create new heaps
    std::lock_guard<std::mutex> LockGuard(sm_AllocationMutex);

    D3D12_DESCRIPTOR_HEAP_DESC Desc;
    Desc.Type = Type;
    Desc.NumDescriptors = sm_NumDescriptorsPerHeap; // 256
    Desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    Desc.NodeMask = 1;

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pHeap;
    // need Device to create heap
    ASSERT_SUCCEEDED(Graphics::g_Device->CreateDescriptorHeap(&Desc, MY_IID_PPV_ARGS(&pHeap)));
    sm_DescriptorHeapPool.emplace_back(pHeap);

    return pHeap.Get();
}

//
// DescriptorHeap implementation
//
void DescriptorHeap::Create(const std::wstring& Name, D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t MaxCount)
{
    m_HeapDesc.Type = Type;
    m_HeapDesc.NumDescriptors = MaxCount;
    m_HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    m_HeapDesc.NodeMask = 1;

    // create DescriptorHeap
    ASSERT_SUCCEEDED(Graphics::g_Device->CreateDescriptorHeap(&m_HeapDesc, MY_IID_PPV_ARGS(m_Heap.ReleaseAndGetAddressOf())));

#ifdef RELEASE
    (void)Name;
#else
    m_Heap->SetName(Name.c_str());
#endif

    m_DescriptorSize = Graphics::g_Device->GetDescriptorHandleIncrementSize(m_HeapDesc.Type);
    m_NumFreeDescriptors = m_HeapDesc.NumDescriptors;
    m_FirstHandle = DescriptorHandle(
        m_Heap->GetCPUDescriptorHandleForHeapStart(),
        m_Heap->GetGPUDescriptorHandleForHeapStart());
    m_NextFreeHandle = m_FirstHandle;
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
    if (DHandle.GetCpuPtr() < m_FirstHandle.GetCpuPtr() ||
        DHandle.GetCpuPtr() >= m_FirstHandle.GetCpuPtr() + m_HeapDesc.NumDescriptors * m_DescriptorSize)
        return false;

    if (DHandle.GetGpuPtr() - m_FirstHandle.GetGpuPtr() !=
        DHandle.GetCpuPtr() - m_FirstHandle.GetCpuPtr())
        return false;

    return true;
}