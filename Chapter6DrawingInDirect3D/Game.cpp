#include "Game.h"
#include "CommandContext.h"
#include "GraphicsCore.h"
#include "ColorBuffer.h"
#include "Display.h"
#include "BufferManager.h"
#include "UploadBuffer.h"

#include <DirectXColors.h>
#include <array>

using namespace Graphics;

struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT4 Color;
};

static std::array<Vertex, 8> vertices =
{
    Vertex({ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT4(Colors::White) }),
    Vertex({ XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT4(Colors::Black) }),
    Vertex({ XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT4(Colors::Red) }),
    Vertex({ XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT4(Colors::Green) }),
    Vertex({ XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT4(Colors::Blue) }),
    Vertex({ XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT4(Colors::Yellow) }),
    Vertex({ XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT4(Colors::Cyan) }),
    Vertex({ XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT4(Colors::Magenta) })
};

static std::array<std::uint16_t, 36> indices =
{
    // front face
    0, 1, 2,
    0, 2, 3,

    // back face
    4, 6, 5,
    4, 7, 6,

    // left face
    4, 5, 1,
    4, 1, 0,

    // right face
    3, 2, 6,
    3, 6, 7,

    // top face
    1, 5, 6,
    1, 6, 2,

    // bottom face
    4, 0, 3,
    4, 3, 7
};

Game::Game() : 
	m_MainViewport{0, 0, static_cast<float>(g_DisplayWidth) , static_cast<float>(g_DisplayHeight) },
	m_MainScissor{ 0, 0, (LONG)g_DisplayWidth, (LONG)g_DisplayHeight }
{
}

void Game::Startup(void)
{
	m_RootSignature.Reset(0, 0);
	m_RootSignature.Finalize(L"Box RootSignature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	m_VertexBuffer.Create(L"vertex buff", 8, sizeof(Vertex), vertices.data());
	m_IndexBuffer.Create(L"index buff", 36, sizeof(std::uint16_t), indices.data());

    D3D12_INPUT_ELEMENT_DESC mInputLayout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    DXGI_FORMAT ColorFormat = g_DisplayPlane[g_CurrentBuffer].GetFormat();
    DXGI_FORMAT DepthFormat = g_SceneDepthBuffer.GetFormat();

    m_PSO.SetRootSignature(m_RootSignature);
    m_PSO.SetRasterizerState(RasterizerDefault);
    m_PSO.SetBlendState(BlendDisable);
    m_PSO.SetDepthStencilState(DepthStateDisabled);

    m_PSO.SetInputLayout(_countof(mInputLayout), mInputLayout);
    m_PSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    m_PSO.SetRenderTargetFormat(ColorFormat, DepthFormat);

    // shader
    m_PSO.SetVertexShader();
    m_PSO.SetPixelShader();

    m_PSO.Finalize();


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
	
	// set PSO
	gfxContext.SetRootSignature(m_RootSignature);

	gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_PRESENT);

	gfxContext.Finish();
}
