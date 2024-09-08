#include "BufferManager.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "DepthBuffer.h"

namespace Graphics
{
	DepthBuffer g_SceneDepthBuffer;
}

#define T2X_COLOR_FORMAT DXGI_FORMAT_R10G10B10A2_UNORM
#define HDR_MOTION_FORMAT DXGI_FORMAT_R16G16B16A16_FLOAT
#define DSV_FORMAT DXGI_FORMAT_D32_FLOAT

void Graphics::InitializeRenderingBuffers(uint32_t width, uint32_t height)
{
	g_SceneDepthBuffer.Create(L"Scene Depth Buffer", width, height, DSV_FORMAT);
}

void Graphics::ResizeDisplayDependentBuffers(uint32_t NativeWidth, uint32_t NativeHeight)
{
}

void Graphics::DestroyRenderingBuffers()
{
	g_SceneDepthBuffer.Destroy();
}
