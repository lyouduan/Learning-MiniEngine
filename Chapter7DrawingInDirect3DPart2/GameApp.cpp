#include "GameApp.h"
#include "GameCore.h"
#include "CommandContext.h"
#include "GraphicsCore.h"
#include "Display.h"
#include "BufferManager.h"
#include "GraphicsCommon.h"

#include <d3dcompiler.h>

using namespace Graphics;
using namespace DirectX;

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
	 
	m_aspectRatio = static_cast<float>(g_DisplayWidth) / static_cast<float>(g_DisplayHeight);
}
void GameApp::Startup(void)
{
	// initialize root signature
	m_RootSignature.Reset(0, 0);
	m_RootSignature.Finalize(L"root sinature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// data
	Vertex triangleVertices[] =
	{
		{{0.0f, 0.25f * m_aspectRatio, 0.0f}, {1.0f, 0.0f, 0.0f, 1.0f}},
		{{0.25f, -0.25f * m_aspectRatio, 0.0f}, {0.0f, 1.0f, 0.0f, 1.0f}},
		{{-0.25f, -0.25f * m_aspectRatio, 0.0f}, {0.0f, 0.0f, 1.0f, 1.0f}}
	};
	uint32_t vertexIndex[] =
	{
		0,1,2
	};

	const uint32_t vertexBufferSize = sizeof(Vertex);
	const uint32_t indexBufferSize = sizeof(std::uint32_t);

	m_VertexBuffer.Create(L"Vertex Buffer", 3, vertexBufferSize, triangleVertices);
	m_IndexBuffer.Create(L"Index Buffer", 3, indexBufferSize, vertexIndex);

	// input layout
	D3D12_INPUT_ELEMENT_DESC mInputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	DXGI_FORMAT ColorFormat = g_DisplayPlane[g_CurrentBuffer].GetFormat();
	DXGI_FORMAT DepthFormat = g_SceneDepthBuffer.GetFormat();

	// shader 
	ComPtr<ID3DBlob> vertexBlob;
	ComPtr<ID3DBlob> pixelBlob;
	D3DReadFileToBlob(L"shader/VertexShader.cso", &vertexBlob);
	D3DReadFileToBlob(L"shader/PixelShader.cso", &pixelBlob);

	// PSO
	m_PSO.SetRootSignature(m_RootSignature);
	m_PSO.SetRasterizerState(RasterizerDefault);
	m_PSO.SetBlendState(BlendDisable);
	m_PSO.SetDepthStencilState(DepthStateDisabled);
	m_PSO.SetInputLayout(_countof(mInputLayout), mInputLayout);
	m_PSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	m_PSO.SetRenderTargetFormat(ColorFormat, DepthFormat);
	m_PSO.SetVertexShader(vertexBlob);
	m_PSO.SetPixelShader(pixelBlob);
	m_PSO.Finalize();
}

void GameApp::Cleanup(void)
{
	m_VertexBuffer.Destroy();
	m_IndexBuffer.Destroy();
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

	// set render target
	gfxContext.SetRenderTarget(g_DisplayPlane[g_CurrentBuffer].GetRTV(), g_SceneDepthBuffer.GetDSV());

	//set root signature
	gfxContext.SetPipelineState(m_PSO);

	gfxContext.SetRootSignature(m_RootSignature);
	gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	gfxContext.SetVertexBuffer(0, m_VertexBuffer.VertexBufferView());
	gfxContext.SetIndexBuffer(m_IndexBuffer.IndexBufferView());
	// draw call
	gfxContext.DrawInstanced(3, 1, 0, 0);

	gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_PRESENT);

	gfxContext.Finish();
}