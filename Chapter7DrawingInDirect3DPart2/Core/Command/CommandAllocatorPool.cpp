#include "CommandAllocatorPool.h"

CommandAllocatorPool::CommandAllocatorPool(D3D12_COMMAND_LIST_TYPE Type)
	: m_cCommandListType(Type),
	m_Device(nullptr)
{
}

CommandAllocatorPool::~CommandAllocatorPool()
{
}

void CommandAllocatorPool::Create(ID3D12Device* pDevice)
{
	m_Device = pDevice;
}

void CommandAllocatorPool::Shutdown()
{
	// release the allocator and clear pool vector
	for (size_t i = 0; i < m_AllocatorPool.size(); ++i)
		m_AllocatorPool[i]->Release();

	m_AllocatorPool.clear();
}

ID3D12CommandAllocator* CommandAllocatorPool::RequestAllocator(uint64_t CompletedFenceValue)
{
	// create allocator and add to pool
	// first find in ready allocator, if not supply, new a one

	// use mutex avoid resource compete
	std::lock_guard<std::mutex> LockGuard(m_AllocatorMutex);

	ID3D12CommandAllocator* pAllocator = nullptr;
	if (!m_ReadyAllocators.empty())
	{
		std::pair<uint64_t, ID3D12CommandAllocator*>& AllocatorPair = m_ReadyAllocators.front();

		// make sure the commandlist in Allocator completed, then allocate it
		if (AllocatorPair.first <= CompletedFenceValue)
		{
			pAllocator = AllocatorPair.second;
			ASSERT_SUCCEEDED(pAllocator->Reset());
			m_ReadyAllocators.pop();
		}
	}

	if (pAllocator == nullptr)
	{
		// create a new allocator and add to pool
		ASSERT_SUCCEEDED(m_Device->CreateCommandAllocator(m_cCommandListType, MY_IID_PPV_ARGS(&pAllocator)));
		wchar_t AllocatorName[32];
		swprintf(AllocatorName, 32, L"CommandAllocator %zu", m_AllocatorPool.size());
		pAllocator->SetName(AllocatorName);
		m_AllocatorPool.push_back(pAllocator);
	}
	

	return pAllocator;
}

void CommandAllocatorPool::DiscardAllocator(uint64_t FenceValue, ID3D12CommandAllocator* Allocator)
{
	std::lock_guard<std::mutex> LockGuard(m_AllocatorMutex);

	// fence value use to make sure commandlist in allocator has been completed.
	m_ReadyAllocators.push(std::make_pair(FenceValue, Allocator));
}
