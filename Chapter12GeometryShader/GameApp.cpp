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
#include "TextureManager.h"
#include "DescriptorHeap.h"
#include <fstream>
#include <d3dcompiler.h>
#include <array>

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
	m_Viewport.MinDepth = 0.0f;
	m_Viewport.MaxDepth = 1.0f;
	 
	m_aspectRatio = static_cast<float>(g_DisplayWidth) / static_cast<float>(g_DisplayHeight);

	mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

}

void GameApp::Startup(void)
{
	// prepare material
	BuildMaterials();
	// load Textures
	LoadTextures();

	// prepare shape and add material
	BuildLandGeometry();
	BuildWavesGeometry();
	BuildBoxGeometry();
	BuildBillboardGeometry();

	// build render items
	BuildLandRenderItems();

	// initialize root signature
	m_RootSignature.Reset(4, 1);
	m_RootSignature[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_VERTEX);
	m_RootSignature[1].InitAsConstantBuffer(1, D3D12_SHADER_VISIBILITY_ALL);
	m_RootSignature[2].InitAsConstantBuffer(2, D3D12_SHADER_VISIBILITY_ALL);
	m_RootSignature[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	// sampler
	m_RootSignature.InitStaticSampler(0, Graphics::SamplerLinearWrapDesc, D3D12_SHADER_VISIBILITY_PIXEL);

	m_RootSignature.Finalize(L"root sinature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	
	// shader input layout
	D3D12_INPUT_ELEMENT_DESC mInputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_INPUT_ELEMENT_DESC mInputLayoutBillboard[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "SIZE", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	DXGI_FORMAT ColorFormat = g_DisplayPlane[g_CurrentBuffer].GetFormat();
	DXGI_FORMAT DepthFormat = g_SceneDepthBuffer.GetFormat();

	// shader 
	ComPtr<ID3DBlob> vertexBlob;
	ComPtr<ID3DBlob> pixelBlob;
	D3DReadFileToBlob(L"shader/VertexShader.cso", &vertexBlob);
	D3DReadFileToBlob(L"shader/PixelShader.cso", &pixelBlob);

	// shader 
	ComPtr<ID3DBlob> vertexBlobTree;
	ComPtr<ID3DBlob> pixelBlobTree;
	ComPtr<ID3DBlob> GSBlob;
	D3DReadFileToBlob(L"shader/GSVertexShader.cso", &vertexBlobTree);
	D3DReadFileToBlob(L"shader/GSPixelShader.cso", &pixelBlobTree);
	D3DReadFileToBlob(L"shader/GS.cso", &GSBlob);

	// PSO
	GraphicsPSO opaquePSO;
	opaquePSO.SetRootSignature(m_RootSignature);
	opaquePSO.SetRasterizerState(RasterizerDefault);
	opaquePSO.SetBlendState(BlendDisable);
	opaquePSO.SetDepthStencilState(DepthStateReadWrite);
	opaquePSO.SetInputLayout(_countof(mInputLayout), mInputLayout);
	opaquePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	opaquePSO.SetRenderTargetFormat(ColorFormat, DepthFormat);
	opaquePSO.SetVertexShader(vertexBlob);
	opaquePSO.SetPixelShader(pixelBlob);
	opaquePSO.Finalize();
	m_PSOs["opaque"] = opaquePSO;

	GraphicsPSO transpacrentPSO = opaquePSO;
	auto blend = Graphics::BlendTraditional;
	blend.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	transpacrentPSO.SetBlendState(blend);
	transpacrentPSO.Finalize();
	m_PSOs["transparent"] = transpacrentPSO;

	GraphicsPSO alphaTestedPSO = opaquePSO;
	auto rater = RasterizerDefault;
	rater.CullMode = D3D12_CULL_MODE_NONE; // not cull 
	alphaTestedPSO.SetRasterizerState(rater);
	alphaTestedPSO.Finalize();
	m_PSOs["alphaTested"] = alphaTestedPSO;

	GraphicsPSO billboardPSO;
	rater = RasterizerDefault;
	rater.CullMode = D3D12_CULL_MODE_NONE;

	billboardPSO.SetRootSignature(m_RootSignature);
	billboardPSO.SetRasterizerState(rater);
	billboardPSO.SetBlendState(BlendDisable);
	billboardPSO.SetDepthStencilState(DepthStateReadWrite);
	billboardPSO.SetInputLayout(_countof(mInputLayoutBillboard), mInputLayoutBillboard);
	billboardPSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT);
	billboardPSO.SetRenderTargetFormat(ColorFormat, DepthFormat);
	billboardPSO.SetVertexShader(vertexBlobTree);
	billboardPSO.SetPixelShader(pixelBlobTree);
	billboardPSO.SetGeometryShader(GSBlob);
	billboardPSO.Finalize();
	m_PSOs["billboard"] = billboardPSO;
}

void GameApp::Cleanup(void)
{
	for (auto& iter : m_AllRenders)
	{
		iter->Geo->m_VertexBuffer.Destroy();
		iter->Geo->m_IndexBuffer.Destroy();
	}

	m_Textures.clear();
	m_Materials.clear();
	m_Geometry.clear();
	m_PSOs.clear();

	delete m_WavesRitem;
}

void GameApp::Update(float deltaT)
{
	//if (GameInput::IsFirstPressed(GameInput::kKey_f1))
		//m_bRenderShapes = !m_bRenderShapes;

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
	m_Projection = XMMatrixPerspectiveFovLH(0.25f * XM_PI, m_aspectRatio, 0.1f, 1000.0f);

	XMStoreFloat4x4(&passConstant.ViewProj, XMMatrixTranspose(m_View * m_Projection)); // hlsl 列主序矩阵

	// light
	XMVECTOR lightDir = -DirectX::XMVectorSet(
		1.0f * sinf(mSunTheta) * cosf(mSunTheta),
		1.0f * cosf(mSunTheta),
		1.0f * sinf(mSunTheta) * sinf(mSunTheta),
		1.0f);
	XMStoreFloat3(&passConstant.Lights[0].Direction, lightDir);
	passConstant.Lights[0].Strength = { 0.9f, 0.9f, 0.9f };
	passConstant.Lights[0].FalloffEnd = 100.0;
	passConstant.Lights[0].Position = { 0.0f, 10.0f, -10.0f };
	passConstant.ambientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
	XMStoreFloat3(&passConstant.eyePosW, eyePosition);

	passConstant.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
	passConstant.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };

	passConstant.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
	passConstant.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };
	// update waves
	UpdateWaves(deltaT);
}

void GameApp::RenderScene(void)
{
	GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");

	gfxContext.SetViewportAndScissor(m_Viewport, m_Scissor);

	gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

	// clear dsv
	gfxContext.ClearDepthAndStencil(g_SceneDepthBuffer);
	// clear rtv
	g_DisplayPlane[g_CurrentBuffer].SetClearColor(Color(passConstant.fogColor.x, passConstant.fogColor.y, passConstant.fogColor.z, passConstant.fogColor.w));

	gfxContext.ClearColor(g_DisplayPlane[g_CurrentBuffer]);

	// set render target
	gfxContext.SetRenderTarget(g_DisplayPlane[g_CurrentBuffer].GetRTV(), g_SceneDepthBuffer.GetDSV());

	//set root signature

	gfxContext.SetRootSignature(m_RootSignature);

	gfxContext.SetDynamicConstantBufferView(1, sizeof(passConstant), &passConstant);

	// draw call
	{
		gfxContext.SetPipelineState(m_PSOs["opaque"]);
		DrawRenderItems(gfxContext, m_LandRenders[(int)RenderLayer::Opaque]);

		gfxContext.SetPipelineState(m_PSOs["alphaTested"]);
		DrawRenderItems(gfxContext, m_LandRenders[(int)RenderLayer::AlphaTested]);

		gfxContext.SetPipelineState(m_PSOs["billboard"]);
		DrawRenderItems(gfxContext, m_LandRenders[(int)RenderLayer::AlphaTestedBillboard]);

		// transparent render pass at last 
		gfxContext.SetPipelineState(m_PSOs["transparent"]);
		DrawRenderItems(gfxContext, m_LandRenders[(int)RenderLayer::Transparent]);
	}
		
	
	gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_PRESENT);

	gfxContext.Finish();
}

void GameApp::DrawRenderItems(GraphicsContext& gfxContext, std::vector<RenderItem*>& items)
{
	ObjConstants objConstants;
	MaterialConstants matCB;
	for (auto& iter : items)
	{
		gfxContext.SetPrimitiveTopology(iter->PrimitiveType);
		gfxContext.SetVertexBuffer(0, iter->Geo->m_VertexBuffer.VertexBufferView());
		gfxContext.SetIndexBuffer(iter->Geo->m_IndexBuffer.IndexBufferView());

		XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(iter->World)); // hlsl 列主序矩阵
		XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(iter->TexTransform)); // hlsl 列主序矩阵
		gfxContext.SetDynamicConstantBufferView(0, sizeof(objConstants), &objConstants);

		XMStoreFloat4x4(&matCB.MatTransform, XMMatrixTranspose(iter->Mat->MatTransform));
		matCB.DiffuseAlbedo = iter->Mat->DiffuseAlbedo;
		matCB.FresnelR0 = iter->Mat->FresnelR0;
		matCB.Roughness = iter->Mat->Roughness;
		// constants
		gfxContext.SetDynamicConstantBufferView(2, sizeof(MaterialConstants), &matCB);

		// srv
		gfxContext.SetDynamicDescriptor(3, 0, iter->srv);

		gfxContext.DrawIndexedInstanced(iter->IndexCount, 1, iter->StartIndexLocation, iter->BaseVertexLocation, 0);
	}
}

void GameApp::BuildLandRenderItems()
{
	auto land = std::make_unique<RenderItem>();
	land->World = XMMatrixIdentity() ;
	land->TexTransform = XMMatrixScaling(5.0f, 5.0f, 1.0f);
	land->Geo = m_Geometry["landGeo"].get();
	land->Mat = m_Materials["grass"].get();
	land->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	land->IndexCount = land->Geo->DrawArgs["land"].IndexCount;
	land->BaseVertexLocation = land->Geo->DrawArgs["land"].BaseVertexLocation;
	land->StartIndexLocation = land->Geo->DrawArgs["land"].StartIndexLocation;
	land->srv = m_Textures["grass"].GetSRV();

	m_LandRenders[(int)RenderLayer::Opaque].push_back(land.get());

	auto wave = std::make_unique<RenderItem>();
	wave->World = XMMatrixIdentity();
	wave->TexTransform = XMMatrixScaling(4.0f, 4.0f, 1.0f);
	wave->Geo = m_Geometry["waveGeo"].get();
	wave->Mat = m_Materials["water"].get();
	wave->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wave->IndexCount = wave->Geo->DrawArgs["wave"].IndexCount;
	wave->BaseVertexLocation = wave->Geo->DrawArgs["wave"].BaseVertexLocation;
	wave->StartIndexLocation = wave->Geo->DrawArgs["wave"].StartIndexLocation;
	wave->srv = m_Textures["water"].GetSRV();

	m_WavesRitem = wave.get();
	m_LandRenders[(int)RenderLayer::Transparent].push_back(wave.get());

	auto box = std::make_unique<RenderItem>();
	box->World = XMMatrixIdentity() * XMMatrixTranslation(3.0f, 2.0f, -9.0f);
	box->Geo = m_Geometry["boxGeo"].get();
	box->Mat = m_Materials["wirefence"].get();
	box->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	box->IndexCount = box->Geo->DrawArgs["sbox"].IndexCount;
	box->BaseVertexLocation = box->Geo->DrawArgs["sbox"].BaseVertexLocation;
	box->StartIndexLocation = box->Geo->DrawArgs["sbox"].StartIndexLocation;
	box->srv = m_Textures["wireFence"].GetSRV();

	m_LandRenders[(int)RenderLayer::AlphaTested].push_back(box.get());

	auto tree = std::make_unique<RenderItem>();
	tree->World = XMMatrixIdentity();
	tree->Geo = m_Geometry["treeGeo"].get();
	tree->Mat = m_Materials["tree"].get();
	tree->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
	tree->IndexCount = tree->Geo->DrawArgs["tree"].IndexCount;
	tree->BaseVertexLocation = tree->Geo->DrawArgs["tree"].BaseVertexLocation;
	tree->StartIndexLocation = tree->Geo->DrawArgs["tree"].StartIndexLocation;
	tree->srv = m_Textures["tree"].GetSRV();

	m_LandRenders[(int)RenderLayer::AlphaTestedBillboard].push_back(tree.get());

	m_AllRenders.push_back(std::move(land));
	m_AllRenders.push_back(std::move(wave));
	m_AllRenders.push_back(std::move(box));
	m_AllRenders.push_back(std::move(tree));
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
		vertices[i].normal = GetHillsNormal(p.x, p.z);
		vertices[i].tex = grid.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices = grid.GetIndices16();

	const uint32_t vertexBufferSize = sizeof(Vertex);
	const uint32_t indexBufferSize = sizeof(uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->name = "landGeo";
	geo->m_VertexBuffer.Create(L"vertex buff", (UINT)vertices.size(), vertexBufferSize, vertices.data());
	geo->m_IndexBuffer.Create(L"Index Buffer", (UINT)indices.size(), indexBufferSize, indices.data());

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.BaseVertexLocation = 0;
	submesh.StartIndexLocation = 0;
	geo->DrawArgs["land"] = std::move(submesh);

	m_Geometry["landGeo"] = std::move(geo);
}

void GameApp::BuildWavesGeometry()
{
	std::vector<std::uint16_t> indices(3 * mWaves->TriangleCount()); // 3 indices per face
	assert(mWaves->VertexCount() < 0x0000ffff);

	// Iterate over each quad.
	int m = mWaves->RowCount();
	int n = mWaves->ColumnCount();
	int k = 0;
	for (int i = 0; i < m - 1; ++i)
	{
		for (int j = 0; j < n - 1; ++j)
		{
			indices[k] = i * n + j;
			indices[k + 1] = i * n + j + 1;
			indices[k + 2] = (i + 1) * n + j;

			indices[k + 3] = (i + 1) * n + j;
			indices[k + 4] = i * n + j + 1;
			indices[k + 5] = (i + 1) * n + j + 1;

			k += 6; // next quad
		}
	}

	UINT indexBufferSize = sizeof(std::uint16_t);

	auto geo = std::make_unique<MeshGeometry>();
	geo->name = "waveGeo";
	geo->m_IndexBuffer.Create(L"Index Buffer", (UINT)indices.size(), indexBufferSize, indices.data());

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.BaseVertexLocation = 0;
	submesh.StartIndexLocation = 0;
	geo->DrawArgs["wave"] = std::move(submesh);

	m_Geometry["waveGeo"] = std::move(geo);
}

void GameApp::BuildBoxGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(6.0f, 6.0f, 6.0f, 3);

	std::vector<Vertex> vertices(box.Vertices.size());
	for (size_t i = 0; i < box.Vertices.size(); ++i)
	{
		auto& p = box.Vertices[i].Position;
		vertices[i].position = p;
		vertices[i].normal = box.Vertices[i].Normal;
		vertices[i].tex = box.Vertices[i].TexC;
	}

	std::vector<std::uint16_t> indices = box.GetIndices16();

	auto geo = std::make_unique<MeshGeometry>();
	geo->name = "boxGeo";
	geo->m_VertexBuffer.Create(L"vertex buff", (UINT)vertices.size(), sizeof(Vertex), vertices.data());
	geo->m_IndexBuffer.Create(L"Index Buffer", (UINT)indices.size(), sizeof(std::uint16_t), indices.data());

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["sbox"] = std::move(submesh);

	m_Geometry["boxGeo"] = std::move(geo);
}

void GameApp::BuildBillboardGeometry()
{
	struct TreeSpriteVertex
	{
		XMFLOAT3 Pos;
		XMFLOAT2 Size;
	};

	static const int treeCount = 16;
	std::array<TreeSpriteVertex, 16> vertices;
	for (UINT i = 0; i < treeCount; ++i)
	{
		float x = Utility::RandF(-45.0f, 45.0f);
		float z = Utility::RandF(-45.0f, 45.0f);
		float y = GetHillsHeight(x, z);

		// Move tree slightly above land height.
		y += 8.0f;

		vertices[i].Pos = XMFLOAT3(x, y, z);
		vertices[i].Size = XMFLOAT2(20.0f, 20.0f);
	}

	std::array<std::uint16_t, 16> indices =
	{
		0, 1, 2, 3, 4, 5, 6, 7,
		8, 9, 10, 11, 12, 13, 14, 15
	};

	auto geo = std::make_unique<MeshGeometry>();
	geo->name = "treeGeo";

	geo->m_VertexBuffer.Create(L"vertex buffer", (UINT)vertices.size(), sizeof(TreeSpriteVertex), vertices.data());
	geo->m_IndexBuffer.Create(L"vertex buffer", (UINT)indices.size(), sizeof(uint16_t), indices.data());

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["tree"] = submesh;

	m_Geometry["treeGeo"] = std::move(geo);

}

float GameApp::GetHillsHeight(float x, float z) const
{
	return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

XMFLOAT3 GameApp::GetHillsNormal(float x, float z) const
{
	// n = (-df/dx, 1, -df/dz)
	XMFLOAT3 n(
		-0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
		1.0f,
		-0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

	XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
	XMStoreFloat3(&n, unitNormal);

	return n;
}

void GameApp::BuildMaterials()
{
	auto grass = std::make_unique<Material>();
	grass->Name = "grass";
	grass->DiffuseAlbedo = XMFLOAT4(1.0, 1.0, 1.0, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.125f;

	// This is not a good water material definition, but we do not have all the rendering
	// tools we need (transparency, environment reflection), so we fake it for now.
	auto water = std::make_unique<Material>();
	water->Name = "water";
	water->DiffuseAlbedo = XMFLOAT4(1.0, 1.0, 1.0, 0.2f);
	water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;

	auto bricks0 = std::make_unique<Material>();
	bricks0->Name = "bricks0";
	//bricks0->MatCBIndex = 0;
	//bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->DiffuseAlbedo = XMFLOAT4(Colors::ForestGreen);
	bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bricks0->Roughness = 0.1f;

	auto stone0 = std::make_unique<Material>();
	stone0->Name = "stone0";
	//stone0->MatCBIndex = 1;
	//stone0->DiffuseSrvHeapIndex = 1;
	stone0->DiffuseAlbedo = XMFLOAT4(Colors::LightSteelBlue);
	stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	stone0->Roughness = 0.3f;

	auto tile0 = std::make_unique<Material>();
	tile0->Name = "tile0";
	//tile0->MatCBIndex = 2;
	//tile0->DiffuseSrvHeapIndex = 2;
	tile0->DiffuseAlbedo = XMFLOAT4(Colors::LightGray);
	tile0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	tile0->Roughness = 0.2f;

	auto skullMat = std::make_unique<Material>();
	skullMat->Name = "skullMat";
	//skullMat->MatCBIndex = 3;
	//skullMat->DiffuseSrvHeapIndex = 3;
	skullMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	skullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05);
	skullMat->Roughness = 0.3f;

	auto wirefence = std::make_unique<Material>();
	wirefence->Name = "wirefence";
	wirefence->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wirefence->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	wirefence->Roughness = 0.25f;

	auto treeSprites = std::make_unique<Material>();
	treeSprites->Name = "tree";
	treeSprites->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	treeSprites->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	treeSprites->Roughness = 0.125f;

	m_Materials["grass"] = std::move(grass);
	m_Materials["water"] = std::move(water);
	m_Materials["bricks0"] = std::move(bricks0);
	m_Materials["stone0"] = std::move(stone0);
	m_Materials["tile0"] = std::move(tile0);
	m_Materials["skullMat"] = std::move(skullMat);
	m_Materials["wirefence"] = std::move(wirefence);
	m_Materials["tree"] = std::move(treeSprites);
}

void GameApp::LoadTextures()
{
	TextureManager::Initialize(L"textures/");

	TextureRef grassTex = TextureManager::LoadDDSFromFile(L"grass.dds");
	if(grassTex.IsValid())
		m_Textures["grass"] = grassTex;

	TextureRef waterTex = TextureManager::LoadDDSFromFile(L"water1.dds");
	if (waterTex.IsValid())
		m_Textures["water"] = waterTex;

	TextureRef tileTex = TextureManager::LoadDDSFromFile(L"tile.dds");
	if (tileTex.IsValid())
		m_Textures["tile"] = tileTex;

	TextureRef stoneTex = TextureManager::LoadDDSFromFile(L"stone.dds");
	if (stoneTex.IsValid())
		m_Textures["stone"] = stoneTex;

	TextureRef bricks2Tex = TextureManager::LoadDDSFromFile(L"bricks2.dds");
	if (bricks2Tex.IsValid())
		m_Textures["bricks2"] = bricks2Tex;

	TextureRef woodTex = TextureManager::LoadDDSFromFile(L"WoodCrate01.dds");
	if (woodTex.IsValid())
		m_Textures["wood"] = woodTex;

	TextureRef WireFenceTex = TextureManager::LoadDDSFromFile(L"WireFence.dds");
	if (WireFenceTex.IsValid())
		m_Textures["wireFence"] = WireFenceTex;

	TextureRef white1x1Tex = TextureManager::LoadDDSFromFile(L"white1x1.dds");
	if (white1x1Tex.IsValid())
		m_Textures["white1x1"] = white1x1Tex;

	TextureRef treeTex = TextureManager::LoadDDSFromFile(L"tree02S.dds");
	if (treeTex.IsValid())
		m_Textures["tree"] = treeTex;

	Utility::Printf("Found %u textures\n", m_Textures.size());
	
}

void GameApp::UpdateWaves(float deltaT)
{
	// Every quarter second, generate a random wave.
	static float t_base = 0.0f;

	t_base += deltaT;

	if (t_base >= 0.25f)
	{
		t_base -= 0.25f;

		int i = Utility::Rand(4, mWaves->RowCount() - 5);
		int j = Utility::Rand(4, mWaves->ColumnCount() - 5);

		float r = Utility::RandF(0.2f, 0.5f);

		mWaves->Disturb(i, j, r);
	}


	// Update the wave simulation.
	mWaves->Update(deltaT);

	// Update the wave vertex buffer with the new solution.
	m_VerticesWaves.clear();
	for (int i = 0; i < mWaves->VertexCount(); ++i)
	{
		Vertex v;

		v.position = mWaves->Position(i);
		v.normal = mWaves->Normal(i);

		v.tex.x = 0.5f + v.position.x / mWaves->Width();
		v.tex.y = 0.5f - v.position.z / mWaves->Depth();

		m_VerticesWaves.push_back(v);
	}
	AnimateMaterials(deltaT);

	m_Geometry["waveGeo"]->m_VertexBuffer.Create(L"vertex buffer", m_VerticesWaves.size(), sizeof(Vertex), m_VerticesWaves.data());
}

void GameApp::AnimateMaterials(float deltaT)
{
	XMFLOAT4X4 matTrans;
	XMStoreFloat4x4(&matTrans, m_WavesRitem->Mat->MatTransform);
	float& tu = matTrans(3, 0);
	float& tv = matTrans(3, 1);

	tu += 0.01f * deltaT;
	tv += 0.002f * deltaT;
	
	m_WavesRitem->Mat->MatTransform = XMLoadFloat4x4(&matTrans);
}

