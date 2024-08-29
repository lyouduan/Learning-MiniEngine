#pragma once
#include "pch.h"

class ColorBuffer;
class GraphicsContext;
class CommandContext;
class GpuResource;

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
    void DestroyAllContexts(void);

private:
    std::vector<std::unique_ptr<CommandContext> > sm_ContextPool[4];
    std::queue<CommandContext*> sm_AvailableContexts[4];
    std::mutex sm_ContextAllocationMutex;
};

// class don't allow copy or copy operator
struct NonCopyable
{
    NonCopyable() = default;
    NonCopyable(const NonCopyable&) = delete;
    NonCopyable& operator=(const NonCopyable&) = delete;
};

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

    // flush existing commands to the GPU but keep the context alive
    uint64_t Flush(bool WaitForCompletion = false);

    // flush existing commands and release the current context
    uint64_t Finish(bool WaitForCompletion = false);

    // prepare to render by reserving a command list and command allocator
    void Initialize(void);


    GraphicsContext& GetGraphicsContext()
    {
        ASSERT(m_Type != D3D12_COMMAND_LIST_TYPE_COMPUTE, "Cannot convert async compute context to graphics");
        return reinterpret_cast<GraphicsContext&>(*this);
    }

    ID3D12GraphicsCommandList* GetCommandList() {
        return m_CommandList;
    }

    void TransitionResource(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate = false);

    inline void FlushResourceBarriers(void);

protected:
    ID3D12GraphicsCommandList* m_CommandList;
    ID3D12CommandAllocator* m_CurrentAllocator;

    std::wstring m_ID;
    void SetID(const std::wstring& ID) { m_ID = ID; }

    D3D12_RESOURCE_BARRIER m_ResourceBarrierBuffer[16];
    UINT m_NumBarriersToFlush;

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
    void ClearColor(ColorBuffer& Target, float Color[4], D3D12_RECT* Rect = nullptr);

    void SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[]);
    void SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[],
        D3D12_CPU_DESCRIPTOR_HANDLE DSV);
    void SetRenderTarget(const D3D12_CPU_DESCRIPTOR_HANDLE RTV) { SetRenderTargets(1, &RTV); }
    void SetRenderTarget(const D3D12_CPU_DESCRIPTOR_HANDLE RTV, D3D12_CPU_DESCRIPTOR_HANDLE DSV) {
        SetRenderTargets(1, &RTV, DSV);
    }

    void SetViewport(const D3D12_VIEWPORT& vp);
    void SetViewport(FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT minDepth = 0.0f, FLOAT maxDepth = 1.0f);
    void SetScissor(const D3D12_RECT& rect);
    void SetScissor(UINT left, UINT top, UINT right, UINT bottom);
    void SetViewportAndScissor(const D3D12_VIEWPORT& vp, const D3D12_RECT& rect);
    void SetViewportAndScissor(UINT x, UINT y, UINT w, UINT h);

};