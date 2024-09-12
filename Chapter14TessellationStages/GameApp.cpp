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

	//mWaves = std::make_unique<Waves>(128, 128, 1.0f, 0.03f, 4.0f, 0.2f);

}

void GameApp::Startup(void)
{
	// prepare material
	BuildMaterials();

	// prepare shape and add material
	BuildQuadPatchGeometry();
	BuildBoxGeometry();

	// build render items
	BuildRenderItems();

	// initialize root signature
	m_RootSignature.Reset(2, 0);
	m_RootSignature[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL);
	m_RootSignature[1].InitAsConstantBuffer(1, D3D12_SHADER_VISIBILITY_ALL);

	m_RootSignature.Finalize(L"root sinature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	
	// shader input layout
	D3D12_INPUT_ELEMENT_DESC mInputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_INPUT_ELEMENT_DESC mInputLayout2[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	DXGI_FORMAT ColorFormat = g_DisplayPlane[g_CurrentBuffer].GetFormat();
	DXGI_FORMAT DepthFormat = g_SceneDepthBuffer.GetFormat();

	// shader 
	ComPtr<ID3DBlob> vertexBlob;
	ComPtr<ID3DBlob> HSBlob;
	ComPtr<ID3DBlob> DSBlob;
	ComPtr<ID3DBlob> pixelBlob;
	D3DReadFileToBlob(L"shader/VertexShader.cso", &vertexBlob);
	D3DReadFileToBlob(L"shader/HullShader.cso", &HSBlob);
	D3DReadFileToBlob(L"shader/DomainShader.cso", &DSBlob);
	D3DReadFileToBlob(L"shader/PixelShader.cso", &pixelBlob);

	ComPtr<ID3DBlob> vertexBlob2;
	ComPtr<ID3DBlob> pixelBlob2;
	D3DReadFileToBlob(L"shader/VS.cso", &vertexBlob2);
	D3DReadFileToBlob(L"shader/PS.cso", &pixelBlob2);

	// PSO
	GraphicsPSO opaquePSO;
	
	auto rater = RasterizerDefault;
	rater.FillMode = D3D12_FILL_MODE_WIREFRAME;
	
	opaquePSO.SetRootSignature(m_RootSignature);
	opaquePSO.SetRasterizerState(rater);
	opaquePSO.SetBlendState(BlendDisable);
	opaquePSO.SetDepthStencilState(DepthStateReadWrite);
	opaquePSO.SetInputLayout(_countof(mInputLayout), mInputLayout);
	opaquePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH);
	opaquePSO.SetRenderTargetFormat(ColorFormat, DepthFormat);
	opaquePSO.SetVertexShader(vertexBlob);
	opaquePSO.SetHullShader(HSBlob);
	opaquePSO.SetDomainShader(DSBlob);
	opaquePSO.SetPixelShader(pixelBlob);
	opaquePSO.Finalize();
	m_PSOs["opaque"] = opaquePSO;

	GraphicsPSO opaquePSO1;
	opaquePSO1.SetRootSignature(m_RootSignature);
	opaquePSO1.SetRasterizerState(rater);
	opaquePSO1.SetBlendState(BlendDisable);
	opaquePSO1.SetDepthStencilState(DepthStateReadWrite);
	opaquePSO1.SetInputLayout(_countof(mInputLayout2), mInputLayout2);
	opaquePSO1.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	opaquePSO1.SetRenderTargetFormat(ColorFormat, DepthFormat);
	opaquePSO1.SetVertexShader(vertexBlob2);
	opaquePSO1.SetPixelShader(pixelBlob2);
	opaquePSO1.Finalize();
	m_PSOs["opaque1"] = opaquePSO1;

}

void GameApp::Cleanup(void)
{
	for (auto& iter : m_AllRenders)
	{
		iter->Geo->m_VertexBuffer.Destroy();
		iter->Geo->m_IndexBuffer.Destroy();
	}
	m_Materials.clear();
	m_Geometry.clear();
	m_PSOs.clear();

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
	m_Projection = XMMatrixPerspectiveFovLH(0.25f * XM_PI, m_aspectRatio, 1.0f, 1000.0f);

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

void GameApp::RenderScene(void)
{
	GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");

	gfxContext.SetViewportAndScissor(m_Viewport, m_Scissor);

	gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

	// clear dsv
	gfxContext.ClearDepthAndStencil(g_SceneDepthBuffer);
	// clear rtv
	g_DisplayPlane[g_CurrentBuffer].SetClearColor(Color(0.0f, 0.0f, 0.0f, 0.0f));

	gfxContext.ClearColor(g_DisplayPlane[g_CurrentBuffer]);

	// set render target
	gfxContext.SetRenderTarget(g_DisplayPlane[g_CurrentBuffer].GetRTV(), g_SceneDepthBuffer.GetDSV());

	//set root signature
	gfxContext.SetRootSignature(m_RootSignature);

	gfxContext.SetDynamicConstantBufferView(1, sizeof(passConstant), &passConstant);

	// draw call
	{
		gfxContext.SetPipelineState(m_PSOs["opaque"]);
		DrawRenderItems(gfxContext, m_QuadRenders[(int)RenderLayer::Opaque]);
	}

	// draw call
	{
		gfxContext.SetPipelineState(m_PSOs["opaque1"]);
		DrawRenderItems(gfxContext, m_QuadRenders[(int)RenderLayer::Transparent]);
	}

	gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_PRESENT);

	gfxContext.Finish();
}

void GameApp::DrawRenderItems(GraphicsContext& gfxContext, std::vector<RenderItem*>& items)
{
	ObjConstants objConstants;
	for (auto& iter : items)
	{
		gfxContext.SetPrimitiveTopology(iter->PrimitiveType);
		gfxContext.SetVertexBuffer(0, iter->Geo->m_VertexBuffer.VertexBufferView());
		gfxContext.SetIndexBuffer(iter->Geo->m_IndexBuffer.IndexBufferView());

		XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(iter->World)); // hlsl 列主序矩阵
		XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(iter->TexTransform)); // hlsl 列主序矩阵
		gfxContext.SetDynamicConstantBufferView(0, sizeof(objConstants), &objConstants);

		gfxContext.DrawIndexedInstanced(iter->IndexCount, 1, iter->StartIndexLocation, iter->BaseVertexLocation, 0);
		//gfxContext.DrawIndexed(iter->IndexCount, iter->StartIndexLocation, iter->BaseVertexLocation);
	}
}

void GameApp::BuildRenderItems()
{
	auto quadPatchRitem = std::make_unique<RenderItem>();
	quadPatchRitem->World = XMMatrixIdentity() * XMMatrixTranslation(0.0, 0.0, 0.0);
	quadPatchRitem->TexTransform = XMMatrixIdentity();
	quadPatchRitem->Geo = m_Geometry["quadpatchGeo"].get();
	quadPatchRitem->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST;
	quadPatchRitem->IndexCount = quadPatchRitem->Geo->DrawArgs["quadpatch"].IndexCount;
	quadPatchRitem->BaseVertexLocation = quadPatchRitem->Geo->DrawArgs["quadpatch"].BaseVertexLocation;
	quadPatchRitem->StartIndexLocation = quadPatchRitem->Geo->DrawArgs["quadpatch"].StartIndexLocation;
	m_QuadRenders[(int)RenderLayer::Opaque].push_back(quadPatchRitem.get());

	m_AllRenders.push_back(std::move(quadPatchRitem));

	auto testbox = std::make_unique<RenderItem>();
	testbox->World = XMMatrixIdentity()*XMMatrixTranslation(0.0, 0.0, -20);
	testbox->TexTransform = XMMatrixIdentity();
	testbox->Geo = m_Geometry["boxGeo"].get();
	testbox->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	testbox->IndexCount = testbox->Geo->DrawArgs["sbox"].IndexCount;
	testbox->BaseVertexLocation = testbox->Geo->DrawArgs["sbox"].BaseVertexLocation;
	testbox->StartIndexLocation = testbox->Geo->DrawArgs["sbox"].StartIndexLocation;
	//m_QuadRenders[(int)RenderLayer::Transparent].push_back(box.get());

	m_AllRenders.push_back(std::move(quadPatchRitem));
	m_AllRenders.push_back(std::move(testbox));
}

void GameApp::BuildQuadPatchGeometry()
{
	std::vector<XMFLOAT3> vertices =
	{
		XMFLOAT3(-10.0f, 0.0f, +10.0f),
		XMFLOAT3(+10.0f, 0.0f, +10.0f),
		XMFLOAT3(-10.0f, 0.0f, -10.0f),
		XMFLOAT3(+10.0f, 0.0f, -10.0f)
	};

	std::vector<std::uint16_t> indices = { 0, 1, 2, 3 };
	
	auto geo = std::make_unique<MeshGeometry>();
	geo->name = "quadpatchGeo";
	geo->m_VertexBuffer.Create(L"vertex buffer", (UINT)vertices.size(), sizeof(XMFLOAT3), vertices.data());
	geo->m_IndexBuffer.Create(L"vertex buffer", (UINT)indices.size(), sizeof(std::uint16_t), indices.data());

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.BaseVertexLocation = 0;
	submesh.StartIndexLocation = 0;

	geo->DrawArgs["quadpatch"] = std::move(submesh);

	m_Geometry["quadpatchGeo"] = std::move(geo);
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
	water->DiffuseAlbedo = XMFLOAT4(1.0, 1.0, 1.0, 0.5f);
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

	TextureRef treeTex = TextureManager::LoadDDSFromFile(L"treeArray.dds");
	if (treeTex.IsValid())
		m_Textures["tree"] = treeTex;

	Utility::Printf("Found %u textures\n", m_Textures.size());
	
}
