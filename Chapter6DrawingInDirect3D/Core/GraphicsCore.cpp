#include "GraphicsCore.h"
#include "CommandListManager.h"
#include "Display.h"
#include "CommandContext.h"
#include "GraphicsCommon.h"

#ifndef SAFE_RELEASE
#define SAFE_RELEASE(x) if (x != nullptr) { x->Release(); x = nullptr; }
#endif

namespace Graphics
{
    ID3D12Device* g_Device = nullptr;
    CommandListManager g_CommandManager;
    ContextManager g_ContextManager;

    DescriptorAllocator g_DescriptorAllocator[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] =
    {
        D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
        D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER,
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV,
        D3D12_DESCRIPTOR_HEAP_TYPE_DSV
    };
}

// initialize the DirectX resource required to run.
void Graphics::Initialize(bool RequireDXRSupport)
{
	// obtain device
	Microsoft::WRL::ComPtr<ID3D12Device> pDevice;

    //-------------- enable Debug --------------
	uint32_t useDebugLayers = 0;
#if _DEBUG
	// Default to true for debug builds
	useDebugLayers = 1;
#endif
    
    DWORD dxgiFactoryFlags = 0;

    if (useDebugLayers)
    {
        Microsoft::WRL::ComPtr<ID3D12Debug> debugInterface;
        if (SUCCEEDED(D3D12GetDebugInterface(MY_IID_PPV_ARGS(&debugInterface))))
        {
            debugInterface->EnableDebugLayer();

            uint32_t useGPUBasedValidation = 0;
            if (useGPUBasedValidation)
            {
                Microsoft::WRL::ComPtr<ID3D12Debug1> debugInterface1;
                if (SUCCEEDED((debugInterface->QueryInterface(MY_IID_PPV_ARGS(&debugInterface1)))))
                {
                    debugInterface1->SetEnableGPUBasedValidation(true);
                }
            }
        }
        else
        {
            Utility::Print("WARNING:  Unable to enable D3D12 debug validation layer\n");
        }
#if _DEBUG
        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()))))
        {
            dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

            DXGI_INFO_QUEUE_MESSAGE_ID hide[] =
            {
                80 /* IDXGISwapChain::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides. */,
            };
            DXGI_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
        }
#endif
    }

    // obatin the DXGI factory
    Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory;
    ASSERT_SUCCEEDED(CreateDXGIFactory2(dxgiFactoryFlags, MY_IID_PPV_ARGS(&dxgiFactory)));

    // Create the D3D graphics device
    Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
    // not use warp driver
    uint32_t bUseWarpDriver = false;
    if (!bUseWarpDriver)
    {
        SIZE_T MaxSize = 0;

        for (uint32_t Idx = 0; DXGI_ERROR_NOT_FOUND != dxgiFactory->EnumAdapters1(Idx, &pAdapter); ++Idx)
        {
            DXGI_ADAPTER_DESC1 desc;
            pAdapter->GetDesc1(&desc);

            // Is a software adapter?
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;

            // Is this the desired vendor desired?
            //if (desiredVendor != 0 && desiredVendor != desc.VendorId)
            //    continue;

            // Can create a D3D12 device?
            if (FAILED(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, MY_IID_PPV_ARGS(&pDevice))))
                continue;

            // Does support DXR if required?
            //if (RequireDXRSupport && !IsDirectXRaytracingSupported(pDevice.Get()))
            //    continue;

            // By default, search for the adapter with the most memory because that's usually the dGPU.
            if (desc.DedicatedVideoMemory < MaxSize)
                continue;

            MaxSize = desc.DedicatedVideoMemory;

            if (g_Device != nullptr)
                g_Device->Release();

            g_Device = pDevice.Detach(); /*Get the device*/

            Utility::Printf(L"Selected GPU:  %s (%u MB)\n", desc.Description, desc.DedicatedVideoMemory >> 20);
        }
    }

    if (g_Device == nullptr)
    {
        if (bUseWarpDriver)
            Utility::Print("WARP software adapter requested.  Initializing...\n");
        else
            Utility::Print("Failed to find a hardware adapter.  Falling back to WARP.\n");
        ASSERT_SUCCEEDED(dxgiFactory->EnumWarpAdapter(MY_IID_PPV_ARGS(&pAdapter)));
        ASSERT_SUCCEEDED(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_11_0, MY_IID_PPV_ARGS(&pDevice)));
        g_Device = pDevice.Detach();
    }

    // suppress levels of debug info 
#if _DEBUG
    ID3D12InfoQueue* pInfoQueue = nullptr;
    if (SUCCEEDED(g_Device->QueryInterface(MY_IID_PPV_ARGS(&pInfoQueue))))
    {
        // Suppress whole categories of messages
        //D3D12_MESSAGE_CATEGORY Categories[] = {};

        // Suppress messages based on their severity level
        D3D12_MESSAGE_SEVERITY Severities[] =
        {
            D3D12_MESSAGE_SEVERITY_INFO
        };

        // Suppress individual messages by their ID
        D3D12_MESSAGE_ID DenyIds[] =
        {
            // This occurs when there are uninitialized descriptors in a descriptor table, even when a
            // shader does not access the missing descriptors.  I find this is common when switching
            // shader permutations and not wanting to change much code to reorder resources.
            D3D12_MESSAGE_ID_INVALID_DESCRIPTOR_HANDLE,

            // Triggered when a shader does not export all color components of a render target, such as
            // when only writing RGB to an R10G10B10A2 buffer, ignoring alpha.
            D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_PS_OUTPUT_RT_OUTPUT_MISMATCH,

            // This occurs when a descriptor table is unbound even when a shader does not access the missing
            // descriptors.  This is common with a root signature shared between disparate shaders that
            // don't all need the same types of resources.
            D3D12_MESSAGE_ID_COMMAND_LIST_DESCRIPTOR_TABLE_NOT_SET,

            // RESOURCE_BARRIER_DUPLICATE_SUBRESOURCE_TRANSITIONS
            D3D12_MESSAGE_ID_RESOURCE_BARRIER_DUPLICATE_SUBRESOURCE_TRANSITIONS,

            // Suppress errors from calling ResolveQueryData with timestamps that weren't requested on a given frame.
            D3D12_MESSAGE_ID_RESOLVE_QUERY_INVALID_QUERY_STATE,

            // Ignoring InitialState D3D12_RESOURCE_STATE_COPY_DEST. Buffers are effectively created in state D3D12_RESOURCE_STATE_COMMON.
            D3D12_MESSAGE_ID_CREATERESOURCE_STATE_IGNORED,
        };

        D3D12_INFO_QUEUE_FILTER NewFilter = {};
        //NewFilter.DenyList.NumCategories = _countof(Categories);
        //NewFilter.DenyList.pCategoryList = Categories;
        NewFilter.DenyList.NumSeverities = _countof(Severities);
        NewFilter.DenyList.pSeverityList = Severities;
        NewFilter.DenyList.NumIDs = _countof(DenyIds);
        NewFilter.DenyList.pIDList = DenyIds;

        pInfoQueue->PushStorageFilter(&NewFilter);
        pInfoQueue->Release();
    }
#endif

    // use device to create command
    g_CommandManager.Create(g_Device);

    // Common state was moved to GraphicsCommon.*
    InitializeCommonState();

    // create swapchain buffer
    Display::Initialize();

}

void Graphics::Shutdown(void)
{
    g_CommandManager.IdleGPU();

    Display::Shutdown();

#if defined(_GAMING_DESKTOP) && defined(_DEBUG)
    ID3D12DebugDevice* debugInterface;
    if (SUCCEEDED(g_Device->QueryInterface(&debugInterface)))
    {
        debugInterface->ReportLiveDeviceObjects(D3D12_RLDO_DETAIL | D3D12_RLDO_IGNORE_INTERNAL);
        debugInterface->Release();
    }
#endif

    if (g_Device != nullptr)
    {
        g_Device->Release();
        g_Device = nullptr;
    }
}

