#pragma once

#include "pch.h"
#include "CommandListManager.h"
#include "GpuResource.h"
#include "ColorBuffer.h"
#include "DepthBuffer.h"

#include <vector>
#include <queue>

class CommandContext;
class GraphicsContext;

#define VALID_COMPUTE_QUEUE_RESOURCE_STATES \
    ( D3D12_RESOURCE_STATE_UNORDERED_ACCESS \
    | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE \
    | D3D12_RESOURCE_STATE_COPY_DEST \
    | D3D12_RESOURCE_STATE_COPY_SOURCE )

class ContextManager
{
public:
	ContextManager(void){}

	CommandContext* AllocateContext(D3D12_COMMAND_LIST_TYPE Type);
	void FreeContext(CommandContext*);
	void DestroyAllContexts();

private:
	std::vector<std::unique_ptr<CommandContext>> sm_ContextPool[4];
	std::queue<CommandContext*> sm_AvailableContexts[4];
	std::mutex sm_ContextAllocationMutex;
};

struct NonCopyable
{
	NonCopyable() = default;
	NonCopyable(const NonCopyable&) = delete;
	NonCopyable& operator=(const NonCopyable&) = delete;
};

// general command
class CommandContext : NonCopyable
{
	friend ContextManager;

private:

	CommandContext(D3D12_COMMAND_LIST_TYPE Type);

	void Reset(void);

public:
	~CommandContext(void);
	static void DestroyAllContexts(void);

	static CommandContext& Begin(const std::wstring ID = L"");

	// Flush existing commands to the GPU but keep the context alive
	uint64_t Flush(bool WaitForCompletion = false);

	// Flush existing commands and release the current context
	uint64_t Finish(bool WaitForCompletion = false);

	// Prepare to render by reserving a command list and command allocator
	void Initialize(void);

	GraphicsContext& GetGraphicsContext() {
		ASSERT(m_Type != D3D12_COMMAND_LIST_TYPE_COMPUTE, "Cannot convert async compute context to graphics");
		return reinterpret_cast<GraphicsContext&>(*this);
	}


	ID3D12GraphicsCommandList* GetCommandList() {
		return m_CommandList;
	}

	inline void CopyBuffer(GpuResource& Dest, GpuResource& Src);
	inline void CopyBufferRegion(GpuResource& Dest, size_t DestOffset, GpuResource& Src, size_t SrcOffset, size_t NumBytes);

	inline void CopySubresource(GpuResource& Dest, UINT DestSubIndex, GpuResource& Src, UINT SrcSubIndex);

	void TransitionResource(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);
	void BeginResourceTransition(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);
	void InsertUAVBarrier(GpuResource& Resource, bool FlushImmediate = false);
	void InsertAliasBarrier(GpuResource& Before, GpuResource& After, bool FlushImmediate = false);
	inline void FlushResourceBarriers(void);

	void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* HeapPtr);
	void SetDescriptorHeaps(UINT HeapCount, D3D12_DESCRIPTOR_HEAP_TYPE Type[], ID3D12DescriptorHeap* HeapPtrs[]);

protected:

	void BindDescriptorHeaps(void);

	CommandListManager* m_OwningManager;
	ID3D12GraphicsCommandList* m_CommandList;
	ID3D12CommandAllocator* m_CurrentAllocator;

	D3D12_RESOURCE_BARRIER m_ResourceBarrierBuffer[16];
	UINT m_NumBarriersToFlush;

	ID3D12DescriptorHeap* m_CurrentDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	std::wstring m_ID;
	void SetID(const std::wstring& ID) { m_ID = ID; }

	D3D12_COMMAND_LIST_TYPE m_Type;
};

class GraphicsContext : public CommandContext
{
public:
	static GraphicsContext& Begin(const std::wstring& ID = L"")
	{
		return CommandContext::Begin(ID).GetGraphicsContext();
	}

	void ClearColor(ColorBuffer& Target, D3D12_RECT* Rect = nullptr);
	void ClearColor(ColorBuffer& Target, float Colour[4], D3D12_RECT* Rect = nullptr);
	void ClearDepth(DepthBuffer& Target);
	void ClearStencil(DepthBuffer& Target);
	void ClearDepthAndStencil(DepthBuffer& Target);


	void SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[]);
	void SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[], D3D12_CPU_DESCRIPTOR_HANDLE DSV);
	void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV) { SetRenderTargets(1, &RTV); }
	void SetRenderTarget(D3D12_CPU_DESCRIPTOR_HANDLE RTV, D3D12_CPU_DESCRIPTOR_HANDLE DSV) { SetRenderTargets(1, &RTV, DSV); }
	void SetDepthStencilTarget(D3D12_CPU_DESCRIPTOR_HANDLE DSV) { SetRenderTargets(0, nullptr, DSV); }

	void SetViewport(const D3D12_VIEWPORT& vp);
	void SetViewport(FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT minDepth = 0.0f, FLOAT maxDepth = 1.0f);
	void SetScissor(const D3D12_RECT& rect);
	void SetScissor(UINT left, UINT top, UINT right, UINT bottom);
	void SetViewportAndScissor(const D3D12_VIEWPORT& vp, const D3D12_RECT& rect);
	void SetViewportAndScissor(UINT x, UINT y, UINT w, UINT h);


private:


};
