#pragma once
#include <cstdint>


// mainly create swapchain to display
namespace Display
{
    // initialize the swapchain and related rtv
    void Initialize(void);
    // shutdown the rtv
    void Shutdown(void);
    // resize swapchain
    void Resize(uint32_t width, uint32_t height);
    // present the swapchain
    void Present(void);

};

namespace Graphics
{
    extern uint32_t g_DisplayWidth;
    extern uint32_t g_DisplayHeight;

}
