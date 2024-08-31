#include "CommandContext.h"
#include "GpuResource.h"
#include "GraphicsCore.h"
#include "CommandListManager.h"
#include "PipelineSate.h"
#include "RootSignature.h"
using namespace Graphics;

void ContextManager::DestroyAllContexts(void)
{
    for (uint32_t i = 0; i < 4; ++i)
        sm_ContextPool[i].clear();
}

CommandContext* ContextManager::AllocateContext(D3D12_COMMAND_LIST_TYPE Type)
{
    std::lock_guard<std::mutex> LockGuard(sm_ContextAllocationMutex);

    auto& AvailableContexts = sm_AvailableContexts[Type];

    CommandContext* ret = nullptr;
    if (AvailableContexts.empty())
    {
        ret = new CommandContext(Type);
        sm_ContextPool[Type].emplace_back(ret);
        ret->Initialize();
    }
    else
    {
        ret = AvailableContexts.front();
        AvailableContexts.pop();
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

CommandContext::CommandContext(D3D12_COMMAND_LIST_TYPE Type) :
    m_Type(Type)/*,
    m_DynamicViewDescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
    m_DynamicSamplerDescriptorHeap(*this, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),
    m_CpuLinearAllocator(kCpuWritable),
    m_GpuLinearAllocator(kGpuExclusive)*/
{
    m_OwningManager = nullptr;
    m_CommandList = nullptr;
    m_CurrentAllocator = nullptr;
    ZeroMemory(m_CurrentDescriptorHeaps, sizeof(m_CurrentDescriptorHeaps));

    m_CurGraphicsRootSignature = nullptr;
    m_CurComputeRootSignature = nullptr;
    m_CurPipelineState = nullptr;
    m_NumBarriersToFlush = 0;
}

inline void CommandContext::CopyBuffer(GpuResource& Dest, GpuResource& Src)
{
    TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST);
    TransitionResource(Src, D3D12_RESOURCE_STATE_COPY_SOURCE);
    FlushResourceBarriers();
    m_CommandList->CopyResource(Dest.GetResource(), Src.GetResource());
}
inline void CommandContext::CopyBufferRegion(GpuResource& Dest, size_t DestOffset, GpuResource& Src, size_t SrcOffset, size_t NumBytes)
{
    TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST);
    //TransitionResource(Src, D3D12_RESOURCE_STATE_COPY_SOURCE);
    FlushResourceBarriers();
    m_CommandList->CopyBufferRegion(Dest.GetResource(), DestOffset, Src.GetResource(), SrcOffset, NumBytes);
}

void CommandContext::CopySubresource(GpuResource& Dest, UINT DestSubIndex, GpuResource& Src, UINT SrcSubIndex)
{
    FlushResourceBarriers();

    D3D12_TEXTURE_COPY_LOCATION DestLocation =
    {
        Dest.GetResource(),
        D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        DestSubIndex
    };

    D3D12_TEXTURE_COPY_LOCATION SrcLocation =
    {
        Src.GetResource(),
        D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
        SrcSubIndex
    };

    m_CommandList->CopyTextureRegion(&DestLocation, 0, 0, 0, &SrcLocation, nullptr);
}

void CommandContext::InitializeBuffer(GpuBuffer& Dest, const void* Data, size_t NumBytes, size_t DestOffset)
{
    CommandContext& InitContext = CommandContext::Begin();

    DynAlloc mem = InitContext.ReserveUploadMemory(NumBytes);
    SIMDMemCopy(mem.DataPtr, Data, Math::DivideByMultiple(NumBytes, 16));

    // copy data to the intermediate upload heap and then schedule a copy from the upload heap to default texture
    InitContext.TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST, true);
    InitContext.m_CommandList->CopyBufferRegion(Dest.GetResource(), DestOffset, mem.Buffer.GetResource(), 0, NumBytes);
    InitContext.TransitionResource(Dest, D3D12_RESOURCE_STATE_GENERIC_READ, true);

    // Execute the command list and wait for it to finish so we can release the upload buffer
    InitContext.Finish(true);
}

void CommandContext::InitializeBuffer(GpuBuffer& Dest, const UploadBuffer& Src, size_t SrcOffset, size_t NumBytes, size_t DestOffset)
{
    CommandContext& InitContext = CommandContext::Begin();

    size_t MaxBytes = std::min<size_t>(Dest.GetBufferSize() - DestOffset, Src.GetBufferSize() - SrcOffset);
    NumBytes = std::min<size_t>(MaxBytes, NumBytes);

    // copy data to the intermediate upload heap and then schedule a copy from the upload heap to the default texture
    InitContext.TransitionResource(Dest, D3D12_RESOURCE_STATE_COPY_DEST, true);
    InitContext.m_CommandList->CopyBufferRegion(Dest.GetResource(), DestOffset, (ID3D12Resource*)Src.GetResource(), SrcOffset, NumBytes);
    InitContext.TransitionResource(Dest, D3D12_RESOURCE_STATE_GENERIC_READ, true);

    // Execute the command list and wait for it to finish so we can release the upload buffer
    InitContext.Finish(true);
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

        // Check to see if we already started the transition
        if (NewState == Resource.m_TransitioningState)
        {
            BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
            Resource.m_TransitioningState = (D3D12_RESOURCE_STATES)-1;
        }
        else
            BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;

        Resource.m_UsageState = NewState;
    }
    //else if (NewState == D3D12_RESOURCE_STATE_UNORDERED_ACCESS)
    //    InsertUAVBarrier(Resource, FlushImmediate);

    if (FlushImmediate || m_NumBarriersToFlush == 16)
        FlushResourceBarriers();
}

void CommandContext::BeginResourceTransition(GpuResource& Resource, D3D12_RESOURCE_STATES NewState, bool FlushImmediate)
{
    // If it's already transitioning, finish that transition
    if (Resource.m_TransitioningState != (D3D12_RESOURCE_STATES)-1)
        TransitionResource(Resource, Resource.m_TransitioningState);

    D3D12_RESOURCE_STATES OldState = Resource.m_UsageState;

    if (OldState != NewState)
    {
        ASSERT(m_NumBarriersToFlush < 16, "Exceeded arbitrary limit on buffered barriers");
        D3D12_RESOURCE_BARRIER& BarrierDesc = m_ResourceBarrierBuffer[m_NumBarriersToFlush++];

        BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        BarrierDesc.Transition.pResource = Resource.GetResource();
        BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        BarrierDesc.Transition.StateBefore = OldState;
        BarrierDesc.Transition.StateAfter = NewState;

        BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_BEGIN_ONLY;

        Resource.m_TransitioningState = NewState;
    }

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

inline void CommandContext::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* HeapPtr)
{
    if (m_CurrentDescriptorHeaps[Type] != HeapPtr)
    {
        m_CurrentDescriptorHeaps[Type] = HeapPtr;
        BindDescriptorHeaps();
    }
}

inline void CommandContext::SetDescriptorHeaps(UINT HeapCount, D3D12_DESCRIPTOR_HEAP_TYPE Type[], ID3D12DescriptorHeap* HeapPtrs[])
{
    bool AnyChanged = false;

    for (UINT i = 0; i < HeapCount; ++i)
    {
        if (m_CurrentDescriptorHeaps[Type[i]] != HeapPtrs[i])
        {
            m_CurrentDescriptorHeaps[Type[i]] = HeapPtrs[i];
            AnyChanged = true;
        }
    }

    if (AnyChanged)
        BindDescriptorHeaps();
}

void CommandContext::SetPipelineState(const PSO& PSO)
{
    ID3D12PipelineState* PipelineState = PSO.GetPipelineStateObject();
    if (PipelineState == m_CurPipelineState)
        return;

    m_CommandList->SetPipelineState(PipelineState);
    m_CurPipelineState = PipelineState;
}

inline void CommandContext::SetPredication(ID3D12Resource* Buffer, UINT64 BufferOffset, D3D12_PREDICATION_OP Op)
{
    m_CommandList->SetPredication(Buffer, BufferOffset, Op);
}

void CommandContext::BindDescriptorHeaps(void)
{
    UINT NonNullHeaps = 0;
    ID3D12DescriptorHeap* HeapsToBind[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
    for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
    {
        ID3D12DescriptorHeap* HeapIter = m_CurrentDescriptorHeaps[i];
        if (HeapIter != nullptr)
            HeapsToBind[NonNullHeaps++] = HeapIter;
    }

    if (NonNullHeaps > 0)
        m_CommandList->SetDescriptorHeaps(NonNullHeaps, HeapsToBind);
}

void CommandContext::Reset(void)
{
    // We only call Reset() on previously freed contexts.  The command list persists, but we must
    // request a new allocator.
    ASSERT(m_CommandList != nullptr && m_CurrentAllocator == nullptr);
    m_CurrentAllocator = g_CommandManager.GetQueue(m_Type).RequestAllocator();
    m_CommandList->Reset(m_CurrentAllocator, nullptr);

    m_CurGraphicsRootSignature = nullptr;
    m_CurComputeRootSignature = nullptr;
    m_CurPipelineState = nullptr;
    m_NumBarriersToFlush = 0;

    BindDescriptorHeaps();
}

CommandContext::~CommandContext(void)
{
    if (m_CommandList != nullptr)
        m_CommandList->Release();
}
void CommandContext::DestroyAllContexts(void)
{
    //LinearAllocator::DestroyAll();
    //DynamicDescriptorHeap::DestroyAll();
    g_ContextManager.DestroyAllContexts();

}
CommandContext& CommandContext::Begin(const std::wstring ID)
{
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

    // make sure previous barriers flush
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


//
// implement GraphicsContext
//
void GraphicsContext::ClearColor(ColorBuffer& Target, D3D12_RECT* Rect)
{
    FlushResourceBarriers();
    m_CommandList->ClearRenderTargetView(Target.GetRTV(), Target.GetClearColor().GetPtr(), (Rect == nullptr) ? 0 : 1, Rect);
}

void GraphicsContext::ClearColor(ColorBuffer& Target, float Colour[4], D3D12_RECT* Rect)
{
    FlushResourceBarriers();
    m_CommandList->ClearRenderTargetView(Target.GetRTV(), Colour, (Rect == nullptr) ? 0 : 1, Rect);
}

void GraphicsContext::ClearDepth(DepthBuffer& Target)
{
    FlushResourceBarriers();
    m_CommandList->ClearDepthStencilView(Target.GetDSV(), D3D12_CLEAR_FLAG_DEPTH, Target.GetClearDepth(), Target.GetClearStencil(), 0, nullptr);
}

void GraphicsContext::ClearStencil(DepthBuffer& Target)
{
    FlushResourceBarriers();
    m_CommandList->ClearDepthStencilView(Target.GetDSV(), D3D12_CLEAR_FLAG_STENCIL, Target.GetClearDepth(), Target.GetClearStencil(), 0, nullptr);
}

void GraphicsContext::ClearDepthAndStencil(DepthBuffer& Target)
{
    FlushResourceBarriers();
    m_CommandList->ClearDepthStencilView(Target.GetDSV(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, Target.GetClearDepth(), Target.GetClearStencil(), 0, nullptr);
}

void GraphicsContext::BeginQuery(ID3D12QueryHeap* QueryHeap, D3D12_QUERY_TYPE Type, UINT HeapIndex)
{
    m_CommandList->BeginQuery(QueryHeap, Type, HeapIndex);
}

void GraphicsContext::EndQuery(ID3D12QueryHeap* QueryHeap, D3D12_QUERY_TYPE Type, UINT HeapIndex)
{
    m_CommandList->EndQuery(QueryHeap, Type, HeapIndex);
}

void GraphicsContext::ResolveQueryData(ID3D12QueryHeap* QueryHeap, D3D12_QUERY_TYPE Type, UINT StartIndex, UINT NumQueries, ID3D12Resource* DestinationBuffer, UINT64 DestinationBufferOffset)
{
    m_CommandList->ResolveQueryData(QueryHeap, Type, StartIndex, NumQueries, DestinationBuffer, DestinationBufferOffset);
}

void GraphicsContext::SetRootSignature(const RootSignature& RootSig)
{
    if (RootSig.GetSignature() == m_CurGraphicsRootSignature)
        return;
    m_CommandList->SetGraphicsRootSignature(m_CurGraphicsRootSignature = RootSig.GetSignature());

    //m_DynamicViewDescriptorHeap.ParseGraphicsRootSignature(RootSig);
    //m_DynamicSamplerDescriptorHeap.ParseGraphicsRootSignature(RootSig);
}

void GraphicsContext::SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[], D3D12_CPU_DESCRIPTOR_HANDLE DSV)
{
    m_CommandList->OMSetRenderTargets(NumRTVs, RTVs, FALSE, &DSV);
}

void GraphicsContext::SetRenderTargets(UINT NumRTVs, const D3D12_CPU_DESCRIPTOR_HANDLE RTVs[])
{
    m_CommandList->OMSetRenderTargets(NumRTVs, RTVs, FALSE, nullptr);
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
