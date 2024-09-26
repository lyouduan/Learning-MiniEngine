#include "BufferManager.h"
#include "GraphicsCore.h"
#include "CommandContext.h"
#include "DepthBuffer.h"

namespace Graphics
{
	//DepthBuffer g_SceneDepthBuffer;
	 
	// reverse-z
	// set the clear value as 0.0 on far plane, 1.0 is the near plane in DNC
	DepthBuffer g_SceneDepthBuffer(0.0, 0);
	DepthBuffer g_CubeMapDepthBuffer(0.0, 0);

	CubeMapBuffer g_SceneCubeMapBuffer;
}

#define T2X_COLOR_FORMAT DXGI_FORMAT_R10G10B10A2_UNORM
#define HDR_MOTION_FORMAT DXGI_FORMAT_R16G16B16A16_FLOAT
#define DSV_FORMAT DXGI_FORMAT_D32_FLOAT

void Graphics::InitializeRenderingBuffers(uint32_t width, uint32_t height)
{
	g_SceneCubeMapBuffer.Create(L"Cubemap Buffer", 512, 512, 1, T2X_COLOR_FORMAT);
	g_CubeMapDepthBuffer.Create(L"Cubemap Depth Buffer", 512, 512, DSV_FORMAT);

	g_SceneDepthBuffer.Create(L"Scene Depth Buffer", width, height, DSV_FORMAT);
}

void Graphics::ResizeDisplayDependentBuffers(uint32_t NativeWidth, uint32_t NativeHeight)
{
}

void Graphics::DestroyRenderingBuffers()
{
	g_SceneCubeMapBuffer.Destroy();
	g_CubeMapDepthBuffer.Destroy();

	g_SceneDepthBuffer.Destroy();
}
