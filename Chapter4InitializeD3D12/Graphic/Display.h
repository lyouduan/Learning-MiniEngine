#pragma once
#include <cstdint>

namespace Display
{
	// create swapchain
	void Initialize(void); 

	void Shutdown(void);

	void Resize(uint32_t width, uint32_t height);

	void Present(void);
};

