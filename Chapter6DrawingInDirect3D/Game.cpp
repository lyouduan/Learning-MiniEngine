#include "Game.h"
#include "CommandContext.h"
#include "GraphicsCore.h"
#include "ColorBuffer.h"
#include "Display.h"
#include "BufferManager.h"

using namespace Graphics;

Game::Game() : 
	m_MainViewport{0, 0, static_cast<float>(g_DisplayWidth) , static_cast<float>(g_DisplayHeight) },
	m_MainScissor{ 0, 0, (LONG)g_DisplayWidth, (LONG)g_DisplayHeight }
{
}

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
	
	gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

	// set DSV
	gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);
	gfxContext.ClearDepth(g_SceneDepthBuffer);
	gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_READ, true);

	// clear RTV
	g_DisplayPlane[g_CurrentBuffer].SetClearColor({ 0.2f, 0.4f, 0.6f, 1.0f });
	gfxContext.ClearColor(g_DisplayPlane[g_CurrentBuffer]);
	
	// set Render Target
	gfxContext.SetRenderTarget(g_DisplayPlane[g_CurrentBuffer].GetRTV(), g_SceneDepthBuffer.GetDSV_DepthReadOnly());
	
	gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_PRESENT);

	gfxContext.Finish();
}
