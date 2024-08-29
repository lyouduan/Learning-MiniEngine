#include "pch.h"
#include "Display.h"
#include "GraphicsCore.h"
#include "CommandListManager.h"
#include "GameCore.h"
#include "ColorBuffer.h"

namespace GameCore { extern HWND g_hWnd; }
#ifndef SWAP_CHAIN_BUFFER_COUNT
#define SWAP_CHAIN_BUFFER_COUNT 3
#endif // SWAP_CHAIN_BUFFER_COUNT

DXGI_FORMAT SwapChainFormat = DXGI_FORMAT_R10G10B10A2_UNORM;

using namespace Graphics;

namespace Graphics
{
	uint32_t g_DisplayWidth = 1280;
	uint32_t g_DisplayHeight = 720;

	IDXGISwapChain1* s_SwapChain1 = nullptr;

	ColorBuffer g_DisplayPlane[SWAP_CHAIN_BUFFER_COUNT];
	UINT g_CurrentBuffer = 0; // tack to current buffer
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

	ASSERT_SUCCEEDED(dxgiFactory->CreateSwapChainForHwnd(
		g_CommandManager.GetCommandQueue(),
		GameCore::g_hWnd,
		&swapChainDesc,
		&fsSwapChainDesc,
		nullptr,
		&s_SwapChain1));

	// create rtv for swapchain
	for (size_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
	{
		ComPtr<ID3D12Resource> DisplayPlane;
		ASSERT_SUCCEEDED(s_SwapChain1->GetBuffer(i, MY_IID_PPV_ARGS(&DisplayPlane)));
		g_DisplayPlane[i].CreateFromSwapChain(L"Primary SwapChain Buffer", DisplayPlane.Detach());
	}

}

void Display::Shutdown(void)
{
	s_SwapChain1->SetFullscreenState(FALSE, nullptr);
	s_SwapChain1->Release();

	for (UINT i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
		g_DisplayPlane[i].Destroy();

}

void Display::Resize(uint32_t width, uint32_t height)
{
	g_CommandManager.IdelGPU();

	g_DisplayWidth = width;
	g_DisplayHeight = height;

	DEBUGPRINT("Changing display resolution to %ux%u", width, height);

	for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
		g_DisplayPlane[i].Destroy();

	ASSERT(s_SwapChain1 != nullptr);
	ASSERT_SUCCEEDED(s_SwapChain1->ResizeBuffers(SWAP_CHAIN_BUFFER_COUNT, width, height, SwapChainFormat, 0));

	for (uint32_t i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
	{
		ComPtr<ID3D12Resource> DisplayPlane;
		ASSERT_SUCCEEDED(s_SwapChain1->GetBuffer(i, MY_IID_PPV_ARGS(&DisplayPlane)));
		g_DisplayPlane[i].CreateFromSwapChain(L"Primary SwapChain Buffer", DisplayPlane.Detach());
	}

	g_CurrentBuffer = 0;

	g_CommandManager.IdelGPU();
	
}

void Display::Present(void)
{
	s_SwapChain1->Present(0, 0);
	g_CurrentBuffer = (g_CurrentBuffer + 1) % SWAP_CHAIN_BUFFER_COUNT;
	 
}

