#include "CommandListManager.h"
#include "GraphicsCore.h"


CommandQueue::CommandQueue(D3D12_COMMAND_LIST_TYPE Type) :
	m_Type(Type),
	m_CommandQueue(nullptr),
	m_pFence(nullptr),
	m_NextFenceValue((uint64_t)Type << 56 | 1),
	m_LastCompletedFenceValue((uint64_t)Type << 56),
	m_AllocatorPool(Type)
{
}

CommandQueue::~CommandQueue()
{
	ShutDown();
}

void CommandQueue::Create(ID3D12Device* pDevice)
{
	ASSERT(pDevice != nullptr);
	ASSERT(!IsReady());
	ASSERT(m_AllocatorPool.Size() == 0);
	
	// create commandQueue
	D3D12_COMMAND_QUEUE_DESC QueueDesc = {};
	QueueDesc.Type = m_Type;
	QueueDesc.NodeMask = 0;
	pDevice->CreateCommandQueue(&QueueDesc, MY_IID_PPV_ARGS(&m_CommandQueue));
	m_CommandQueue->SetName(L"CommandListManager::m_CommandQueue");

	// create fence
	ASSERT_SUCCEEDED(pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, MY_IID_PPV_ARGS(&m_pFence)));
	m_pFence->SetName(L"CommandListManager::m_pFence");
	m_pFence->Signal((uint64_t)m_Type << 56);

	// fenceEvent
	m_FenceEventHandle = CreateEvent(nullptr, false, false, nullptr);
	ASSERT(m_FenceEventHandle != NULL);

	// create allocator for commandList
	m_AllocatorPool.Create(pDevice);

	ASSERT(IsReady());
}

void CommandQueue::ShutDown()
{
}

uint64_t CommandQueue::IncrementFence(void)
{
	std::lock_guard<std::mutex> LockGuaed(m_FenceMutex);

	// CommandQueue signal new fence
	m_CommandQueue->Signal(m_pFence, m_NextFenceValue);

	return m_NextFenceValue++;
}

bool CommandQueue::IsFenceComplete(uint64_t FenceValue)
{
	if (FenceValue > m_LastCompletedFenceValue)
		m_LastCompletedFenceValue = std::max(m_LastCompletedFenceValue, m_pFence->GetCompletedValue());

	return FenceValue <= m_LastCompletedFenceValue;
}

namespace Graphics
{
	extern CommandListManager g_CommandManager;
}

void CommandQueue::StallForFence(uint64_t FenceValue)
{
	CommandQueue& Producer = Graphics::g_CommandManager.GetQueue((D3D12_COMMAND_LIST_TYPE)(FenceValue >> 56));
	m_CommandQueue->Wait(Producer.m_pFence, FenceValue);
}

void CommandQueue::StallForProducer(CommandQueue& Producer)
{
	ASSERT(Producer.m_NextFenceValue > 0);

	// wait for event completed
	m_CommandQueue->Wait(m_pFence, Producer.m_NextFenceValue - 1);
}

void CommandQueue::WaitForFence(uint64_t FenceValue)
{
	if (IsFenceComplete(FenceValue))
		return;

	{
		std::lock_guard<std::mutex> LockGuard(m_EventMutex);
		m_pFence->SetEventOnCompletion(FenceValue, m_FenceEventHandle);
		WaitForSingleObject(m_FenceEventHandle, INFINITE);
		m_LastCompletedFenceValue = FenceValue;
	}
}

uint64_t CommandQueue::ExecuteCommandList(ID3D12CommandList* List)
{
	std::lock_guard<std::mutex> LockGuard(m_FenceMutex);

	ASSERT_SUCCEEDED(((ID3D12GraphicsCommandList*)List)->Close());

	// kickoff the command list
	m_CommandQueue->ExecuteCommandLists(1, &List);
	
	// signal the next fence value(with the GPU)
	m_CommandQueue->Signal(m_pFence, m_NextFenceValue);

	// and increment the fence value
	return m_NextFenceValue++;
}

ID3D12CommandAllocator* CommandQueue::RequestAllocator(void)
{
	uint64_t CompletedFnceValue = m_pFence->GetCompletedValue();

	return m_AllocatorPool.RequestAllocator(CompletedFnceValue);
}

void CommandQueue::DiscardAllocator(uint64_t FenceValueForReset, ID3D12CommandAllocator* Allocator)
{
	m_AllocatorPool.DiscardAllocator(FenceValueForReset, Allocator);
}

CommandListManager::CommandListManager() :
	m_Device(nullptr),
	m_GraphicsQueue(D3D12_COMMAND_LIST_TYPE_DIRECT),
	m_ComputeQueue(D3D12_COMMAND_LIST_TYPE_COMPUTE),
	m_CopyQueue(D3D12_COMMAND_LIST_TYPE_COPY)
{
}

CommandListManager::~CommandListManager()
{
	ShutDown();
}

void CommandListManager::Create(ID3D12Device* pDevice)
{
	ASSERT(pDevice != nullptr);

	m_Device = pDevice;
	m_GraphicsQueue.Create(pDevice);
	m_ComputeQueue.Create(pDevice);
	m_CopyQueue.Create(pDevice);
}

void CommandListManager::ShutDown()
{
	m_GraphicsQueue.ShutDown();
	m_ComputeQueue.ShutDown();
	m_CopyQueue.ShutDown();
}

void CommandListManager::CreateNewCommandList(D3D12_COMMAND_LIST_TYPE Type, ID3D12GraphicsCommandList** List, ID3D12CommandAllocator** Allocator)
{
	ASSERT(Type != D3D12_COMMAND_LIST_TYPE_BUNDLE, "Bundles are not yet supported");

	switch (Type)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT: *Allocator = m_GraphicsQueue.RequestAllocator(); break;
	case D3D12_COMMAND_LIST_TYPE_BUNDLE: break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE: *Allocator = m_ComputeQueue.RequestAllocator(); break;
	case D3D12_COMMAND_LIST_TYPE_COPY: *Allocator = m_CopyQueue.RequestAllocator(); break;
	}

	ASSERT_SUCCEEDED(m_Device->CreateCommandList(0, Type, *Allocator, nullptr, MY_IID_PPV_ARGS(List)));
	(*List)->SetName(L"CommandList");
}

void CommandListManager::WaitForFence(uint64_t FenceValue)
{
	CommandQueue& Producer = Graphics::g_CommandManager.GetQueue((D3D12_COMMAND_LIST_TYPE)(FenceValue >> 56));
	Producer.WaitForFence(FenceValue);
}
