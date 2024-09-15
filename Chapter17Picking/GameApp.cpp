#include "pch.h"
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
	 
	m_aspectRatio = static_cast<float>(g_DisplayHeight) / static_cast<float>(g_DisplayWidth);
}

void GameApp::Startup(void)
{
	// prepare material
	BuildMaterials();
	// load Textures
	LoadTextures();

	// prepare shape and add material
	BuildCarGeometry();

	// build render items
	BuildRenderItems();
	

	// initialize root signature
	m_RootSignature.Reset(4, 1);
	m_RootSignature[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL);
	m_RootSignature[1].InitAsBufferSRV(0, D3D12_SHADER_VISIBILITY_ALL, 1);
	m_RootSignature[2].InitAsBufferSRV(1, D3D12_SHADER_VISIBILITY_ALL, 1);
	m_RootSignature[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, m_srvs.size());
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
	ComPtr<ID3DBlob> pPixelBlob;
	D3DReadFileToBlob(L"shader/VertexShader.cso", &vertexBlob);
	D3DReadFileToBlob(L"shader/PixelShader.cso", &pixelBlob);

	// PSO
	GraphicsPSO opaquePSO;
	opaquePSO.SetRootSignature(m_RootSignature);
	opaquePSO.SetRasterizerState(RasterizerDefaultCCw); // yz
	opaquePSO.SetBlendState(BlendDisable);
	opaquePSO.SetDepthStencilState(DepthStateReadWriteRZ);// reversed-z
	opaquePSO.SetInputLayout(_countof(mInputLayout), mInputLayout);
	opaquePSO.SetPrimitiveTopologyType(D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
	opaquePSO.SetRenderTargetFormat(ColorFormat, DepthFormat);
	opaquePSO.SetVertexShader(vertexBlob);
	opaquePSO.SetPixelShader(pixelBlob);
	opaquePSO.Finalize();
	m_PSOs["opaque"] = opaquePSO;

	GraphicsPSO highlightPSO = opaquePSO;
	// Change the depth test from < to <= so that if we draw the same triangle twice, it will
	// still pass the depth test.  This is needed because we redraw the picked triangle with a
	// different material to highlight it.  If we do not use <=, the triangle will fail the 
	// depth test the 2nd time we try and draw it.
	auto depthDesc = DepthStateReadWriteRZ;
	depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_GREATER_EQUAL;

	auto blendDesc = Graphics::BlendTraditional;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	highlightPSO.SetBlendState(blendDesc);
	highlightPSO.SetDepthStencilState(depthDesc);
	highlightPSO.Finalize();
	m_PSOs["highlight"] = highlightPSO;

}

void GameApp::Cleanup(void)
{
	for (auto& iter : m_AllRenders)
	{
		iter->Geo->m_VertexBuffer.Destroy();
		iter->Geo->m_IndexBuffer.Destroy();
	}

	m_Materials.clear();
	m_Textures.clear();
	
}

void GameApp::Update(float deltaT)
{
	// update camera 
	UpdateCamera(deltaT);
	// update Instance
	UpdateInstanceIndex(deltaT);

	Pick();

	if (GameInput::IsFirstPressed(GameInput::kKey_f1))
		m_bFrustumCulling = !m_bFrustumCulling;
	
	m_View = camera.GetViewMatrix();
	m_Projection = camera.GetProjMatrix();

	XMStoreFloat4x4(&passConstant.ViewProj, XMMatrixTranspose(camera.GetViewProjMatrix())); // hlsl 列主序矩阵

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

	XMStoreFloat3(&passConstant.eyePosW, camera.GetPosition());

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
	g_DisplayPlane[g_CurrentBuffer].SetClearColor(Color(0.2f, 0.4, 0.6, 1.0));

	gfxContext.ClearColor(g_DisplayPlane[g_CurrentBuffer]);

	// set render target
	gfxContext.SetRenderTarget(g_DisplayPlane[g_CurrentBuffer].GetRTV(), g_SceneDepthBuffer.GetDSV());

	gfxContext.SetRootSignature(m_RootSignature);

	gfxContext.SetDynamicConstantBufferView(0, sizeof(passConstant), &passConstant);

	// structured buffer
	gfxContext.SetBufferSRV(1, InstBuffer);
	gfxContext.SetBufferSRV(2, matBuffer);

	// srv tables
	gfxContext.SetDynamicDescriptors(3, 0, m_srvs.size(), &m_srvs[0]);

	{
		gfxContext.SetPipelineState(m_PSOs["opaque"]);
		DrawRenderItems(gfxContext, m_LayerRenders[(int)RenderLayer::Opaque]);

		//gfxContext.SetPipelineState(m_PSOs["opaque"]);
		//DrawRenderItems(gfxContext, m_LayerRenders[(int)RenderLayer::Picking]);
	}
	
	gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_PRESENT);

	gfxContext.Finish();
}

void GameApp::DrawRenderItems(GraphicsContext& gfxContext, std::vector<RenderItem*>& items)
{
	for (auto& iter : items)
	{
		gfxContext.SetPrimitiveTopology(iter->PrimitiveType);
		gfxContext.SetVertexBuffer(0, iter->Geo->m_VertexBuffer.VertexBufferView());
		gfxContext.SetIndexBuffer(iter->Geo->m_IndexBuffer.IndexBufferView());

		gfxContext.DrawIndexedInstanced(iter->IndexCount, iter->InstanceCount, iter->StartIndexLocation, iter->BaseVertexLocation, 0);
	}
}

void GameApp::BuildRenderItems()
{
	auto skullRitem = std::make_unique<RenderItem>();
	skullRitem->World = XMMatrixIdentity() * XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(0.0f, 1.0f, 0.0f);
	skullRitem->ObjCBIndex = 0;
	skullRitem->Mat = m_Materials["skullMat"].get();
	skullRitem->Geo = m_Geometry["carGeo"].get();
	skullRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skullRitem->InstanceCount = 0;
	skullRitem->IndexCount = skullRitem->Geo->DrawArgs["car"].IndexCount;
	skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["car"].StartIndexLocation;
	skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["car"].BaseVertexLocation;
	skullRitem->Bound = skullRitem->Geo->DrawArgs["car"].Bound;
	const int n = 5;

	skullRitem->inst.resize(n*n*n);
	skullRitem->InstanceCount = 5 * 5 * 5;

	float width = 200.0f;
	float height = 200.0f;
	float depth = 200.0f;

	float x = -0.5f * width;
	float y = -0.5f * height;
	float z = -0.5f * depth;
	float dx = width / (n - 1);
	float dy = height / (n - 1);
	float dz = depth / (n - 1);
	for (int k = 0; k < n; ++k)
	{
		for (int i = 0; i < n; ++i)
		{
			for (int j = 0; j < n; ++j)
			{
				int index = k * n * n + i * n + j;
				// Position instanced along a 3D grid.
				skullRitem->inst[index].World = XMFLOAT4X4(
					1.0f, 0.0f, 0.0f, 0.0f,
					0.0f, 1.0f, 0.0f, 0.0f,
					0.0f, 0.0f, 1.0f, 0.0f,
					x + j * dx, y + i * dy, z + k * dz, 1.0f);

				XMStoreFloat4x4(&skullRitem->inst[index].TexTransform, XMMatrixScaling(2.0f, 2.0f, 1.0f));
				skullRitem->inst[index].MaterialIndex = index % (m_Materials.size()-1);
			}
		}
	}


	auto pickedRitem = std::make_unique<RenderItem>();
	pickedRitem->World = XMMatrixIdentity();
	pickedRitem->TexTransform = XMMatrixIdentity();
	pickedRitem->ObjCBIndex = 1;
	pickedRitem->Mat = m_Materials["highlight0"].get();
	pickedRitem->Geo = m_Geometry["carGeo"].get();
	pickedRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	pickedRitem->Bound = pickedRitem->Geo->DrawArgs["car"].Bound;

	// Picked triangle is not visible until one is picked.
	pickedRitem->Visible = false;

	// DrawCall parameters are filled out when a triangle is picked.
	pickedRitem->IndexCount = 0;
	pickedRitem->StartIndexLocation = 0;
	pickedRitem->BaseVertexLocation = 0;

	mPickedRitem = skullRitem.get();

	m_LayerRenders[(int)RenderLayer::Opaque].push_back(skullRitem.get());
	m_LayerRenders[(int)RenderLayer::Picking].push_back(pickedRitem.get());

	m_AllRenders.push_back(std::move(skullRitem));
	m_AllRenders.push_back(std::move(pickedRitem));
}

void GameApp::BuildCarGeometry()
{
	std::ifstream fin("../Models/car.txt");

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

	XMFLOAT3 vMinf(INFINITY, INFINITY, INFINITY);
	XMFLOAT3 vMaxf(-INFINITY, -INFINITY, -INFINITY);

	XMVECTOR vMin = XMLoadFloat3(&vMinf);
	XMVECTOR vMax = XMLoadFloat3(&vMaxf);

	std::vector<Vertex> vertices(vcount);
	for (UINT i = 0; i < vcount; ++i)
	{
		fin >> vertices[i].position.x >> vertices[i].position.y >> vertices[i].position.z;
		fin >> vertices[i].normal.x >> vertices[i].normal.y >> vertices[i].normal.z;

		// Model does not have texture coordinates, so just zero them out.
		vertices[i].tex = { 0.0f, 0.0f };

		XMVECTOR P = XMLoadFloat3(&vertices[i].position);

		vMin = XMVectorMin(vMin, P);
		vMax = XMVectorMax(vMax, P);
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
	geo->name = "carGeo";
	geo->m_VertexBuffer.Create(L"vertex buff", (UINT)vertices.size(), sizeof(Vertex), vertices.data());
	geo->m_IndexBuffer.Create(L"Index Buffer", (UINT)indices.size(), sizeof(std::int32_t), indices.data());

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;
	XMStoreFloat3(&submesh.Bound.Center, 0.5f * (vMax + vMin));
	XMStoreFloat3(&submesh.Bound.Extents, 0.5f * (vMax - vMin));

	geo->DrawArgs["car"] = std::move(submesh);

	geo->vertices = std::move(vertices);
	geo->indices = std::move(indices);

	m_Geometry["carGeo"] = std::move(geo);
}

void GameApp::BuildMaterials()
{
	auto grass = std::make_unique<Material>();
	grass->Name = "grass";
	grass->DiffuseMapIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4(1.0, 1.0, 1.0, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.125f;

	// This is not a good water material definition, but we do not have all the rendering
	// tools we need (transparency, environment reflection), so we fake it for now.
	auto water = std::make_unique<Material>();
	water->Name = "water";
	water->DiffuseMapIndex = 1;
	water->DiffuseAlbedo = XMFLOAT4(1.0, 1.0, 1.0, 0.5f);
	water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;

	auto tile0 = std::make_unique<Material>();
	tile0->Name = "tile0";
	tile0->DiffuseMapIndex = 2;
	tile0->DiffuseAlbedo = XMFLOAT4(Colors::LightGray);
	tile0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	tile0->Roughness = 0.2f;

	auto stone0 = std::make_unique<Material>();
	stone0->Name = "stone0";
	stone0->DiffuseMapIndex = 3;
	stone0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	stone0->Roughness = 0.3f;

	auto bricks0 = std::make_unique<Material>();
	bricks0->Name = "bricks0";
	bricks0->DiffuseMapIndex = 4;
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bricks0->Roughness = 0.1f;

	auto skullMat = std::make_unique<Material>();
	skullMat->Name = "skullMat";
	skullMat->DiffuseMapIndex = 5;
	skullMat->DiffuseAlbedo = XMFLOAT4(0.7f, 0.7f, 0.7f, 1.0f);
	skullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05);
	skullMat->Roughness = 0.0f;

	auto highlight0 = std::make_unique<Material>();
	highlight0->Name = "highlight0";
	highlight0->DiffuseMapIndex = 6;
	highlight0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 0.0f, 1.0f);
	highlight0->FresnelR0 = XMFLOAT3(0.06f, 0.06f, 0.06f);
	highlight0->Roughness = 0.0f;

	m_Materials[grass->Name] = std::move(grass);
	m_Materials[tile0->Name] = std::move(tile0);
	m_Materials[water->Name] = std::move(water);
	m_Materials[stone0->Name] = std::move(stone0);
	m_Materials[bricks0->Name] = std::move(bricks0);
	m_Materials[skullMat->Name] = std::move(skullMat);
	m_Materials[highlight0->Name] = std::move(highlight0);

	std::vector<MaterialConstants> mat;
	MaterialConstants temp;
	for (auto& m : m_Materials)
	{
		temp.DiffuseAlbedo = m.second->DiffuseAlbedo;
		temp.FresnelR0 = m.second->FresnelR0;
		temp.DiffuseMapIndex = m.second->DiffuseMapIndex;
		XMStoreFloat4x4(&temp.MatTransform, XMMatrixTranspose(m.second->MatTransform));
		temp.Roughness = m.second->Roughness;
		mat.push_back(temp);
	}
	std::sort(mat.begin(), mat.end(), 
		[](const MaterialConstants& a, const MaterialConstants& b) 
		{return a.DiffuseMapIndex < b.DiffuseMapIndex; }
	);
	matBuffer.Create(L"material buffer", (UINT)m_Materials.size(), sizeof(MaterialConstants), mat.data());
}

void GameApp::LoadTextures()
{
	TextureManager::Initialize(L"../textures/");

	TextureRef grassTex = TextureManager::LoadDDSFromFile(L"grass.dds");
	if(grassTex.IsValid())
		m_Textures.push_back(grassTex);

	TextureRef waterTex = TextureManager::LoadDDSFromFile(L"water1.dds");
	if (waterTex.IsValid())
		m_Textures.push_back(waterTex);

	TextureRef tileTex = TextureManager::LoadDDSFromFile(L"tile.dds");
	if (tileTex.IsValid())
		m_Textures.push_back(tileTex);

	TextureRef stoneTex = TextureManager::LoadDDSFromFile(L"stone.dds");
	if (stoneTex.IsValid())
		m_Textures.push_back(stoneTex);

	TextureRef bricks2Tex = TextureManager::LoadDDSFromFile(L"bricks2.dds");
	if (bricks2Tex.IsValid())
		m_Textures.push_back(bricks2Tex);

	TextureRef WireFenceTex = TextureManager::LoadDDSFromFile(L"WireFence.dds");
	if (WireFenceTex.IsValid())
		m_Textures.push_back(WireFenceTex);

	TextureRef white1x1Tex = TextureManager::LoadDDSFromFile(L"white1x1.dds");
	if (white1x1Tex.IsValid())
		m_Textures.push_back(white1x1Tex);

	Utility::Printf("Found %u textures\n", m_Textures.size());

	//m_srvs.resize(m_Textures.size());
	for (auto& t : m_Textures)
		m_srvs.push_back(t.GetSRV());
}

void GameApp::UpdateInstanceIndex(float deltaT)
{
	XMMATRIX view = camera.GetViewMatrix();
	XMVECTOR vDet = XMMatrixDeterminant(view);
	XMMATRIX invView = XMMatrixInverse(&vDet, view);

	std::vector<Instances> visibleInstance;
	for (auto& e : m_LayerRenders[(int)RenderLayer::Opaque])
	{
		const auto& inst = e->inst;
		Utility::Printf("total Instance counts : %u \n", inst.size());
		for (UINT i = 0; i < (UINT)inst.size(); ++i)
		{
			XMMATRIX world = XMLoadFloat4x4(&inst[i].World);
			XMMATRIX texTransform = XMLoadFloat4x4(&inst[i].TexTransform);
			XMMATRIX matTransform = XMLoadFloat4x4(&inst[i].MatTransform);

			XMVECTOR wDet = XMMatrixDeterminant(world);
			XMMATRIX invWorld = XMMatrixInverse(&wDet, world);

			// View space to the object's local space.
			XMMATRIX viewToLocal = XMMatrixMultiply(invView, invWorld);

			// Transform the camera frustum from view space to the object's local space.
			
			BoundingFrustum localSpaceFrustum;
			mCamFrustum.Transform(localSpaceFrustum, viewToLocal);

			if ((localSpaceFrustum.Contains(e->Bound) != DirectX::DISJOINT) || m_bFrustumCulling)
			{
				Instances temp;
				XMStoreFloat4x4(&temp.World, XMMatrixTranspose(world));
				XMStoreFloat4x4(&temp.TexTransform, XMMatrixTranspose(texTransform));
				XMStoreFloat4x4(&temp.MatTransform, XMMatrixTranspose(matTransform));
				temp.MaterialIndex = inst[i].MaterialIndex;
				visibleInstance.push_back(std::move(temp));
			}
		}

		e->InstanceCount = visibleInstance.size();
	}

	InstBuffer.Create(L"Instance buffer", (UINT)visibleInstance.size(), sizeof(Instances), visibleInstance.data());
	
	Utility::Printf("Instance counts after culling : %u \n", visibleInstance.size());
}

void GameApp::UpdateCamera(float deltaT)
{
	if (GameInput::IsPressed(GameInput::kMouse0) || GameInput::IsPressed(GameInput::kMouse1)) {
		// Make each pixel correspond to a quarter of a degree.
		float dx = m_xLast - GameInput::GetAnalogInput(GameInput::kAnalogMouseX);
		float dy = m_yLast - GameInput::GetAnalogInput(GameInput::kAnalogMouseY);

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
	float z = -m_radius * cosf(m_yRotate) * cosf(m_xRotate);

	camera.SetEyeAtUp({x,y,z}, Math::Vector3(Math::kZero), Math::Vector3(Math::kYUnitVector));
	camera.SetAspectRatio(m_aspectRatio);
	camera.Update();

	BoundingFrustum::CreateFromMatrix(mCamFrustum, camera.GetProjMatrix());
}

void GameApp::Pick()
{
	if (!GameInput::IsPressed(GameInput::kMouse0))
		return;
	POINT point = GameInput::GetCurPos();
	// Make each pixel correspond to a quarter of a degree.

	XMFLOAT4X4 P;
	XMStoreFloat4x4(&P, camera.GetProjMatrix());
	// compute picking ray in view sapce
	float vx = (+0.2 * point.x / g_DisplayWidth  - 1.0f) / P(0,0);
	float vy = (-0.2 * point.y / g_DisplayHeight + 1.0f) / P(1,1);

	// ray definition in view space
	XMVECTOR rayOrigin = XMVectorSet(0.0, 0.0, 0.0, 1.0);
	XMVECTOR rayDir    = XMVectorSet(vx,  vy,  1.0, 0.0);

	XMMATRIX V = camera.GetViewMatrix();
	auto det = XMMatrixDeterminant(V);
	XMMATRIX invView = XMMatrixInverse(&det, V);

	// assume nothing is picked to start, so the picked render-item is invisible

	mPickedRitem->Visible = false;

	// check if we pick an opaque render item. A real app might keep a separate "picking list"
	// of objects that can be selected.
	for (auto ri : m_LayerRenders[(int)RenderLayer::Opaque])
	{
		for (int i = 0; i < ri->InstanceCount; ++i)
		{
			auto item = ri->inst[i];

			XMMATRIX W = XMLoadFloat4x4(&item.World);
			auto det = XMMatrixDeterminant(W);
			XMMATRIX invWorld = XMMatrixInverse(&det, W);
			// tranform ray to view space of mesh
			XMMATRIX toLocal = XMMatrixMultiply(invView, invWorld);

			rayOrigin = XMVector3TransformCoord(rayOrigin, toLocal);
			rayDir = XMVector3TransformNormal(rayDir, toLocal);

			// make the ray direction unit length for the intersection tests;
			rayDir = XMVector3Normalize(rayDir);

			// If we hit the bounding box of the Mesh, then we might have picked a Mesh triangle,
		// so do the ray/triangle tests.
		//
		// If we did not hit the bounding box, then it is impossible that we hit 
		// the Mesh, so do not waste effort doing ray/triangle tests.
			float tmin = 0.0;
			if (ri->Bound.Intersects(rayOrigin, rayDir, tmin))
			{
				auto vertices = ri->Geo->vertices;
				auto indices = ri->Geo->indices;

				UINT triCount = ri->IndexCount / 3;

				tmin = INFINITY;
				for (UINT i = 0; i < triCount; ++i)
				{
					// Indices for this triangle.
					UINT i0 = indices[i * 3 + 0];
					UINT i1 = indices[i * 3 + 1];
					UINT i2 = indices[i * 3 + 2];

					// Vertices for this triangle.
					XMVECTOR v0 = XMLoadFloat3(&vertices[i0].position);
					XMVECTOR v1 = XMLoadFloat3(&vertices[i1].position);
					XMVECTOR v2 = XMLoadFloat3(&vertices[i2].position);

					float t = 0.0f;
					if (TriangleTests::Intersects(rayOrigin, rayDir, v0, v1, v2, t))
					{
						// this is the new nearest picked triangle
						if (t < tmin)
						{
							tmin = t;
							UINT pickedTriangle = i;

							mPickedRitem->Visible = true;
							mPickedRitem->IndexCount = ri->IndexCount;
							mPickedRitem->BaseVertexLocation = 0;
							// offset to the pick triangle in the mesh index buffer;
							mPickedRitem->StartIndexLocation = 0;

							//Picked render item needs same world matrix as object picked.
							mPickedRitem->InstanceCount = 1;
							Instances temp = item;

							temp.MaterialIndex = 6;
							XMMATRIX m = XMMatrixIdentity() * XMMatrixScaling(1.1, 1.1, 1.1);
							XMStoreFloat4x4(&temp.World, m);
							mPickedRitem->inst.resize(1);
							mPickedRitem->inst[0] = std::move(temp);
						}
					}
				}
			}
		}
	}
}
