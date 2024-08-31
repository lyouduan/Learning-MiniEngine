#include "pch.h"
#include "Display.h"
#include "GraphicsCore.h"
#include "ColorBuffer.h"
#include "BufferManager.h"

#define CONDITIONALLY_ENABLE_HDR_OUTPUT 1

DXGI_FORMAT SwapChainFormat = DXGI_FORMAT_R10G10B10A2_UNORM;

namespace GameCore { extern HWND g_hWnd; }

using namespace Graphics;

namespace Graphics
{
    enum eResolution { k720p, k900p, k1080p, k1440p, k1800p, k2160p };
    
    //const char* ResolutionLabels[] = { "1280x720", "1600x900", "1920x1080", "2560x1440", "3200x1800", "3840x2160" };
    //const uint32_t kNumPredefinedResolutions = 6;
    //EnumVar NativeResolution("Graphics/Display/Native Resolution", k1080p, kNumPredefinedResolutions, ResolutionLabels);

    //uint32_t g_NativeWidth = 0;
    //uint32_t g_NativeHeight = 0;
    uint32_t g_DisplayWidth = 1280;
	uint32_t g_DisplayHeight = 720;

    //void ResolutionToUINT(eResolution res, uint32_t& width, uint32_t& height)
    //{
    //    switch (res)
    //    {
    //    default:
    //    case k720p:
    //        width = 1280;
    //        height = 720;
    //        break;
    //    case k900p:
    //        width = 1600;
    //        height = 900;
    //        break;
    //    case k1080p:
    //        width = 1920;
    //        height = 1080;
    //        break;
    //    case k1440p:
    //        width = 2560;
    //        height = 1440;
    //        break;
    //    case k1800p:
    //        width = 3200;
    //        height = 1800;
    //        break;
    //    case k2160p:
    //        width = 3840;
    //        height = 2160;
    //        break;
    //    }
    //}

    void SetNativeResolution(void)
    {
        //uint32_t NativeWidth, NativeHeight;

        //ResolutionToUINT(eResolution((int)NativeResolution), NativeWidth, NativeHeight);
        //
        //if (g_NativeWidth == NativeWidth && g_NativeHeight == NativeHeight)
        //    return;
        //DEBUGPRINT("Changing native resolution to %ux%u", NativeWidth, NativeHeight);

        //g_NativeWidth = NativeWidth;
        //g_NativeHeight = NativeHeight;

        g_CommandManager.IdleGPU();

        // initialize the buffer(DSV)
        InitializeRenderingBuffers(g_DisplayWidth, g_DisplayHeight);
    }

    void SetDisplayResolution(void)
    {
#ifdef _GAMING_DESKTOP
        static int SelectedDisplayRes = DisplayResolution;
        if (SelectedDisplayRes == DisplayResolution)
            return;

        SelectedDisplayRes = DisplayResolution;
        ResolutionToUINT((eResolution)SelectedDisplayRes, g_DisplayWidth, g_DisplayHeight);
        DEBUGPRINT("Changing display resolution to %ux%u", g_DisplayWidth, g_DisplayHeight);

        g_CommandManager.IdleGPU();

        Display::Resize(g_DisplayWidth, g_DisplayHeight);

        SetWindowPos(GameCore::g_hWnd, 0, 0, 0, g_DisplayWidth, g_DisplayHeight, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
#endif
    }


    // RTV
    ColorBuffer g_DisplayPlane[SWAP_CHAIN_BUFFER_COUNT];
    UINT g_CurrentBuffer = 0;

    IDXGISwapChain1* s_SwapChain1 = nullptr;
}

void Display::Initialize(void)
{
	ASSERT(s_SwapChain1 == nullptr, "Graphics has already been initialized");

    Microsoft::WRL::ComPtr<IDXGIFactory4> dxgiFactory;
    ASSERT_SUCCEEDED(CreateDXGIFactory2(0, MY_IID_PPV_ARGS(&dxgiFactory)));

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = g_DisplayWidth;
    swapChainDesc.Height = g_DisplayHeight;
    swapChainDesc.Format = SwapChainFormat;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.SampleDesc.Quality = 0;
    swapChainDesc.Scaling = DXGI_SCALING_NONE;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    DXGI_SWAP_CHAIN_FULLSCREEN_DESC fsSwapChainDesc = {};
    fsSwapChainDesc.Windowed = TRUE;

    // create swapchain
    ASSERT_SUCCEEDED(dxgiFactory->CreateSwapChainForHwnd(
        g_CommandManager.GetCommandQueue(),
        GameCore::g_hWnd,
        &swapChainDesc,
        &fsSwapChainDesc,
        nullptr,
        &s_SwapChain1));

    // create rtv for swapchain
    // need to create descriptorheap, views, apply resource from gpu
    for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
    {
        ComPtr<ID3D12Resource> DisplayPlane;
        ASSERT_SUCCEEDED(s_SwapChain1->GetBuffer(i, MY_IID_PPV_ARGS(&DisplayPlane)));
        g_DisplayPlane[i].CreateFromSwapChain(L"Primary SwapChain Buffer", DisplayPlane.Detach());
    }

    // set resolution for buffer
    SetNativeResolution();
}

void Display::Shutdown(void) 
{
    s_SwapChain1->SetFullscreenState(FALSE, nullptr);
    s_SwapChain1->Release();

    for (UINT i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
        g_DisplayPlane[i].Destroy();


    // depth buffer
    g_SceneDepthBuffer.Destroy();
}

void Display::Resize(uint32_t width, uint32_t height)
{
    g_CommandManager.IdleGPU();

    g_DisplayWidth = width;
    g_DisplayHeight = height;

    DEBUGPRINT("Changing display resolution to %ux%u", width, height);

    for (UINT i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
        g_DisplayPlane[i].Destroy();

    ASSERT(s_SwapChain1 != nullptr);
    ASSERT_SUCCEEDED(s_SwapChain1->ResizeBuffers(SWAP_CHAIN_BUFFER_COUNT, width, height, SwapChainFormat, 0));

    // re-create rtv from new swapchain
    for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
    {
        ComPtr<ID3D12Resource> DisplayPlane;
        ASSERT_SUCCEEDED(s_SwapChain1->GetBuffer(i, MY_IID_PPV_ARGS(&DisplayPlane)));
        g_DisplayPlane[i].CreateFromSwapChain(L"Primary SwapChain Buffer", DisplayPlane.Detach());
    }

    g_CurrentBuffer = 0;

    g_CommandManager.IdleGPU();



}

void Display::Present(void) 
{
    s_SwapChain1->Present(0, 0);

    g_CurrentBuffer = (g_CurrentBuffer + 1) % SWAP_CHAIN_BUFFER_COUNT;
}