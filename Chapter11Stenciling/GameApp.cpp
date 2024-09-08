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

}

void GameApp::Startup(void)
{
	// prepare material
	BuildMaterials();
	// load Textures
	LoadTextures();

	// prepare shape and add material
	BuildSkullGeometry();
	BuildRoomGeometry();

	// build render items
	BuildRoomRenderItems();

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

	DXGI_FORMAT ColorFormat = g_DisplayPlane[g_CurrentBuffer].GetFormat();
	DXGI_FORMAT DepthFormat = g_SceneDepthBuffer.GetFormat();

	// shader 
	ComPtr<ID3DBlob> vertexBlob;
	ComPtr<ID3DBlob> pixelBlob;
	D3DReadFileToBlob(L"shader/VertexShader.cso", &vertexBlob);
	D3DReadFileToBlob(L"shader/PixelShader.cso", &pixelBlob);

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

	GraphicsPSO reflectedPSO = opaquePSO;
	rater = RasterizerDefault;
	rater.FrontCounterClockwise = TRUE; // winding order not change, so it shuold be opposite
	reflectedPSO.SetRasterizerState(rater);
	reflectedPSO.Finalize();
	m_PSOs["reflected"] = reflectedPSO;

}

void GameApp::Cleanup(void)
{
	for (auto& iter : m_AllRenders)
	{
		iter->Geo->m_VertexBuffer.Destroy();
		iter->Geo->m_IndexBuffer.Destroy();
	}
	
}

void GameApp::Update(float deltaT)
{
	UpdatePassCB(deltaT);
	UpdateReflectedPassCB(deltaT);

	UpdateSkull(deltaT);
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
	g_DisplayPlane[g_CurrentBuffer].SetClearColor(Color(0.2f, 0.4f, 0.6f, 1.0f));

	gfxContext.ClearColor(g_DisplayPlane[g_CurrentBuffer]);

	// set render target
	gfxContext.SetRenderTarget(g_DisplayPlane[g_CurrentBuffer].GetRTV(), g_SceneDepthBuffer.GetDSV());

	//set root signature
	gfxContext.SetPipelineState(m_PSOs["opaque"]);

	gfxContext.SetRootSignature(m_RootSignature);

	gfxContext.SetDynamicConstantBufferView(1, sizeof(passConstant), &passConstant);

	// draw call
	{
		gfxContext.SetPipelineState(m_PSOs["opaque"]);
		DrawRenderItems(gfxContext, m_RItemLayer[(int)RenderLayer::Opaque]);

		// use reflected light direction
		gfxContext.SetDynamicConstantBufferView(1, sizeof(reflectedPassConstant), &reflectedPassConstant);

		gfxContext.SetPipelineState(m_PSOs["reflected"]);
		DrawRenderItems(gfxContext, m_RItemLayer[(int)RenderLayer::Reflected]);

		gfxContext.SetDynamicConstantBufferView(1, sizeof(passConstant), &passConstant);

		gfxContext.SetPipelineState(m_PSOs["transparent"]);
		DrawRenderItems(gfxContext, m_RItemLayer[(int)RenderLayer::Transparent]);

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

void GameApp::BuildRoomRenderItems()
{
	auto floorRitem = std::make_unique<RenderItem>();
	floorRitem->World = XMMatrixIdentity();
	floorRitem->TexTransform = XMMatrixIdentity();
	floorRitem->Mat = m_Materials["checkertile"].get();
	floorRitem->Geo = m_Geometry["roomGeo"].get();
	floorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	floorRitem->IndexCount = floorRitem->Geo->DrawArgs["floor"].IndexCount;
	floorRitem->StartIndexLocation = floorRitem->Geo->DrawArgs["floor"].StartIndexLocation;
	floorRitem->BaseVertexLocation = floorRitem->Geo->DrawArgs["floor"].BaseVertexLocation;
	floorRitem->srv = m_Textures["checkboard"].GetSRV();
	m_RItemLayer[(int)RenderLayer::Opaque].push_back(floorRitem.get());

	auto reflectedFloorRitem = std::make_unique<RenderItem>();
	*reflectedFloorRitem = *floorRitem;
	mFloorRitem = reflectedFloorRitem.get();
	m_RItemLayer[(int)RenderLayer::Reflected].push_back(reflectedFloorRitem.get());
	
	auto wallsRitem = std::make_unique<RenderItem>();
	wallsRitem->World = XMMatrixIdentity();
	wallsRitem->TexTransform = XMMatrixIdentity();
	wallsRitem->Mat = m_Materials["bricks"].get();
	wallsRitem->Geo = m_Geometry["roomGeo"].get();
	wallsRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wallsRitem->IndexCount = wallsRitem->Geo->DrawArgs["wall"].IndexCount;
	wallsRitem->StartIndexLocation = wallsRitem->Geo->DrawArgs["wall"].StartIndexLocation;
	wallsRitem->BaseVertexLocation = wallsRitem->Geo->DrawArgs["wall"].BaseVertexLocation;
	wallsRitem->srv = m_Textures["bricks"].GetSRV();
	m_RItemLayer[(int)RenderLayer::Opaque].push_back(wallsRitem.get());

	auto skullRitem = std::make_unique<RenderItem>();
	skullRitem->World = XMMatrixIdentity();
	skullRitem->TexTransform = XMMatrixIdentity();
	skullRitem->Mat = m_Materials["skullMat"].get();
	skullRitem->Geo = m_Geometry["skullGeo"].get();
	skullRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skull"].IndexCount;
	skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skull"].StartIndexLocation;
	skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skull"].BaseVertexLocation;
	skullRitem->srv = m_Textures["white1x1"].GetSRV();
	mSkullRitem = skullRitem.get();
	m_RItemLayer[(int)RenderLayer::Opaque].push_back(skullRitem.get());

	auto reflectedSkullRitem = std::make_unique<RenderItem>();
	*reflectedSkullRitem = *skullRitem;
	mReflectedSkullRitem = reflectedSkullRitem.get();
	m_RItemLayer[(int)RenderLayer::Reflected].push_back(reflectedSkullRitem.get());

	auto mirrorRitem = std::make_unique<RenderItem>();
	mirrorRitem->World = XMMatrixIdentity();
	mirrorRitem->TexTransform = XMMatrixIdentity();
	mirrorRitem->Mat = m_Materials["icemirror"].get();
	mirrorRitem->Geo = m_Geometry["roomGeo"].get();
	mirrorRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	mirrorRitem->IndexCount = mirrorRitem->Geo->DrawArgs["mirror"].IndexCount;
	mirrorRitem->StartIndexLocation = mirrorRitem->Geo->DrawArgs["mirror"].StartIndexLocation;
	mirrorRitem->BaseVertexLocation = mirrorRitem->Geo->DrawArgs["mirror"].BaseVertexLocation;
	mirrorRitem->srv = m_Textures["ice"].GetSRV();

	

	m_RItemLayer[(int)RenderLayer::Mirrors].push_back(mirrorRitem.get());
	m_RItemLayer[(int)RenderLayer::Transparent].push_back(mirrorRitem.get());

	m_AllRenders.push_back(std::move(reflectedFloorRitem));
	m_AllRenders.push_back(std::move(floorRitem));
	m_AllRenders.push_back(std::move(wallsRitem));
	m_AllRenders.push_back(std::move(skullRitem));
	m_AllRenders.push_back(std::move(reflectedSkullRitem));
	m_AllRenders.push_back(std::move(mirrorRitem));

}

void GameApp::BuildSkullGeometry()
{
	std::ifstream fin("Models/skull.txt");

	if (!fin)
	{
		MessageBox(0, L"Models/skull.txt not found.", 0, 0);
		return;
	}

	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;

	fin >> ignore >> vcount;
	fin >> ignore >> tcount;
	fin >> ignore >> ignore >> ignore >> ignore;

	std::vector<Vertex> vertices(vcount);
	for (UINT i = 0; i < vcount; ++i)
	{
		fin >> vertices[i].position.x >> vertices[i].position.y >> vertices[i].position.z;
		fin >> vertices[i].normal.x >> vertices[i].normal.y >> vertices[i].normal.z;

		// Model does not have texture coordinates, so just zero them out.
		vertices[i].tex = { 0.0f, 0.0f };
	}

	fin >> ignore;
	fin >> ignore;
	fin >> ignore;

	std::vector<std::int32_t> indices(3 * tcount);
	for (UINT i = 0; i < tcount; ++i)
	{
		fin >> indices[i * 3 + 0] >> indices[i * 3 + 1] >> indices[i * 3 + 2];
	}

	fin.close();

	auto geo = std::make_unique<MeshGeometry>();
	geo->name = "skullGeo";
	geo->m_VertexBuffer.Create(L"vertex buff", (UINT)vertices.size(), sizeof(Vertex), vertices.data());
	geo->m_IndexBuffer.Create(L"Index Buffer", (UINT)indices.size(), sizeof(std::int32_t), indices.data());

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["skull"] = std::move(submesh);

	m_Geometry["skullGeo"] = std::move(geo);

}

void GameApp::BuildRoomGeometry()
{
	std::array<Vertex, 20> vertices =
	{
		// Floor: Observe we tile texture coordinates.
		Vertex({-3.5f, 0.0f, -10.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 4.0f}), // 0 
		Vertex({-3.5f, 0.0f,   0.0f}, {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f}),
		Vertex({7.5f,  0.0f,   0.0f}, {0.0f, 1.0f, 0.0f}, {4.0f, 0.0f}),
		Vertex({7.5f,  0.0f, -10.0f}, {0.0f, 1.0f, 0.0f}, {4.0f, 4.0f}),

		// Wall: Observe we tile texture coordinates, and that we
		// leave a gap in the middle for the mirror.
		Vertex({-3.5f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 2.0f}), // 4
		Vertex({-3.5f, 4.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}),
		Vertex({-2.5f, 4.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.5f, 0.0f}),
		Vertex({-2.5f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.5f, 2.0f}),

		Vertex({2.5f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 2.0f}), // 8 
		Vertex({2.5f, 4.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}),
		Vertex({7.5f, 4.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {2.0f, 0.0f}),
		Vertex({7.5f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {2.0f, 2.0f}),
			   
		Vertex({-3.5f, 4.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}), // 12
		Vertex({-3.5f, 6.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}),
		Vertex({7.5f, 6.0f, 0.0f},  {0.0f, 0.0f, -1.0f}, {6.0f, 0.0f}),
		Vertex({7.5f, 4.0f, 0.0f},  {0.0f, 0.0f, -1.0f}, {6.0f, 1.0f}),

		// Mirror
		Vertex({-2.5f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 1.0f}), // 16
		Vertex({-2.5f, 4.0f, 0.0f}, {0.0f, 0.0f, -1.0f}, {0.0f, 0.0f}),
		Vertex({2.5f, 4.0f,  0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 0.0f}),
		Vertex({2.5f, 0.0f,  0.0f}, {0.0f, 0.0f, -1.0f}, {1.0f, 1.0f})
	};

	std::array<std::int16_t, 30> indices =
	{
		// Floor
		0, 1, 2,
		0, 2, 3,

		// Walls
		4, 5, 6,
		4, 6, 7,

		8, 9, 10,
		8, 10, 11,

		12, 13, 14,
		12, 14, 15,

		// Mirror
		16, 17, 18,
		16, 18, 19
	};

	SubmeshGeometry floorSubmesh;
	floorSubmesh.IndexCount = 6;
	floorSubmesh.StartIndexLocation = 0;
	floorSubmesh.BaseVertexLocation = 0;

	SubmeshGeometry wallSubmesh;
	wallSubmesh.IndexCount = 18;
	wallSubmesh.StartIndexLocation = 6;
	wallSubmesh.BaseVertexLocation = 0;

	SubmeshGeometry mirrorSubmesh;
	mirrorSubmesh.IndexCount = 6;
	mirrorSubmesh.StartIndexLocation = 24;
	mirrorSubmesh.BaseVertexLocation = 0;

	auto geo = std::make_unique<MeshGeometry>();
	geo->name = "roomGeo";
	geo->m_VertexBuffer.Create(L"vertex buff", (UINT)vertices.size(), sizeof(Vertex), vertices.data());
	geo->m_IndexBuffer.Create(L"Index Buffer", (UINT)indices.size(), sizeof(std::int16_t), indices.data());

	geo->DrawArgs["floor"]  = std::move(floorSubmesh);
	geo->DrawArgs["wall"]   = std::move(wallSubmesh);
	geo->DrawArgs["mirror"] = std::move(mirrorSubmesh);

	m_Geometry["roomGeo"]   = std::move(geo);
}

void GameApp::BuildMaterials()
{
	auto bricks = std::make_unique<Material>();
	bricks->Name = "bricks";
	bricks->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	bricks->Roughness = 0.25f;

	auto checkertile = std::make_unique<Material>();
	checkertile->Name = "checkertile";
	checkertile->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	checkertile->FresnelR0 = XMFLOAT3(0.07f, 0.07f, 0.07f);
	checkertile->Roughness = 0.3f;

	auto icemirror = std::make_unique<Material>();
	icemirror->Name = "icemirror";
	icemirror->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.3f);
	icemirror->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	icemirror->Roughness = 0.5f;

	auto skullMat = std::make_unique<Material>();
	skullMat->Name = "skullMat";
	skullMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	skullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05);
	skullMat->Roughness = 0.3f;

	auto shadowMat = std::make_unique<Material>();
	shadowMat->Name = "shadowMat";
	shadowMat->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 0.5f);
	shadowMat->FresnelR0 = XMFLOAT3(0.001f, 0.001f, 0.001f);
	shadowMat->Roughness = 0.0f;

	m_Materials["bricks"] = std::move(bricks);
	m_Materials["checkertile"] = std::move(checkertile);
	m_Materials["icemirror"] = std::move(icemirror);
	m_Materials["shadowMat"] = std::move(shadowMat);
	m_Materials["skullMat"] = std::move(skullMat);
}

void GameApp::LoadTextures()
{
	TextureManager::Initialize(L"textures/");

	TextureRef bricks3sTex = TextureManager::LoadDDSFromFile(L"bricks3.dds");
	if(bricks3sTex.IsValid())
		m_Textures["bricks"] = bricks3sTex;

	TextureRef checkboardTex = TextureManager::LoadDDSFromFile(L"checkboard.dds");
	if (checkboardTex.IsValid())
		m_Textures["checkboard"] = checkboardTex;

	TextureRef iceTex = TextureManager::LoadDDSFromFile(L"ice.dds");
	if (iceTex.IsValid())
		m_Textures["ice"] = iceTex;

	TextureRef white1x1Tex = TextureManager::LoadDDSFromFile(L"white1x1.dds");
	if (white1x1Tex.IsValid())
		m_Textures["white1x1"] = white1x1Tex;

	Utility::Printf("Found %u textures\n", m_Textures.size());
	
}

void GameApp::UpdatePassCB(float deltaT)
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
}

void GameApp::UpdateReflectedPassCB(float deltaT)
{
	reflectedPassConstant = passConstant;

	XMVECTOR mirrorPlane = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // xy plane
	XMMATRIX R = XMMatrixReflect(mirrorPlane);

	for (int i = 0; i < 3; ++i)
	{
		XMVECTOR lightDir = XMLoadFloat3(&passConstant.Lights[i].Direction);
		// transforms a 3D vector representing a normal by a matrix.
		XMVECTOR reflectionLightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&reflectedPassConstant.Lights[i].Direction, reflectionLightDir);
	}
}

void GameApp::UpdateSkull(float deltaT)
{
	if (GameInput::IsFirstPressed(GameInput::kKey_a))
		mSkullTranslation.x -= 5.0f * deltaT;
	if (GameInput::IsFirstPressed(GameInput::kKey_d))
		mSkullTranslation.x += 5.0f * deltaT;
	if (GameInput::IsFirstPressed(GameInput::kKey_w))
		mSkullTranslation.y += 5.0f * deltaT;
	if (GameInput::IsFirstPressed(GameInput::kKey_s))
		mSkullTranslation.y -= 5.0f * deltaT;

	// Don't let user move below ground plane
	mSkullTranslation.y = std::max(mSkullTranslation.y, 0.0f);

	// Update the new world matrix.
	XMMATRIX skullRotate = XMMatrixRotationY(0.5f * XM_PI);
	XMMATRIX skullScale = XMMatrixScaling(0.45f, 0.45f, 0.45f);
	XMMATRIX skullOffset = XMMatrixTranslation(mSkullTranslation.x, mSkullTranslation.y, mSkullTranslation.z);
	XMMATRIX skullWorld = skullRotate * skullScale * skullOffset;
	mSkullRitem->World = skullWorld;

	// reflection world matrix
	XMVECTOR mirrorPlane = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f); // xy plane
	XMMATRIX R = XMMatrixReflect(mirrorPlane);
	mReflectedSkullRitem->World = skullWorld * R;


	mFloorRitem->World = XMMatrixIdentity() * R;
}

