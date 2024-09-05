#include "GameApp.h"
#include "GameCore.h"
#include "CommandContext.h"
#include "GraphicsCore.h"
#include "Display.h"
#include "BufferManager.h"
#include "GraphicsCommon.h"
#include "GameInput.h"
#include <DirectXColors.h>
#include "GeometryGenerator.h"

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
	// prepare data
	BuildLandGeometry();


	// initialize root signature
	m_RootSignature.Reset(1, 0);
	m_RootSignature[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
	m_RootSignature.Finalize(L"root sinature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	
	// shader input layout
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
	if (GameInput::IsPressed(GameInput::kMouse0) || GameInput::IsPressed(GameInput::kMouse1)) {
		// Make each pixel correspond to a quarter of a degree.
		float dx = m_xLast - GameInput::GetAnalogInput(GameInput::kAnalogMouseX);
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

	// Update the view matrix.
	const XMVECTOR eyePosition = XMVectorSet(x, y, z, 1); // point
	const XMVECTOR focusPoint = XMVectorSet(0, 0, 0, 1); // point
	const XMVECTOR upDirection = XMVectorSet(0, 1, 0, 0);
	m_View = XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

	m_Projection = XMMatrixPerspectiveFovLH(0.25f * XM_PI, m_aspectRatio, 0.1f, 100.0f);
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
	for (auto& iter : m_AllRenders)
	{
		auto m_MVP = iter->world * m_View * m_Projection;
		gfxContext.SetDynamicConstantBufferView(0, sizeof(m_MVP), &m_MVP);
		gfxContext.DrawIndexedInstanced(iter->IndexCount, 1, 0, 0, 0);
	}

	gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_PRESENT);

	gfxContext.Finish();
}

void GameApp::BuildLandGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

	//
	// Extract the vertex elements we are interested and apply the height function to
	// each vertex.  In addition, color the vertices based on their height so we have
	// sandy looking beaches, grassy low hills, and snow mountain peaks.
	//

	std::vector<Vertex> vertices(grid.Vertices.size());
	for (size_t i = 0; i < grid.Vertices.size(); ++i)
	{
		auto& p = grid.Vertices[i].Position;
		vertices[i].position = p;
		vertices[i].position.y = GetHillsHeight(p.x, p.z);

		// Color the vertex based on its height.
		if (vertices[i].position.y < -10.0f)
		{
			// Sandy beach color.
			vertices[i].color = XMFLOAT4(1.0f, 0.96f, 0.62f, 1.0f);
		}
		else if (vertices[i].position.y < 5.0f)
		{
			// Light yellow-green.
			vertices[i].color = XMFLOAT4(0.48f, 0.77f, 0.46f, 1.0f);
		}
		else if (vertices[i].position.y < 12.0f)
		{
			// Dark yellow-green.
			vertices[i].color = XMFLOAT4(0.1f, 0.48f, 0.19f, 1.0f);
		}
		else if (vertices[i].position.y < 20.0f)
		{
			// Dark brown.
			vertices[i].color = XMFLOAT4(0.45f, 0.39f, 0.34f, 1.0f);
		}
		else
		{
			// White snow.
			vertices[i].color = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
		}
	}

	std::vector<std::uint16_t> indices = grid.GetIndices16();

	const uint32_t vertexBufferSize = sizeof(Vertex);
	const uint32_t indexBufferSize = sizeof(uint16_t);

	m_VertexBuffer.Create(L"vertex buff", (UINT)vertices.size(), vertexBufferSize, vertices.data());
	m_IndexBuffer.Create(L"Index Buffer", (UINT)indices.size(), indexBufferSize, indices.data());

	auto land = std::make_unique<RenderItem>(); 
	land->world = XMMatrixIdentity() * XMMatrixScaling(0.5, 0.5, 0.5) * XMMatrixTranslation(0.0f, -10.0f, -20.f);
	land->IndexCount = (UINT)indices.size();
	land->StartIndexLocation = 0;
	land->BaseVertexLocation = 0;

	m_AllRenders.push_back(std::move(land));
}

float GameApp::GetHillsHeight(float x, float z) const
{
	return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}
