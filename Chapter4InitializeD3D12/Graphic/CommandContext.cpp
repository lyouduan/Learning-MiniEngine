#include "pch.h"
#include "CommandContext.h"
#include "GraphicsCore.h"
#include "CommandListManager.h"
#include "ColorBuffer.h"
#include "GpuResource.h"

using namespace Graphics;

CommandContext* ContextManager::AllocateContext(D3D12_COMMAND_LIST_TYPE Type)
{
    std::lock_guard<std::mutex> LockGuard(sm_ContextAllocationMutex);

    auto& AvailabelContexts = sm_AvailableContexts[Type];

    CommandContext* ret = nullptr;
    if (AvailabelContexts.empty())
    {
        ret = new CommandContext(Type);
        sm_ContextPool[Type].emplace_back(ret);
        ret->Initialize();
    }
    else
    {
        ret = AvailabelContexts.front();
        AvailabelContexts.pop();
        ret->Reset();
    }
    ASSERT(ret != nullptr);

    ASSERT(ret->m_Type == Type);

    return ret;
}

void ContextManager::FreeContext(CommandContext* UsedContext)
{
    ASSERT(UsedContext != nullptr);
    std::lock_guard<std::mutex> LockGuard(sm_ContextAllocationMutex);
    sm_AvailableContexts[UsedContext->m_Type].push(UsedContext);
}

void ContextManager::DestroyAllContexts(void)
{
    for (uint32_t i = 0; i < 4; ++i)
        sm_ContextPool[i].clear();
}

CommandContext::CommandContext(D3D12_COMMAND_LIST_TYPE Type) :
    m_Type(Type)
{
    m_CommandList = nullptr;
    m_CurrentAllocator = nullptr;

    m_NumBarriersToFlush = 0;
}

void CommandContext::Reset(void)
{
    // we only call Reset() on previously freed contexts.
    // the command list persists, but we must request a new allocator.
    ASSERT(m_CommandList != nullptr && m_CurrentAllocator == nullptr);
    m_CurrentAllocator = g_CommandManager.GetQueue(m_Type).RequestAllocator();
    m_CommandList->Reset(m_CurrentAllocator, nullptr);

}

CommandContext::~CommandContext(void)
{
    if (m_CommandList != nullptr)
        m_CommandList->Release();
}

void CommandContext::DestroyAllContexts(void)
{
    g_ContextManager.DestroyAllContexts();
}

CommandContext& CommandContext::Begin(const std::wstring ID)
{
    // TODO: insert return statement here
    CommandContext* NewContext = g_ContextManager.AllocateContext(D3D12_COMMAND_LIST_TYPE_DIRECT);
    NewContext->SetID(ID);
    //if (ID.length() > 0)
    //    EngineProfiling::BeginBlock(ID, NewContext);

    return *NewContext;
}

uint64_t CommandContext::Flush(bool WaitForCompletion)
{
    ASSERT(m_CurrentAllocator != nullptr);

    uint64_t FenceValue = Graphics::g_CommandManager.GetQueue(m_Type).ExecuteCommandList(m_CommandList);

    if (WaitForCompletion)
        g_CommandManager.WaitForFence(FenceValue);

    // reset the command list and restore previous state
    m_CommandList->Reset(m_CurrentAllocator, nullptr);

    return FenceValue;

    return 0;
}

uint64_t CommandContext::Finish(bool WaitForCompletion)
{
    ASSERT(m_Type == D3D12_COMMAND_LIST_TYPE_DIRECT || m_Type == D3D12_COMMAND_LIST_TYPE_COMPUTE);

    FlushResourceBarriers();

    ASSERT(m_CurrentAllocator != nullptr);

    CommandQueue& Queue = g_CommandManager.GetQueue(m_Type);

    uint64_t FenceValue = Queue.ExecuteCommandList(m_CommandList);
    Queue.DiscardAllocator(FenceValue, m_CurrentAllocator);
    m_CurrentAllocator = nullptr;

    if (WaitForCompletion)
        g_CommandManager.WaitForFence(FenceValue);

    g_ContextManager.FreeContext(this);

    return FenceValue;
}

void CommandContext::Initialize(void)
{
    g_CommandManager.CreateNewCommandList(m_Type, &m_CommandList, &m_CurrentAllocator);
}

void CommandContext::TransitionResource(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate)
{
    D3D12_RESOURCE_STATES OldState = Resource.m_UsageState;

    if (m_Type == D3D12_COMMAND_LIST_TYPE_COMPUTE)
    {
        ASSERT((OldState & VALID_COMPUTE_QUEUE_RESOURCE_STATES) == OldState);
        ASSERT((NewState & VALID_COMPUTE_QUEUE_RESOURCE_STATES) == NewState);
    }

    if (OldState != NewState)
    {
        ASSERT(m_NumBarriersToFlush < 16, "Exceeded arbitrary limit on buffered barriers");

        D3D12_RESOURCE_BARRIER& BarrierDesc = m_ResourceBarrierBuffer[m_NumBarriersToFlush++];

        BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        BarrierDesc.Transition.pResource = Resource.GetResource();
        BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        BarrierDesc.Transition.StateBefore = OldState;
        BarrierDesc.Transition.StateAfter = NewState;

        // check to see if we already started the transition
        if (NewState == Resource.m_TransitioningState)
        {
            BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
            Resource.m_TransitioningState = (D3D12_RESOURCE_STATES)-1;
        }
        else
            BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

        Resource.m_UsageState = NewState;
    }
    //else if(NewState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
        //InsertUAVBarrier(Resource, FlushImmediate);

    if (FlushImmediate || m_NumBarriersToFlush == 16)
        FlushResourceBarriers();
}

inline void CommandContext::FlushResourceBarriers(void)
{
    if (m_NumBarriersToFlush > 0)
    {
        m_CommandList->ResourceBarrier(m_NumBarriersToFlush, m_ResourceBarrierBuffer);
        m_NumBarriersToFlush = 0;
    }
}

void GraphicsContext::ClearColor(ColorBuffer& Target, D3D12_RECT* Rect)
{
    m_CommandList->ClearRenderTargetView(Target.GetRTV(), Target.GetClearColor().GetPtr(), (Rect == nullptr) ? 0 : 1, Rect);
}

void GraphicsContext::ClearColor(ColorBuffer& Target, float Color[4], D3D12_RECT* Rect)
{
    m_CommandList->ClearRenderTargetView(Target.GetRTV(), Color, (Rect == nullptr) ? 0 : 1, Rect);
}

void GraphicsContext::SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[])
{
    m_CommandList->OMSetRenderTargets(NumRTVs, RTVs, FALSE, nullptr);
}

void GraphicsContext::SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[], D3D12_CPU_DESCRIPTOR_HANDLE DSV)
{
    m_CommandList->OMSetRenderTargets(NumRTVs, RTVs, FALSE, &DSV);
}

void GraphicsContext::SetViewport(const D3D12_VIEWPORT& vp)
{
    m_CommandList->RSSetViewports(1, &vp);
}

void GraphicsContext::SetViewport(FLOAT x, FLOAT y, FLOAT w, FLOAT h, FLOAT minDepth, FLOAT maxDepth)
{
    D3D12_VIEWPORT vp;
    vp.Width = w;
    vp.Height = h;
    vp.MinDepth = minDepth;
    vp.MaxDepth = maxDepth;
    vp.TopLeftX = x;
    vp.TopLeftY = y;
    m_CommandList->RSSetViewports(1, &vp);
}

void GraphicsContext::SetScissor(const D3D12_RECT& rect)
{
    ASSERT(rect.left < rect.right && rect.top < rect.bottom);
    m_CommandList->RSSetScissorRects(1, &rect);
}

void GraphicsContext::SetScissor(UINT left, UINT top, UINT right, UINT bottom)
{
    SetScissor(CD3DX12_RECT(left, top, right, bottom));
}

void GraphicsContext::SetViewportAndScissor(const D3D12_VIEWPORT& vp, const D3D12_RECT& rect)
{
    ASSERT(rect.left < rect.right && rect.top < rect.bottom);
    m_CommandList->RSSetViewports(1, &vp);
    m_CommandList->RSSetScissorRects(1, &rect);
}

void GraphicsContext::SetViewportAndScissor(UINT x, UINT y, UINT w, UINT h)
{
    SetViewport((float)x, (float)y, (float)w, (float)h);
    SetScissor(x, y, x + w, y + h);
}
