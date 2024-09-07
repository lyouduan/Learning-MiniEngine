#include "pch.h"
#include "DynamicDescriptorHeap.h"
#include "GraphicsCore.h"
#include "RootSignature.h"
#include "CommandContext.h"
#include "CommandListManager.h"

using namespace Graphics;

//
// DynamicDescriptorHeap Implementation
//

// static members
std::mutex DynamicDescriptorHeap::sm_Mutex;
std::vector<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> DynamicDescriptorHeap::sm_DescriptorHeapPool[2];
std::queue<std::pair<uint64_t, ID3D12DescriptorHeap*>> DynamicDescriptorHeap::sm_RetiredDescriptorHeaps[2];
std::queue<ID3D12DescriptorHeap*> DynamicDescriptorHeap::sm_AvailableDescriptorHeaps[2];

ID3D12DescriptorHeap* DynamicDescriptorHeap::RequestDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE HeapType)
{
	std::lock_guard<std::mutex> LockGuard(sm_Mutex);

	uint32_t idx = HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ? 1 : 0;

	// obtain the retired heap, push into the available queue
	while (!sm_RetiredDescriptorHeaps[idx].empty() && g_CommandManager.IsFenceComplete(sm_RetiredDescriptorHeaps[idx].front().first))
	{
		sm_AvailableDescriptorHeaps[idx].push(sm_RetiredDescriptorHeaps[idx].front().second);
		sm_RetiredDescriptorHeaps[idx].pop();
	}

	if (!sm_AvailableDescriptorHeaps[idx].empty())
	{
		ID3D12DescriptorHeap* HeapPtr = sm_AvailableDescriptorHeaps[idx].front();
		sm_AvailableDescriptorHeaps[idx].pop();
		return HeapPtr;
	}
	else
	{
		// create new heaps
		D3D12_DESCRIPTOR_HEAP_DESC HeapDesc = {};
		HeapDesc.Type = HeapType;
		HeapDesc.NumDescriptors = kNumDescriptorsPerHeap;
		HeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		HeapDesc.NodeMask = 1;

		Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> HeapPtr;
		ASSERT_SUCCEEDED(g_Device->CreateDescriptorHeap(&HeapDesc, MY_IID_PPV_ARGS(&HeapPtr)));
		sm_DescriptorHeapPool[idx].emplace_back(HeapPtr);

		return HeapPtr.Get();
	}
}

void DynamicDescriptorHeap::DiscardDescriptorHeaps(D3D12_DESCRIPTOR_HEAP_TYPE HeapType, uint64_t FenceValue, const std::vector<ID3D12DescriptorHeap*>& UsedHeaps)
{
	std::lock_guard<std::mutex> LockGuard(sm_Mutex);

	uint32_t idx = HeapType == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ? 1 : 0;

	for (auto iter = UsedHeaps.begin(); iter != UsedHeaps.end(); ++iter)
		sm_RetiredDescriptorHeaps[idx].push(std::make_pair(FenceValue, *iter));
}

void DynamicDescriptorHeap::RetireCurrentHeap(void)
{
	// Don't retire unused heaps.
	if (m_CurrentOffset == 0)
	{
		ASSERT(m_CurrentHeapPtr == nullptr);
		return;
	}

	ASSERT(m_CurrentHeapPtr != nullptr);
	m_RetiredHeaps.push_back(m_CurrentHeapPtr);
	m_CurrentHeapPtr = nullptr;
	m_CurrentOffset = 0;
}

void DynamicDescriptorHeap::RetireUsedHeaps(uint64_t fenceValue)
{
	DiscardDescriptorHeaps(m_DescriptorType, fenceValue, m_RetiredHeaps);
	m_RetiredHeaps.clear();
}

DynamicDescriptorHeap::DynamicDescriptorHeap(CommandContext& OwningContext, D3D12_DESCRIPTOR_HEAP_TYPE HeapType)
	: m_OwningContext(OwningContext), m_DescriptorType(HeapType)
{
	m_CurrentHeapPtr = nullptr;
	m_CurrentOffset = 0;
	m_DescriptorSize = Graphics::g_Device->GetDescriptorHandleIncrementSize(HeapType);
}

DynamicDescriptorHeap::~DynamicDescriptorHeap()
{
}

void DynamicDescriptorHeap::CleanupUsedHeaps(uint64_t fenceValue)
{
	RetireCurrentHeap();
	RetireUsedHeaps(fenceValue);
	m_GraphicsHandleCache.ClearCache();
	m_ComputeHandleCache.ClearCache();
}

inline ID3D12DescriptorHeap* DynamicDescriptorHeap::GetHeapPointer()
{
	if (m_CurrentHeapPtr == nullptr)
	{
		ASSERT(m_CurrentOffset == 0);
		m_CurrentHeapPtr = RequestDescriptorHeap(m_DescriptorType);
		m_FirstDescriptor = DescriptorHandle(
			m_CurrentHeapPtr->GetCPUDescriptorHandleForHeapStart(),
			m_CurrentHeapPtr->GetGPUDescriptorHandleForHeapStart());
	}

	return m_CurrentHeapPtr;
}

// 计算需要的空间大小，用于存储旧的描述符表
uint32_t DynamicDescriptorHeap::DescriptorHandleCache::ComputeStagedSize()
{
	// Sum the maximum assigned offsets of stale descriptor tables to determine total needed space.
	uint32_t NeededSpace = 0;
	uint32_t RootIndex;
	uint32_t StaleParams = m_StaleRootParamsBitMap;
    // _BitScanReverse找到低位第一个设置为1的位
	while (_BitScanForward((unsigned long*)&RootIndex, StaleParams))
	{
        // 从位图中移除
		StaleParams ^= (1 << RootIndex);

        // 找到描述符表中分配的最大偏移量
		uint32_t MaxSetHandle;
		ASSERT(TRUE == _BitScanReverse((unsigned long*)&MaxSetHandle, m_RootDescriptorTable[RootIndex].AssignedHandlesBitMap),
			"Root entry marked as stale but has no stale descriptors");
        // 计算所需空间，并加到总空间中。+1 是因为索引是从 0 开始的。
		NeededSpace += MaxSetHandle + 1;
	}
	return NeededSpace;
}

// 缓存中旧的描述符表 重新上传到 GPU可见描述符堆，并绑定到 指定的根参数
void DynamicDescriptorHeap::DescriptorHandleCache::CopyAndBindStaleTables(
    D3D12_DESCRIPTOR_HEAP_TYPE Type, uint32_t DescriptorSize,
    DescriptorHandle DestHandleStart, ID3D12GraphicsCommandList* CmdList,
    void (STDMETHODCALLTYPE ID3D12GraphicsCommandList::* SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE))
{
    uint32_t StaleParamCount = 0; // 记录需要更新的描述符表数量
    uint32_t TableSize[DescriptorHandleCache::kMaxNumDescriptorTables]; //保存每个描述符表的大小（需要的描述符数量
    uint32_t RootIndices[DescriptorHandleCache::kMaxNumDescriptorTables]; //记录根参数索引
    uint32_t NeededSpace = 0; //计算所需的描述符空间
    uint32_t RootIndex; //用于存储当前遍历到的根参数索引

    // 遍历需要更新的描述符表
    // Sum the maximum assigned offsets of stale descriptor tables to determine total needed space.
    uint32_t StaleParams = m_StaleRootParamsBitMap;
    while (_BitScanForward((unsigned long*)&RootIndex, StaleParams))
    {
        RootIndices[StaleParamCount] = RootIndex;
        StaleParams ^= (1 << RootIndex);

        uint32_t MaxSetHandle;
        ASSERT(TRUE == _BitScanReverse((unsigned long*)&MaxSetHandle, m_RootDescriptorTable[RootIndex].AssignedHandlesBitMap),
            "Root entry marked as stale but has no stale descriptors");

        NeededSpace += MaxSetHandle + 1;
        TableSize[StaleParamCount] = MaxSetHandle + 1;

        ++StaleParamCount;
    }

    ASSERT(StaleParamCount <= DescriptorHandleCache::kMaxNumDescriptorTables,
        "We're only equipped to handle so many descriptor tables");

    // 清除位图，表示所有过期的描述符表将被更新
    m_StaleRootParamsBitMap = 0;

    // 一次最多可以复制 16 个描述符
    static const uint32_t kMaxDescriptorsPerCopy = 16;
    // 保存每个 目的地描述符 的起始位置和大小
    UINT NumDestDescriptorRanges = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE pDestDescriptorRangeStarts[kMaxDescriptorsPerCopy];
    UINT pDestDescriptorRangeSizes[kMaxDescriptorsPerCopy];

    // 保存每个 源描述符 的起始位置和大小
    UINT NumSrcDescriptorRanges = 0;
    D3D12_CPU_DESCRIPTOR_HANDLE pSrcDescriptorRangeStarts[kMaxDescriptorsPerCopy];
    UINT pSrcDescriptorRangeSizes[kMaxDescriptorsPerCopy];

    // 复制并绑定描述符
    for (uint32_t i = 0; i < StaleParamCount; ++i)
    {
        RootIndex = RootIndices[i];
        (CmdList->*SetFunc)(RootIndex, DestHandleStart);

        DescriptorTableCache& RootDescTable = m_RootDescriptorTable[RootIndex];

        D3D12_CPU_DESCRIPTOR_HANDLE* SrcHandles = RootDescTable.TableStart;
        uint64_t SetHandles = (uint64_t)RootDescTable.AssignedHandlesBitMap;
        D3D12_CPU_DESCRIPTOR_HANDLE CurDest = DestHandleStart;
        DestHandleStart += TableSize[i] * DescriptorSize;

        unsigned long SkipCount;
        while (_BitScanForward64(&SkipCount, SetHandles))
        {
            // Skip over unset descriptor handles
            SetHandles >>= SkipCount;
            SrcHandles += SkipCount;
            CurDest.ptr += SkipCount * DescriptorSize;

            unsigned long DescriptorCount;
            _BitScanForward64(&DescriptorCount, ~SetHandles);
            SetHandles >>= DescriptorCount;

            // If we run out of temp room, copy what we've got so far
            if (NumSrcDescriptorRanges + DescriptorCount > kMaxDescriptorsPerCopy)
            {
                //  CopyDescriptors; 用于批量复制描述符
                g_Device->CopyDescriptors(
                    NumDestDescriptorRanges, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes,
                    NumSrcDescriptorRanges, pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes,
                    Type);

                NumSrcDescriptorRanges = 0;
                NumDestDescriptorRanges = 0;
            }

            // Setup destination range
            pDestDescriptorRangeStarts[NumDestDescriptorRanges] = CurDest;
            pDestDescriptorRangeSizes[NumDestDescriptorRanges] = DescriptorCount;
            ++NumDestDescriptorRanges;

            // Setup source ranges (one descriptor each because we don't assume they are contiguous)
            for (uint32_t j = 0; j < DescriptorCount; ++j)
            {
                pSrcDescriptorRangeStarts[NumSrcDescriptorRanges] = SrcHandles[j];
                pSrcDescriptorRangeSizes[NumSrcDescriptorRanges] = 1;
                ++NumSrcDescriptorRanges;
            }

            // Move the destination pointer forward by the number of descriptors we will copy
            SrcHandles += DescriptorCount;
            CurDest.ptr += DescriptorCount * DescriptorSize;
        }
    }

    g_Device->CopyDescriptors(
        NumDestDescriptorRanges, pDestDescriptorRangeStarts, pDestDescriptorRangeSizes,
        NumSrcDescriptorRanges, pSrcDescriptorRangeStarts, pSrcDescriptorRangeSizes,
        Type);
}

// 检查是否有足够 动态描述符堆的空间 来更新 旧描述符表
void DynamicDescriptorHeap::CopyAndBindStagedTables(DescriptorHandleCache& HandleCache, ID3D12GraphicsCommandList* CmdList,
        void (STDMETHODCALLTYPE ID3D12GraphicsCommandList::* SetFunc)(UINT, D3D12_GPU_DESCRIPTOR_HANDLE))
{
    // 计算缓存中待上传的描述符表所需空间大小
    uint32_t NeededSize = HandleCache.ComputeStagedSize();
    // 检查是否有足够空间
    if (!HasSpace(NeededSize))
    {
        // 释放 退役的堆
        RetireCurrentHeap();
        // 解绑所有有效的描述符表，确保后续绑定新的描述符
        UnbindAllValid();
        // 重新计算空间大小，因为在释放退役的堆可能会产生更多的旧描述符
        NeededSize = HandleCache.ComputeStagedSize();
    }

    // This can trigger the creation of a new heap
    m_OwningContext.SetDescriptorHeap(m_DescriptorType, GetHeapPointer());
    // 复制到到GPU
    HandleCache.CopyAndBindStaleTables(m_DescriptorType, m_DescriptorSize, Allocate(NeededSize), CmdList, SetFunc);
}

void DynamicDescriptorHeap::UnbindAllValid(void)
{
    m_GraphicsHandleCache.UnbindAllValid();
    m_ComputeHandleCache.UnbindAllValid();
}

// 用于直接从CPU上传描述符到GPU中
D3D12_GPU_DESCRIPTOR_HANDLE DynamicDescriptorHeap::UploadDirect(D3D12_CPU_DESCRIPTOR_HANDLE Handle)
{
    if (!HasSpace(1))
    {
        RetireCurrentHeap();
        UnbindAllValid();
    }

    m_OwningContext.SetDescriptorHeap(m_DescriptorType, GetHeapPointer());

    DescriptorHandle DestHandle = m_FirstDescriptor + m_CurrentOffset * m_DescriptorSize;
    m_CurrentOffset += 1;

    // CopyDescriptorsSimple用于将单个描述符从源位置（CPU 端）复制到目标位置（GPU 端）
    g_Device->CopyDescriptorsSimple(1, DestHandle, Handle, m_DescriptorType);

    return DestHandle;
}

void DynamicDescriptorHeap::DescriptorHandleCache::UnbindAllValid()
{
    m_StaleRootParamsBitMap = 0;

    unsigned long TableParams = m_RootDescriptorTablesBitMap;
    unsigned long RootIndex;
    while (_BitScanForward(&RootIndex, TableParams))
    {
        TableParams ^= (1 << RootIndex);
        if (m_RootDescriptorTable[RootIndex].AssignedHandlesBitMap != 0)
            m_StaleRootParamsBitMap |= (1 << RootIndex);
    }
}

// 用于将一组CPU端的描述符缓存到描述符表中，后续再上传到GPU
// 通过缓存机制提高效率
void DynamicDescriptorHeap::DescriptorHandleCache::StageDescriptorHandles(UINT RootIndex, UINT Offset, UINT NumHandles, const D3D12_CPU_DESCRIPTOR_HANDLE Handles[])
{
    // 检查描述符表是否为CBV_SRV_UAV
    ASSERT(((1 << RootIndex) & m_RootDescriptorTablesBitMap) != 0, "Root parameter is not a CBV_SRV_UAV descriptor table");
    ASSERT(Offset + NumHandles <= m_RootDescriptorTable[RootIndex].TableSize);

    // 获取RootIndex对应的描述符表缓存
    DescriptorTableCache& TableCache = m_RootDescriptorTable[RootIndex];
    // 计算出写入描述符的缓存位置：起始位置+偏移
    D3D12_CPU_DESCRIPTOR_HANDLE* CopyDest = TableCache.TableStart + Offset;
    // 复制到缓存位置中
    for (UINT i = 0; i < NumHandles; ++i)
        CopyDest[i] = Handles[i];
    // 更新缓存描述符表的位图
    TableCache.AssignedHandlesBitMap |= ((1 << NumHandles) - 1) << Offset;
    // 标记根参数为过期，表示需要修改
    m_StaleRootParamsBitMap |= (1 << RootIndex);
}

// 解析根签名，将描述符表信息存储到 描述符表缓存 中
void DynamicDescriptorHeap::DescriptorHandleCache::ParseRootSignature(D3D12_DESCRIPTOR_HEAP_TYPE Type, const RootSignature& RootSig)
{
    UINT CurrentOffset = 0;
    // 检查根签名中的参数数量是否小于等于 16
    ASSERT(RootSig.m_NumParameters <= 16, "Maybe we need to support something greater");

    // 初始化根参数位图
    m_StaleRootParamsBitMap = 0;
    // 根据描述符堆类型选择位图，
    m_RootDescriptorTablesBitMap = (Type == D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER ?
        RootSig.m_SamplerTableBitMap : RootSig.m_DescriptorTableBitMap);

    // 寻找位图标记为1的位
    unsigned long TableParams = m_RootDescriptorTablesBitMap;
    unsigned long RootIndex;
    while (_BitScanForward(&RootIndex, TableParams))
    {
        TableParams ^= (1 << RootIndex);

        // 从根签名中获取当前根参数的描述符表的大小
        UINT TableSize = RootSig.m_DescriptorTableSize[RootIndex];
        ASSERT(TableSize > 0);

        // 获取当前根参数的描述符表缓存
        DescriptorTableCache& RootDescriptorTable = m_RootDescriptorTable[RootIndex];
        RootDescriptorTable.AssignedHandlesBitMap = 0;
        RootDescriptorTable.TableStart = m_HandleCache + CurrentOffset;
        RootDescriptorTable.TableSize = TableSize;

        CurrentOffset += TableSize;
    }

    // 记录所有根参数的描述符表所需的描述符总数
    m_MaxCachedDescriptors = CurrentOffset;

    ASSERT(m_MaxCachedDescriptors <= kMaxNumDescriptors, "Exceeded user-supplied maximum cache size");
}



