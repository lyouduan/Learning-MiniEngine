#include "BufferManager.h"
#include "CommandContext.h"

#define T2X_COLOR_FORMAT DXGI_FORMAT_R10G10B10A2_UNORM
#define HDR_MOTION_FORMAT DXGI_FORMAT_R16G16B16A16_FLOAT
#define DSV_FORMAT DXGI_FORMAT_D32_FLOAT


namespace Graphics
{
    ColorBuffer g_SceneColorBuffer;
    DepthBuffer g_SceneDepthBuffer;

    DXGI_FORMAT DefaultHdrColorFormat = DXGI_FORMAT_R11G11B10_FLOAT;
}


void Graphics::InitializeRenderingBuffers(uint32_t width, uint32_t height)
{
	GraphicsContext& InitContext = GraphicsContext::Begin();

	g_SceneColorBuffer.Create(L"Main Color Buffer", width, height, 1, DefaultHdrColorFormat);
	g_SceneDepthBuffer.Create(L"Scene Depth Buffer", width, height, DSV_FORMAT);

	InitContext.Finish();
}

void Graphics::ResizeDisplayDependentBuffers(uint32_t NativeWidth, uint32_t NativeHeight)
{

}

void Graphics::DestroyRenderingBuffers()
{
	g_SceneDepthBuffer.Destroy();
}
