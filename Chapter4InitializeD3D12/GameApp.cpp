#include "GameApp.h"
#include "GameCore.h"
#include "CommandContext.h"
#include "GraphicsCore.h"
#include "Display.h"
#include "ColorBuffer.h"

using namespace Graphics;

void GameApp::Startup(void)
{

}

void GameApp::Cleanup(void)
{

}

void GameApp::Update(float deltaT)
{

}

void GameApp::RenderScene(void)
{
    GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");
    
    // transition state of Rtv
    gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET, true);

    gfxContext.SetViewportAndScissor(0,0,g_DisplayWidth, g_DisplayWidth);

    g_DisplayPlane[g_CurrentBuffer].SetClearColor({ 0.2f, 0.4f, 0.6f, 1.0f });
    gfxContext.ClearColor(g_DisplayPlane[g_CurrentBuffer]);

    gfxContext.SetRenderTarget(g_DisplayPlane[g_CurrentBuffer].GetRTV());

    // transition state of Rtv
    gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_PRESENT);

    // commit commandlist to commandQueue
    gfxContext.Finish();

}