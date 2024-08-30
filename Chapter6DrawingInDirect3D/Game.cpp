#include "Game.h"
#include "CommandContext.h"
#include "GraphicsCore.h"
#include "ColorBuffer.h"
#include "Display.h"

using namespace Graphics;

void Game::Startup(void)
{

}

void Game::Cleanup(void)
{
}

void Game::Update(float deltaT)
{
}

void Game::RenderScene(void)
{
	GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");
	
	gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	
	gfxContext.SetViewportAndScissor(0, 0, g_DisplayWidth, g_DisplayHeight);

	// clear RTV
	g_DisplayPlane[g_CurrentBuffer].SetClearColor({ 0.2f, 0.4f, 0.6f, 1.0f });
	gfxContext.ClearColor(g_DisplayPlane[g_CurrentBuffer]);

	// set RTV
	gfxContext.SetRenderTarget(g_DisplayPlane[g_CurrentBuffer].GetRTV());

	gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_PRESENT);

	gfxContext.Finish();
}
