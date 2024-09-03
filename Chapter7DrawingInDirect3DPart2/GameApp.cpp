#include "GameApp.h"
#include "GameCore.h"
#include "CommandContext.h"
#include "GraphicsCore.h"
#include "Display.h"
#include "BufferManager.h"

using namespace Graphics;

GameApp::GameApp(void)
{
	m_Scissor.left = 0;
	m_Scissor.right = g_DisplayWidth;
	m_Scissor.top = 0;
	m_Scissor.bottom = g_DisplayHeight;

	m_Viewport.TopLeftX = 0;
	m_Viewport.TopLeftY = 0;
	m_Viewport.Width = static_cast<float>(g_DisplayWidth);
	m_Viewport.Height = static_cast<float>(g_DisplayHeight);
	m_Viewport.MinDepth = 0.0;
	m_Viewport.MaxDepth = 1.0;
}
void GameApp::Startup(void)
{
	// initialize root signature
	m_RootSignature.Reset(0, 0);


	// PSO
	m_PSO.SetRootSignature(m_RootSignature);
}

void GameApp::Cleanup(void)
{
	m_RootSignature.DestroyAll();
	m_PSO.DestroyAll();
}

void GameApp::Update(float deltaT)
{

}

void GameApp::RenderScene(void)
{
	GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");
	gfxContext.SetViewportAndScissor(m_Viewport, m_Scissor);

	gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

	// clear dsv
	gfxContext.ClearDepth(g_SceneDepthBuffer);
	
	// clear rtv
	g_DisplayPlane[g_CurrentBuffer].SetClearColor(Color{ 0.2f, 0.4f, 0.6f, 1.0f });
	gfxContext.ClearColor(g_DisplayPlane[g_CurrentBuffer]);
	
	//set root signature
	gfxContext.SetRootSignature(m_RootSignature);

	// set render target
	gfxContext.SetRenderTarget(g_DisplayPlane[g_CurrentBuffer].GetRTV(), g_SceneDepthBuffer.GetDSV());
	
	gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_PRESENT);

	gfxContext.Finish();
}