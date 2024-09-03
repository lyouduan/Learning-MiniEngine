#pragma once
#include "ColorBuffer.h"
#include "DepthBuffer.h"

namespace Graphics
{
	extern DepthBuffer g_SceneDepthBuffer;

	void InitializeRenderingBuffers(uint32_t NativeWidth, uint32_t NativeHeight);
	void ResizeDisplayDependentBuffers(uint32_t NativeWidth, uint32_t NativeHeight);
	void DestroyRenderingBuffers();
}

