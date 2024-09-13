#pragma once

#include "GpuResource.h"
#include <queue>

// constant blocks must be multiples of 16 constants @ 16 bytes each
#define DEFAULT_ALIGN 256

// various types of allocations may contain null pointers. check before dereferencing if you are unsure
struct DynAlloc
{
	DynAlloc(GpuResource& BaseResource, size_t ThisOffset, size_t ThisSize)
		: Buffer(BaseResource), Offset(ThisOffset), Size(ThisSize)
	{}

	GpuResource& Buffer; // the D3D buffer associated with this memory
	size_t Offset; // offset from start of buffer resource
	size_t Size; // reserved size of this allocation
	void* DataPtr; // CPU-writable address
	D3D12_GPU_VIRTUAL_ADDRESS GpuAddress; // GPU-visible address
};

class LinearAllocationPage : public GpuResource
{
public:

	LinearAllocationPage(ID3D12Resource* pResource, D3D12_RESOURCE_STATES Usage) : GpuResource()
	{
		m_pResource.Attach(pResource);
		m_UsageState = Usage;
		m_GpuVirtualAddress = m_pResource->GetGPUVirtualAddress();
		// map to Cpu address
		m_pResource->Map(0, nullptr, &m_CpuVirtualAddress);
	}

	~LinearAllocationPage()
	{
		Unmap();
	}


	void Map(void)
	{
		if (m_CpuVirtualAddress == nullptr)
		{
			m_pResource->Map(0, nullptr, &m_CpuVirtualAddress);
		}
	}

	void Unmap(void)
	{
		if (m_CpuVirtualAddress != nullptr)
		{
			m_pResource->Unmap(0, nullptr);
			m_CpuVirtualAddress = nullptr;
		}
	}

	void* m_CpuVirtualAddress;
	D3D12_GPU_VIRTUAL_ADDRESS m_GpuVirtualAddress;
};

enum LinearAllocatorType
{
	kInvaliadAllocator = -1,

	kGpuExclusive = 0, // default heap (GPU-visible)
	kCpuWritable = 1, // upload heap (CPU-visible)
	
	kNumAllocatorTypes
};

enum
{
	kGpuAllocatorPageSize = 0x10000,	// 64K
	kCpuAllocatorPageSize = 0x200000	// 2MB
};

class LinearAllocatorPageManager
{
public:
	LinearAllocatorPageManager();

	// request pages
	LinearAllocationPage* RequestPage(void);

	// create new page with size
	LinearAllocationPage* CreateNewPage(size_t PageSize = 0);

	// Discarded pages will get recycled.  This is for fixed size pages.
	void DiscardPages(uint64_t FenceID, const std::vector<LinearAllocationPage*>& Pages);

	// Freed pages will be destroyed once their fence has passed.  This is for single-use,
   // "large" pages.
	void FreeLargePages(uint64_t FenceValue, const std::vector<LinearAllocationPage*>& Pages);

	void Destroy(void) { m_PagePool.clear(); }

private:

	static LinearAllocatorType sm_AutoType;

	LinearAllocatorType m_AllocationType;
	std::vector<std::unique_ptr<LinearAllocationPage>> m_PagePool;
	std::queue<std::pair<uint64_t, LinearAllocationPage*>> m_RetiredPages;
	std::queue<std::pair<uint64_t, LinearAllocationPage*>> m_DeletionQueue;

	std::queue<LinearAllocationPage*> m_AvailablePages;
	std::mutex m_Mutex;
};


class LinearAllocator
{

public:
	LinearAllocator(LinearAllocatorType Type) : m_AllocationType(Type), m_PageSize(0), m_CurOffset(~(size_t)0),
		m_CurPage(nullptr)
	{
		// make sure the type correct
		ASSERT(Type > kInvaliadAllocator && Type < kNumAllocatorTypes);
		// assign the size of different type
		m_PageSize = (Type == kGpuExclusive ? kGpuAllocatorPageSize : kCpuAllocatorPageSize);
	}

	DynAlloc Allocate(size_t SizeInBytes, size_t Alignment = DEFAULT_ALIGN);

	void CleanupUsedPages(uint64_t FenceValue);

	static void DestroyAll(void)
	{
		sm_PageManager[0].Destroy();
		sm_PageManager[1].Destroy();
	}

private:

	DynAlloc AllocateLargePage(size_t SizeInBytes);

	// sm_PageManager[Typ]: 0->GPU , 1->CPU
	static LinearAllocatorPageManager sm_PageManager[2]; 

	LinearAllocatorType m_AllocationType;
	size_t m_PageSize;
	size_t m_CurOffset;
	LinearAllocationPage* m_CurPage;

	std::vector<LinearAllocationPage*> m_RetiredPages;
	std::vector<LinearAllocationPage*> m_LargePageList;
};

