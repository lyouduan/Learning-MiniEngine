#pragma once
#include "ColorBuffer.h"
#include "DepthBuffer.h"
#include "CubeMapBuffer.h"

namespace Graphics
{
	extern CubeMapBuffer g_SceneCubeMapBuffer;
	extern ColorBuffer g_Depth2Buffer;
	extern DepthBuffer g_SceneDepthBuffer;
	extern DepthBuffer g_CubeMapDepthBuffer;

	void InitializeRenderingBuffers(uint32_t NativeWidth, uint32_t NativeHeight);
	void ResizeDisplayDependentBuffers(uint32_t NativeWidth, uint32_t NativeHeight);
	void DestroyRenderingBuffers();
}

