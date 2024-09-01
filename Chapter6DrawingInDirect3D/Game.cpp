#include "pch.h"
#include "Game.h"
#include "CommandContext.h"
#include "GraphicsCore.h"
#include "ColorBuffer.h"
#include "Display.h"
#include "BufferManager.h"
#include "UploadBuffer.h"
#include "GameInput.h"

#include <d3dcompiler.h>

#include <DirectXColors.h>
#include <array>

using namespace Graphics;

    
Game::Game() : 
    m_color{ 0.2f, 0.4f, 0.6f, 1.0f }
{
}

void Game::Startup(void)
{

    struct Vertex
    {
        XMFLOAT3 Pos;
        XMFLOAT4 Color;
    };

    std::array<Vertex, 8> vertices =
    {
        Vertex({ XMFLOAT3(-0.5f, -0.5f, -0.5f), XMFLOAT4(Colors::White) }),
        Vertex({ XMFLOAT3(-0.5f, +0.5f, -0.5f), XMFLOAT4(Colors::Black) }),
        Vertex({ XMFLOAT3(+0.5f, +0.5f, -0.5f), XMFLOAT4(Colors::Red) }),
        Vertex({ XMFLOAT3(+0.5f, -0.5f, -0.5f), XMFLOAT4(Colors::Green) }),
        Vertex({ XMFLOAT3(-0.5f, -0.5f, +0.5f), XMFLOAT4(Colors::Blue) }), // 4
        Vertex({ XMFLOAT3(-0.5f, +0.5f, +0.5f), XMFLOAT4(Colors::Yellow) }),
        Vertex({ XMFLOAT3(+0.5f, +0.5f, +0.5f), XMFLOAT4(Colors::Cyan) }),
        Vertex({ XMFLOAT3(+0.5f, -0.5f, +0.5f), XMFLOAT4(Colors::Magenta) })
    };

    std::array<std::uint16_t, 36> indices =
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
        4, 3, 7,
       
    };

	m_RootSignature.Reset(1, 0);
    m_RootSignature[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
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

    UINT compileFlags = 0;
#if _DEBUG
    compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    ComPtr<ID3DBlob> vertexByte = nullptr;
    ComPtr<ID3DBlob> pixelByte = nullptr;
    //ComPtr<ID3DBlob> error = nullptr;
    //
    D3DReadFileToBlob(L"D:/gcRepo/LearningMiniEngine/Chapter6DrawingInDirect3D/shader/VertexShader.cso", &vertexByte);
    D3DReadFileToBlob(L"D:/gcRepo/LearningMiniEngine/Chapter6DrawingInDirect3D/shader/PixelShader.cso", &pixelByte);

    m_PSO.SetRootSignature(m_RootSignature);
    m_PSO.SetRasterizerState(RasterizerDefault);
    m_PSO.SetBlendState(BlendDisable);
    m_PSO.SetDepthStencilState(DepthStateDisabled);

    m_PSO.SetInputLayout(_countof(mInputLayout), mInputLayout);
    m_PSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    m_PSO.SetRenderTargetFormat(ColorFormat, DepthFormat);
    // shader
    m_PSO.SetVertexShader(vertexByte);
    m_PSO.SetPixelShader(pixelByte);
    m_PSO.Finalize();

    m_MainViewport.TopLeftX = 0;
    m_MainViewport.TopLeftY = 0;
    m_MainViewport.Width = (float)g_DisplayPlane[g_CurrentBuffer].GetWidth();
    m_MainViewport.Height = (float)g_DisplayPlane[g_CurrentBuffer].GetHeight();
    m_MainViewport.MinDepth = 0.0f;
    m_MainViewport.MaxDepth = 1.0f;

    m_MainScissor.left = 0;
    m_MainScissor.top = 0;
    m_MainScissor.right = (LONG)g_DisplayPlane[g_CurrentBuffer].GetWidth();
    m_MainScissor.bottom = (LONG)g_DisplayPlane[g_CurrentBuffer].GetHeight();
    
}

void Game::Cleanup(void)
{
    m_VertexBuffer.Destroy();
    m_IndexBuffer.Destroy();
}

void Game::Update(float deltaT)
{
    if (GameInput::IsPressed(GameInput::kMouse0) || GameInput::IsPressed(GameInput::kMouse1)) {
        // Make each pixel correspond to a quarter of a degree.
        float dx = GameInput::GetAnalogInput(GameInput::kAnalogMouseX) - m_xLast;
        float dy = GameInput::GetAnalogInput(GameInput::kAnalogMouseY) - m_yLast;

        if (GameInput::IsPressed(GameInput::kMouse0))
        {
            // Update angles based on input to orbit camera around box.
            m_xRotate += (dx - m_xDiff);
            m_yRotate += (dy - m_yDiff);
            m_yRotate = (std::max)(-XM_PIDIV2 + 0.1f, m_yRotate);
            m_yRotate = (std::min)(XM_PIDIV2 - 0.1f, m_yRotate);
        }
        else
        {
            m_radius += dx - dy - (m_xDiff - m_yDiff);
        }

        m_xDiff = dx;
        m_yDiff = dy;

        m_xLast += GameInput::GetAnalogInput(GameInput::kAnalogMouseX);
        m_yLast += GameInput::GetAnalogInput(GameInput::kAnalogMouseY);
    }
    else
    {
        m_xDiff = 0.0f;
        m_yDiff = 0.0f;
        m_xLast = 0.0f;
        m_yLast = 0.0f;
    }

    float x = m_radius * cosf(m_yRotate) * sinf(m_xRotate);
    float y = m_radius * sinf(m_yRotate);
    float z = m_radius * cosf(m_yRotate) * cosf(m_xRotate);

    float angle = static_cast<float>(0.0);
    const XMVECTOR rotationAxis = XMVectorSet(0, 1, 1, 0);
    XMMATRIX Model = XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(angle));

    // Update the view matrix.
    const XMVECTOR eyePosition = XMVectorSet(x, y, z, 1); // point
    const XMVECTOR focusPoint = XMVectorSet(0, 0, 0, 1); // point
    const XMVECTOR upDirection = XMVectorSet(0, 1, 0, 0);
    XMMATRIX View = XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

    float aspectRatio = g_DisplayWidth / static_cast<float>(g_DisplayHeight);
    XMMATRIX Proj = XMMatrixPerspectiveFovLH(0.25f * XM_PI, aspectRatio, 0.1f, 100.0f);

    m_MVP = XMMatrixMultiply(View, Proj);
}

void Game::RenderScene(void)
{
	GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");
	
	gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
    gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

	gfxContext.SetViewportAndScissor(m_MainViewport, m_MainScissor);

    // set DSV
    gfxContext.ClearDepth(g_SceneDepthBuffer);

    // set RTV
    g_DisplayPlane[g_CurrentBuffer].SetClearColor(m_color);
    gfxContext.ClearColor(g_DisplayPlane[g_CurrentBuffer]);
    gfxContext.SetRenderTarget(g_DisplayPlane[g_CurrentBuffer].GetRTV(), g_SceneDepthBuffer.GetDSV());
	
	// set PSO
    gfxContext.SetPipelineState(m_PSO);
	gfxContext.SetRootSignature(m_RootSignature);
    gfxContext.SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    gfxContext.SetVertexBuffer(0, m_VertexBuffer.VertexBufferView());
    gfxContext.SetIndexBuffer(m_IndexBuffer.IndexBufferView());
    gfxContext.SetDynamicConstantBufferView(0, sizeof(m_MVP), &m_MVP);
    gfxContext.DrawIndexedInstanced(36, 1, 0, 0, 0);

	gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_PRESENT);
	gfxContext.Finish();
}
