#include "pch.h"
#include "Display.h"
#include"GraphicsCore.h"
#include "CommandListManager.h"
#include "GameCore.h"

namespace GameCore { extern HWND g_hWnd; }

#define SWAP_CHAIN_BUFFER_COUNT 3
DXGI_FORMAT SwapChainFormat = DXGI_FORMAT_R10G10B10A2_UNORM;

using namespace Graphics;

namespace Graphics
{
	uint32_t g_DisplayWidth = 1280;
	uint32_t g_DisplayHeight = 720;

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

	ASSERT_SUCCEEDED(dxgiFactory->CreateSwapChainForHwnd(
		g_CommandManager.GetCommandQueue(),
		GameCore::g_hWnd,
		&swapChainDesc,
		&fsSwapChainDesc,
		nullptr,
		&s_SwapChain1));

	
}

