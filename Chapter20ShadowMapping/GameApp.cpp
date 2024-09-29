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
	BuildShapeGeometry();
	BuildBoxGeometry();
	BuildSkullGeometry();
	BuildFullQuadGeometry();

	// build render items
	BuildLandRenderItems();
	BuildShapeRenderItems();
	BuildSkyboxRenderItems();

	// build cubemap camera
	BuildCubeFaceCamera(0.0, 2.0, 0.0);

	// create shadowMap
	m_shadowMap = std::make_unique<ShadowMap>(1024, 1024, DXGI_FORMAT_D32_FLOAT);


	// set PSO and Root Signature
	SetPsoAndRootSig();

	m_BlurMap = std::make_unique<Blur>(1024, 1024, DXGI_FORMAT_R32_FLOAT);
	
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
	m_Geometry.clear();
}

void GameApp::Update(float deltaT)
{
	// update camera 
	UpdateCamera(deltaT);

	// update waves
	UpdateWaves(deltaT);

	UpdatePassCB(deltaT);

	// update shadow info
	UpdateShadowTranform(deltaT);

	totalTime += deltaT * 0.1;
	// animate the skull around the center sphere
	XMMATRIX skullScale = XMMatrixScaling(0.2f, 0.2f, 0.2f);
	XMMATRIX skullOffset = XMMatrixTranslation(3.0f, 2.0f, 0.0f);
	// 自转：不偏移
	XMMATRIX skullLocalRotate = XMMatrixRotationY(2.0f * totalTime);
	// 绕着场景中心转：加上偏移量
	XMMATRIX skullGlobalRotate = XMMatrixRotationY(0.5f * totalTime);

	m_SkullRitem->World = skullScale * skullLocalRotate * skullOffset * skullGlobalRotate;

	// switch the scene
	if (GameInput::IsFirstPressed(GameInput::kKey_f1))
		m_bRenderShapes = !m_bRenderShapes;
}

void GameApp::RenderScene(void)
{
	GraphicsContext& gfxContext = GraphicsContext::Begin(L"Scene Render");

	// draw cubemap
	DrawSceneToCubeMap(gfxContext);

	DrawSceneToShadowMap(gfxContext);

	//DrawSceneToDepth2Map(gfxContext);

	// ESM完成之前的绘制
	m_BlurMap->Execute(m_shadowMap->GetShadowBuffer(), 1);

	// reset viewport and scissor
	gfxContext.SetViewportAndScissor(m_Viewport, m_Scissor);

	gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

	// clear dsv
	gfxContext.ClearDepthAndStencil(g_SceneDepthBuffer);
	// clear rtv
	g_DisplayPlane[g_CurrentBuffer].SetClearColor(Color(0.2, 0.2, 0.2, 1.0f));

	gfxContext.ClearColor(g_DisplayPlane[g_CurrentBuffer]);

	// set render target
	gfxContext.SetRenderTarget(g_DisplayPlane[g_CurrentBuffer].GetRTV(), g_SceneDepthBuffer.GetDSV());

	gfxContext.SetRootSignature(m_RootSignature);

	XMStoreFloat4x4(&passConstant.ViewProj, XMMatrixTranspose(camera.GetViewProjMatrix()));
	XMStoreFloat4x4(&passConstant.ShadowTransform, XMMatrixTranspose(m_shadowMap->GetShadowTransform()));
	XMStoreFloat3(&passConstant.eyePosW, camera.GetPosition());
	gfxContext.SetDynamicConstantBufferView(1, sizeof(passConstant), &passConstant);

	// structured buffer
	gfxContext.SetBufferSRV(2, matBuffer);

	// srv tables
	gfxContext.SetDynamicDescriptors(4, 0, m_srvs.size(), &m_srvs[0]);
	gfxContext.SetDynamicDescriptors(5, 0, m_Normalsrvs.size(), &m_Normalsrvs[0]);
	gfxContext.SetDynamicDescriptors(6, 0, 1, &m_BlurMap->GetOutput().GetSRV());

	// draw call
	//if (m_bRenderShapes)
	{
		gfxContext.SetPipelineState(m_PSOs["opaque"]);
		DrawRenderItems(gfxContext, m_ShapeRenders[(int)RenderLayer::Opaque]);
	}
	
	// dynamic cube mapping
	gfxContext.SetPipelineState(m_PSOs["opaque"]);
	gfxContext.SetDynamicDescriptor(3, 0, g_SceneCubeMapBuffer.GetSRV());
	DrawRenderItems(gfxContext, m_ShapeRenders[(int)RenderLayer::OpaqueDynamicReflectors]);

	// debug shadow map
	gfxContext.SetPipelineState(m_PSOs["debug"]);
	DrawRenderItems(gfxContext, m_ShapeRenders[(int)RenderLayer::Quad]);

	// draw sky box at last
	gfxContext.SetPipelineState(m_PSOs["sky"]);
	gfxContext.SetDynamicDescriptor(3, 0, m_cubeMap[0].GetSRV());
	DrawRenderItems(gfxContext, m_SkyboxRenders[(int)RenderLayer::Skybox]);

	gfxContext.TransitionResource(g_DisplayPlane[g_CurrentBuffer], D3D12_RESOURCE_STATE_PRESENT);

	gfxContext.Finish();
}

void GameApp::SetPsoAndRootSig()
{
	// initialize root signature
	m_RootSignature.Reset(7, 1);
	m_RootSignature[0].InitAsConstantBuffer(0, D3D12_SHADER_VISIBILITY_ALL);
	m_RootSignature[1].InitAsConstantBuffer(1, D3D12_SHADER_VISIBILITY_ALL);
	m_RootSignature[2].InitAsBufferSRV(0, D3D12_SHADER_VISIBILITY_ALL, 1);
	m_RootSignature[3].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 0, 1);
	m_RootSignature[4].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, m_srvs.size());
	m_RootSignature[5].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, m_Normalsrvs.size(), D3D12_SHADER_VISIBILITY_ALL, 1);
	m_RootSignature[6].InitAsDescriptorRange(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, D3D12_SHADER_VISIBILITY_ALL, 2);
	// sampler
	m_RootSignature.InitStaticSampler(0, Graphics::SamplerLinearWrapDesc, D3D12_SHADER_VISIBILITY_PIXEL);

	m_RootSignature.Finalize(L"root sinature", D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// shader input layout
	D3D12_INPUT_ELEMENT_DESC mInputLayout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
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
	opaquePSO.SetRasterizerState(RasterizerDefaultCCw); // yz
	opaquePSO.SetBlendState(BlendDisable);
	opaquePSO.SetDepthStencilState(DepthStateReadWrite);// reversed-z
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

	// cubemap 
	// shader 
	ComPtr<ID3DBlob> skyboxVS;
	ComPtr<ID3DBlob> skyboxPS;
	D3DReadFileToBlob(L"shader/skyboxVS.cso", &skyboxVS);
	D3DReadFileToBlob(L"shader/skyboxPS.cso", &skyboxPS);

	// pso
	GraphicsPSO cubemapPSO = opaquePSO;
	auto depthDesc = DepthStateReadWrite;
	depthDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // 大于等于

	rater.CullMode = D3D12_CULL_MODE_NONE; // 禁止
	cubemapPSO.SetDepthStencilState(depthDesc);
	cubemapPSO.SetRasterizerState(rater);
	cubemapPSO.SetVertexShader(skyboxVS);
	cubemapPSO.SetPixelShader(skyboxPS);
	cubemapPSO.Finalize();
	m_PSOs["sky"] = cubemapPSO;

	// shadowPSO
	GraphicsPSO shadowPSO = opaquePSO;
	ComPtr<ID3DBlob> shadowMapVS;
	ComPtr<ID3DBlob> shadowMapPS;
	D3DReadFileToBlob(L"shader/shadowMapVS.cso", &shadowMapVS);
	D3DReadFileToBlob(L"shader/shadowMapPS.cso", &shadowMapPS);

	blend = Graphics::BlendTraditional;
	blend.RenderTarget[0].RenderTargetWriteMask = 0;
	shadowPSO.SetBlendState(blend);
	shadowPSO.SetRasterizerState(RasterizerDefaultCCw); // yz
	shadowPSO.SetRenderTargetFormats(0, nullptr, m_shadowMap->GetFormat());
	shadowPSO.SetVertexShader(shadowMapVS);
	shadowPSO.SetPixelShader(shadowMapPS);
	shadowPSO.Finalize();
	m_PSOs["shadow"] = shadowPSO;

	// shadowPSO
	GraphicsPSO debugPSO = opaquePSO;
	rater.CullMode = D3D12_CULL_MODE_NONE; // not cull 
	debugPSO.SetRasterizerState(rater);

	// shader 
	ComPtr<ID3DBlob> shadowDebugVS;
	ComPtr<ID3DBlob> shadowDebugPS;
	D3DReadFileToBlob(L"shader/shadowDebugVS.cso", &shadowDebugVS);
	D3DReadFileToBlob(L"shader/shadowDebugPS.cso", &shadowDebugPS);
	debugPSO.SetVertexShader(shadowDebugVS);
	debugPSO.SetPixelShader(shadowDebugPS);
	debugPSO.Finalize();

	m_PSOs["debug"] = debugPSO;

	// shadowPSO
	GraphicsPSO shadow2PSO = debugPSO;
	ComPtr<ID3DBlob> depth2PS;
	D3DReadFileToBlob(L"shader/depth2PS.cso", &depth2PS);

	shadow2PSO.SetRenderTargetFormat(g_Depth2Buffer.GetFormat(), g_SceneDepthBuffer.GetFormat());
	shadow2PSO.SetPixelShader(depth2PS);
	shadow2PSO.Finalize();
	m_PSOs["shadow2"] = shadow2PSO;
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
		XMStoreFloat4x4(&objConstants.MatTransform, XMMatrixTranspose(iter->MatTransform)); // hlsl 列主序矩阵
		objConstants.MaterialIndex = iter->ObjCBIndex;

		gfxContext.SetDynamicConstantBufferView(0, sizeof(objConstants), &objConstants);

		gfxContext.DrawIndexedInstanced(iter->IndexCount, 1, iter->StartIndexLocation, iter->BaseVertexLocation, 0);
	}
}

void GameApp::DrawSceneToCubeMap(GraphicsContext& gfxContext)
{
	auto width = Graphics::g_SceneCubeMapBuffer.GetWidth();
	auto height = Graphics::g_SceneCubeMapBuffer.GetHeight();
	D3D12_VIEWPORT mViewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	D3D12_RECT mScissorRect = { 0, 0, (LONG)width, (LONG)height };
	gfxContext.SetViewportAndScissor(mViewport, mScissorRect);

	gfxContext.TransitionResource(g_SceneCubeMapBuffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	gfxContext.TransitionResource(g_CubeMapDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

	//clear rtv
	g_SceneCubeMapBuffer.SetClearColor(Color(0.0f, 0.0f, 0.0f, 0.0f));
	gfxContext.ClearColor(g_SceneCubeMapBuffer);

	gfxContext.SetRootSignature(m_RootSignature);

	// structured buffer
	gfxContext.SetBufferSRV(2, matBuffer);

	// srv tables
	gfxContext.SetDynamicDescriptor(3, 0, m_cubeMap[0].GetSRV());
	gfxContext.SetDynamicDescriptors(4, 0, m_srvs.size(), &m_srvs[0]);
	gfxContext.SetDynamicDescriptors(5, 0, m_Normalsrvs.size(), &m_Normalsrvs[0]);

	for (int i = 0; i < 6; ++i)
	{
		// clear dsv
		gfxContext.ClearDepthAndStencil(g_CubeMapDepthBuffer);
		// set render target
		gfxContext.SetRenderTarget(g_SceneCubeMapBuffer.GetRTV(i), g_CubeMapDepthBuffer.GetDSV());

		// update passCB
		XMStoreFloat4x4(&passConstant.ViewProj, XMMatrixTranspose(cubeCamera[i].GetViewProjMatrix()));
		XMStoreFloat3(&passConstant.eyePosW, cubeCamera[i].GetPosition());
		gfxContext.SetDynamicConstantBufferView(1, sizeof(passConstant), &passConstant);

		// draw call
		gfxContext.SetPipelineState(m_PSOs["opaque"]);
		DrawRenderItems(gfxContext, m_ShapeRenders[(int)RenderLayer::Opaque]);
		// draw sky box at last
		gfxContext.SetPipelineState(m_PSOs["sky"]);
		DrawRenderItems(gfxContext, m_SkyboxRenders[(int)RenderLayer::Skybox]);

	}

	gfxContext.TransitionResource(g_SceneCubeMapBuffer, D3D12_RESOURCE_STATE_GENERIC_READ, true);
}

void GameApp::DrawSceneToShadowMap(GraphicsContext& gfxContext)
{
	// 
	gfxContext.SetViewportAndScissor(m_shadowMap->Viewport(), m_shadowMap->ScissorRect());

	// transition buffer to depth write
	gfxContext.TransitionResource(m_shadowMap->GetShadowBuffer(), D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

	gfxContext.ClearDepth(m_shadowMap->GetShadowBuffer());

	gfxContext.SetRenderTargets(0, nullptr, m_shadowMap->GetDSV());

	gfxContext.SetRootSignature(m_RootSignature);

	XMStoreFloat4x4(&passConstant.ViewProj, XMMatrixTranspose(m_shadowMap->GetLightView() * m_shadowMap->GetLightProj()));
	gfxContext.SetDynamicConstantBufferView(1, sizeof(passConstant), &passConstant);

	// structured buffer
	gfxContext.SetBufferSRV(2, matBuffer);

	// srv tables
	gfxContext.SetDynamicDescriptors(4, 0, m_srvs.size(), &m_srvs[0]);

	gfxContext.SetPipelineState(m_PSOs["shadow"]);

	// draw call
	{
		DrawRenderItems(gfxContext, m_ShapeRenders[(int)RenderLayer::Shadow]);
	}
	
	gfxContext.TransitionResource(m_shadowMap->GetShadowBuffer(), D3D12_RESOURCE_STATE_GENERIC_READ, true);
}

void GameApp::DrawSceneToDepth2Map(GraphicsContext& gfxContext)
{
	// 
	auto width = g_Depth2Buffer.GetWidth();
	auto height = g_Depth2Buffer.GetHeight();
	D3D12_VIEWPORT mViewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	D3D12_RECT mScissorRect = { 0, 0, (LONG)width, (LONG)height };
	gfxContext.SetViewportAndScissor(mViewport, mScissorRect);

	// transition buffer to depth write
	gfxContext.TransitionResource(g_Depth2Buffer, D3D12_RESOURCE_STATE_RENDER_TARGET, true);
	gfxContext.TransitionResource(g_SceneDepthBuffer, D3D12_RESOURCE_STATE_DEPTH_WRITE, true);

	g_Depth2Buffer.SetClearColor(Color{ 1.0f, 1.0f, 1.0f, 1.0f });
	gfxContext.ClearColor(g_Depth2Buffer);
	gfxContext.ClearDepth(g_SceneDepthBuffer);

	gfxContext.SetRenderTarget(g_Depth2Buffer.GetRTV(), g_SceneDepthBuffer.GetDSV());

	gfxContext.SetRootSignature(m_RootSignature);

	XMStoreFloat4x4(&passConstant.ViewProj, XMMatrixTranspose(m_shadowMap->GetLightView() * m_shadowMap->GetLightProj()));
	gfxContext.SetDynamicConstantBufferView(1, sizeof(passConstant), &passConstant);

	// structured buffer
	gfxContext.SetBufferSRV(2, matBuffer);

	// set shadowmap
	gfxContext.SetDynamicDescriptors(6, 0, 1, &m_shadowMap->GetSRV());

	gfxContext.SetPipelineState(m_PSOs["shadow2"]);

	// draw call
	{
		DrawRenderItems(gfxContext, m_ShapeRenders[(int)RenderLayer::FullQuad]);
	}

	gfxContext.TransitionResource(g_Depth2Buffer, D3D12_RESOURCE_STATE_GENERIC_READ, true);
}

void GameApp::BuildCubeFaceCamera(float x, float y, float z)
{
	// Generate the cube map about the given position.
	Math::Vector3 center{ x, y, z };

	// look along each coordinate axis;
	Math::Vector3 targets[6] =
	{
		{x + 1.0f, y + 0.0f, z + 0.0f}, // +X
		{x - 1.0f, y + 0.0f, z + 0.0f}, // -X
		{x + 0.0f, y + 1.0f, z + 0.0f}, // +Y

		{x + 0.0f, y - 1.0f, z + 0.0f}, // -Y
		{x + 0.0f, y + 0.0f, z - 1.0f}, // -Z
		{x + 0.0f, y + 0.0f, z + 1.0f}, // +Z

	};

	// Use world up vector (0,1,0) for all directions except +Y/-Y.  In these cases, we
	// are looking down +Y or -Y, so we need a different "up" vector.
	Math::Vector3 ups[6] =
	{
		{ +0.0f, +1.0f, +0.0f },    // +X
		{ +0.0f, +1.0f, +0.0f },    // -X
		{ +0.0f, +0.0f, +1.0f },    // +Y
		{ +0.0f, +0.0f, -1.0f },    // -Y
		{ +0.0f, +1.0f, +0.0f },    // +Z
		{ +0.0f, +1.0f, +0.0f }     // -Z
	};

	for (int i = 0; i < 6; ++i)
	{
		cubeCamera[i].SetEyeAtUp(
			center,
			targets[i],
			ups[i]
		);
		cubeCamera[i].SetPerspectiveMatrix(XM_PI*0.5f, 1, 0.1, 1000.0f); // 45
		cubeCamera[i].Update();
	}
}

void GameApp::BuildShapeRenderItems()
{
	auto boxRitem = std::make_unique<RenderItem>();
	boxRitem->World = XMMatrixIdentity() * XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f);
	boxRitem->TexTransform = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	boxRitem->ObjCBIndex = 3;
	boxRitem->Mat = m_Materials["bricks0"].get();
	boxRitem->Geo = m_Geometry["shapeGeo"].get();
	boxRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxRitem->IndexCount = boxRitem->Geo->DrawArgs["box"].IndexCount;
	boxRitem->StartIndexLocation = boxRitem->Geo->DrawArgs["box"].StartIndexLocation;
	boxRitem->BaseVertexLocation = boxRitem->Geo->DrawArgs["box"].BaseVertexLocation;

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->World = XMMatrixIdentity();
	gridRitem->TexTransform = XMMatrixScaling(8.0f, 8.0f, 1.0f);
	gridRitem->ObjCBIndex = 2;
	gridRitem->Mat = m_Materials["tile0"].get();
	gridRitem->Geo = m_Geometry["shapeGeo"].get();
	gridRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	
	auto skullRitem = std::make_unique<RenderItem>();
	skullRitem->World = XMMatrixIdentity() * XMMatrixScaling(0.5f, 0.5f, 0.5f) * XMMatrixTranslation(0.0f, 1.0f, 0.0f);
	skullRitem->ObjCBIndex = 6;
	skullRitem->Mat = m_Materials["skullMat"].get();
	skullRitem->Geo = m_Geometry["skullGeo"].get();
	skullRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	skullRitem->IndexCount = skullRitem->Geo->DrawArgs["skull"].IndexCount;
	skullRitem->StartIndexLocation = skullRitem->Geo->DrawArgs["skull"].StartIndexLocation;
	skullRitem->BaseVertexLocation = skullRitem->Geo->DrawArgs["skull"].BaseVertexLocation;
	m_SkullRitem = skullRitem.get();
	
	auto globeRitem = std::make_unique<RenderItem>();
	globeRitem->World = XMMatrixIdentity()* XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 2.0f, 0.0f);
	globeRitem->ObjCBIndex = 7;
	globeRitem->Mat = m_Materials["mirror0"].get();
	globeRitem->Geo = m_Geometry["shapeGeo"].get();
	globeRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	globeRitem->IndexCount = globeRitem->Geo->DrawArgs["sphere"].IndexCount;
	globeRitem->StartIndexLocation = globeRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	globeRitem->BaseVertexLocation = globeRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

	auto quadRitem = std::make_unique<RenderItem>();
	quadRitem->World = XMMatrixIdentity();
	quadRitem->TexTransform = XMMatrixIdentity();
	quadRitem->ObjCBIndex = 1;
	quadRitem->Mat = m_Materials["bricks0"].get();
	quadRitem->Geo = m_Geometry["shapeGeo"].get();
	quadRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	quadRitem->IndexCount = quadRitem->Geo->DrawArgs["quad"].IndexCount;
	quadRitem->StartIndexLocation = quadRitem->Geo->DrawArgs["quad"].StartIndexLocation;
	quadRitem->BaseVertexLocation = quadRitem->Geo->DrawArgs["quad"].BaseVertexLocation;

	m_ShapeRenders[(int)RenderLayer::Opaque].push_back(boxRitem.get());
	m_ShapeRenders[(int)RenderLayer::Opaque].push_back(gridRitem.get());
	m_ShapeRenders[(int)RenderLayer::Opaque].push_back(skullRitem.get());
	m_ShapeRenders[(int)RenderLayer::OpaqueDynamicReflectors].push_back(globeRitem.get());

	m_ShapeRenders[(int)RenderLayer::Shadow].push_back(boxRitem.get());
	m_ShapeRenders[(int)RenderLayer::Shadow].push_back(gridRitem.get());
	m_ShapeRenders[(int)RenderLayer::Shadow].push_back(skullRitem.get());
	m_ShapeRenders[(int)RenderLayer::Shadow].push_back(globeRitem.get());

	m_ShapeRenders[(int)RenderLayer::Quad].push_back(quadRitem.get());


	m_AllRenders.push_back(std::move(boxRitem));
	m_AllRenders.push_back(std::move(gridRitem));
	m_AllRenders.push_back(std::move(skullRitem));
	m_AllRenders.push_back(std::move(globeRitem));
	m_AllRenders.push_back(std::move(quadRitem));

	XMMATRIX brickTexTransform = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	for (int i = 0; i < 5; ++i)
	{
		auto leftCylRitem = std::make_unique<RenderItem>();
		auto rightCylRitem = std::make_unique<RenderItem>();
		auto leftSphereRitem = std::make_unique<RenderItem>();
		auto rightSphereRitem = std::make_unique<RenderItem>();

		XMMATRIX leftCylWorld = XMMatrixTranslation(-5.0f, 1.5f, -10.0f + i * 5.0f);
		XMMATRIX rightCylWorld = XMMatrixTranslation(+5.0f, 1.5f, -10.0f + i * 5.0f);

		XMMATRIX leftSphereWorld = XMMatrixTranslation(-5.0f, 3.5f, -10.0f + i * 5.0f);
		XMMATRIX rightSphereWorld = XMMatrixTranslation(+5.0f, 3.5f, -10.0f + i * 5.0f);

		leftCylRitem->World = leftCylWorld;
		leftCylRitem->TexTransform = brickTexTransform;
		leftCylRitem->ObjCBIndex = 3;
		leftCylRitem->Mat = m_Materials["stone0"].get();
		leftCylRitem->Geo = m_Geometry["shapeGeo"].get();
		leftCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftCylRitem->IndexCount = leftCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		leftCylRitem->StartIndexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		leftCylRitem->BaseVertexLocation = leftCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		rightCylRitem->World = rightCylWorld;
		rightCylRitem->TexTransform = brickTexTransform;
		rightCylRitem->ObjCBIndex = 3;
		rightCylRitem->Mat = m_Materials["stone0"].get();
		rightCylRitem->Geo = m_Geometry["shapeGeo"].get();
		rightCylRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightCylRitem->IndexCount = rightCylRitem->Geo->DrawArgs["cylinder"].IndexCount;
		rightCylRitem->StartIndexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].StartIndexLocation;
		rightCylRitem->BaseVertexLocation = rightCylRitem->Geo->DrawArgs["cylinder"].BaseVertexLocation;

		leftSphereRitem->World = leftSphereWorld;
		leftSphereRitem->TexTransform = brickTexTransform;
		leftSphereRitem->ObjCBIndex = 7;
		leftSphereRitem->Mat = m_Materials["mirror0"].get();
		leftSphereRitem->Geo = m_Geometry["shapeGeo"].get();
		leftSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		leftSphereRitem->IndexCount = leftSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		leftSphereRitem->StartIndexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		leftSphereRitem->BaseVertexLocation = leftSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		rightSphereRitem->World = rightSphereWorld;
		rightSphereRitem->TexTransform = brickTexTransform;
		rightSphereRitem->ObjCBIndex = 7;
		rightSphereRitem->Mat = m_Materials["mirror0"].get();
		rightSphereRitem->Geo = m_Geometry["shapeGeo"].get();
		rightSphereRitem->PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		rightSphereRitem->IndexCount = rightSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
		rightSphereRitem->StartIndexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
		rightSphereRitem->BaseVertexLocation = rightSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

		
		m_ShapeRenders[(int)RenderLayer::Opaque].push_back(leftCylRitem.get());
		m_ShapeRenders[(int)RenderLayer::Opaque].push_back(rightCylRitem.get());
		m_ShapeRenders[(int)RenderLayer::Opaque].push_back(leftSphereRitem.get());
		m_ShapeRenders[(int)RenderLayer::Opaque].push_back(rightSphereRitem.get());

		m_ShapeRenders[(int)RenderLayer::Shadow].push_back(leftCylRitem.get());
		m_ShapeRenders[(int)RenderLayer::Shadow].push_back(rightCylRitem.get());
		m_ShapeRenders[(int)RenderLayer::Shadow].push_back(leftSphereRitem.get());
		m_ShapeRenders[(int)RenderLayer::Shadow].push_back(rightSphereRitem.get());

		m_AllRenders.push_back(std::move(leftCylRitem));
		m_AllRenders.push_back(std::move(rightCylRitem));
		m_AllRenders.push_back(std::move(leftSphereRitem));
		m_AllRenders.push_back(std::move(rightSphereRitem));
	}

}

void GameApp::BuildSkyboxRenderItems()
{
	auto box = std::make_unique<RenderItem>();
	box->World = XMMatrixIdentity();
	box->ObjCBIndex = 0;
	box->Mat = m_Materials["sky"].get();
	box->Geo = m_Geometry["boxGeo"].get();
	box->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	box->IndexCount = box->Geo->DrawArgs["sbox"].IndexCount;
	box->BaseVertexLocation = box->Geo->DrawArgs["sbox"].BaseVertexLocation;
	box->StartIndexLocation = box->Geo->DrawArgs["sbox"].StartIndexLocation;

	m_SkyboxRenders[(int)RenderLayer::Skybox].push_back(box.get());
	m_AllRenders.push_back(std::move(box));


	auto fullQuad = std::make_unique<RenderItem>();
	fullQuad->World = XMMatrixIdentity();
	fullQuad->ObjCBIndex = 0;
	fullQuad->Mat = m_Materials["mirror0"].get();
	fullQuad->Geo = m_Geometry["fullQuadGeo"].get();
	fullQuad->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	fullQuad->IndexCount = fullQuad->Geo->DrawArgs["fullQuad"].IndexCount;
	fullQuad->BaseVertexLocation = fullQuad->Geo->DrawArgs["fullQuad"].BaseVertexLocation;
	fullQuad->StartIndexLocation = fullQuad->Geo->DrawArgs["fullQuad"].StartIndexLocation;

	m_ShapeRenders[(int)RenderLayer::FullQuad].push_back(fullQuad.get());
	m_AllRenders.push_back(std::move(fullQuad));
}

void GameApp::BuildLandRenderItems()
{
	auto land = std::make_unique<RenderItem>();
	land->World = XMMatrixIdentity() * XMMatrixScaling(0.5, 0.5, 0.5) * XMMatrixTranslation(0.0f, -15.0f, -30.f);
	land->TexTransform = XMMatrixScaling(5.0f, 5.0f, 1.0f);
	land->ObjCBIndex = 0;
	land->Mat = m_Materials["grass"].get();
	land->Geo = m_Geometry["landGeo"].get();
	land->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	land->IndexCount = land->Geo->DrawArgs["land"].IndexCount;
	land->BaseVertexLocation = land->Geo->DrawArgs["land"].BaseVertexLocation;
	land->StartIndexLocation = land->Geo->DrawArgs["land"].StartIndexLocation;
	m_LandRenders[(int)RenderLayer::Opaque].push_back(land.get());

	auto wave = std::make_unique<RenderItem>();
	wave->World = XMMatrixIdentity() * XMMatrixScaling(0.6, 0.6, 0.6) * XMMatrixTranslation(0.0f, -15.0f, -30.f);
	wave->TexTransform = XMMatrixScaling(4.0f, 4.0f, 1.0f);
	wave->ObjCBIndex = 1;
	wave->Mat = m_Materials["water"].get();
	wave->Geo = m_Geometry["waveGeo"].get();
	wave->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	wave->IndexCount = wave->Geo->DrawArgs["wave"].IndexCount;
	wave->BaseVertexLocation = wave->Geo->DrawArgs["wave"].BaseVertexLocation;
	wave->StartIndexLocation = wave->Geo->DrawArgs["wave"].StartIndexLocation;
	m_WavesRitem = wave.get();

	m_LandRenders[(int)RenderLayer::Transparent].push_back(wave.get());

	auto box = std::make_unique<RenderItem>();
	box->World = XMMatrixIdentity() * XMMatrixTranslation(.0f, -12.0f, -30.f);
	box->ObjCBIndex = 5;
	box->Mat = m_Materials["wirefence"].get();
	box->Geo = m_Geometry["boxGeo"].get();
	box->PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	box->IndexCount = box->Geo->DrawArgs["sbox"].IndexCount;
	box->BaseVertexLocation = box->Geo->DrawArgs["sbox"].BaseVertexLocation;
	box->StartIndexLocation = box->Geo->DrawArgs["sbox"].StartIndexLocation;

	m_LandRenders[(int)RenderLayer::AlphaTested].push_back(box.get());

	m_AllRenders.push_back(std::move(land));
	m_AllRenders.push_back(std::move(wave));
	m_AllRenders.push_back(std::move(box));
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
		vertices[i].tangent = grid.Vertices[i].TangentU;
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

void GameApp::BuildShapeGeometry()
{
	GeometryGenerator geoGen;
	
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);
	GeometryGenerator::MeshData quad = geoGen.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f); // n

	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = gridVertexOffset + (UINT)grid.Vertices.size();
	UINT cylinderVertexOffset = sphereVertexOffset + (UINT)sphere.Vertices.size();
	UINT quadVertexOffset = cylinderVertexOffset + (UINT)cylinder.Vertices.size();

	// Cache the starting index for each object in the concatenated index buffer.
	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = gridIndexOffset + (UINT)grid.Indices32.size();
	UINT cylinderIndexOffset = sphereIndexOffset + (UINT)sphere.Indices32.size();
	UINT quadIndexOffset = cylinderIndexOffset + (UINT)cylinder.Indices32.size();

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	SubmeshGeometry quadSubmesh;
	quadSubmesh.IndexCount = (UINT)quad.Indices32.size();
	quadSubmesh.StartIndexLocation = quadIndexOffset;
	quadSubmesh.BaseVertexLocation = quadVertexOffset;

	auto totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size() +
		quad.Vertices.size();

	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].position = box.Vertices[i].Position;
		vertices[k].normal = box.Vertices[i].Normal;
		vertices[k].tex = box.Vertices[i].TexC;
		vertices[k].tangent = box.Vertices[i].TangentU;
	}

	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].position = grid.Vertices[i].Position;
		vertices[k].normal = grid.Vertices[i].Normal;
		vertices[k].tex = grid.Vertices[i].TexC;
		vertices[k].tangent = grid.Vertices[i].TangentU;
	}

	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].position = sphere.Vertices[i].Position;
		vertices[k].normal = sphere.Vertices[i].Normal;
		vertices[k].tex = sphere.Vertices[i].TexC;
		vertices[k].tangent = sphere.Vertices[i].TangentU;
	}

	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].position = cylinder.Vertices[i].Position;
		vertices[k].normal = cylinder.Vertices[i].Normal;
		vertices[k].tex = cylinder.Vertices[i].TexC;
		vertices[k].tangent = cylinder.Vertices[i].TangentU;
	}

	for (int i = 0; i < quad.Vertices.size(); ++i, ++k)
	{
		vertices[k].position = quad.Vertices[i].Position;
		vertices[k].normal = quad.Vertices[i].Normal;
		vertices[k].tex = quad.Vertices[i].TexC;
		vertices[k].tangent = quad.Vertices[i].TangentU;
	}

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	indices.insert(indices.end(), std::begin(grid.GetIndices16()), std::end(grid.GetIndices16()));
	indices.insert(indices.end(), std::begin(sphere.GetIndices16()), std::end(sphere.GetIndices16()));
	indices.insert(indices.end(), std::begin(cylinder.GetIndices16()), std::end(cylinder.GetIndices16()));
	indices.insert(indices.end(), std::begin(quad.GetIndices16()), std::end(quad.GetIndices16()));

	auto geo = std::make_unique<MeshGeometry>();
	geo->name = "shapeGeo";
	geo->m_VertexBuffer.Create(L"vertex buff", (UINT)vertices.size(), sizeof(Vertex), vertices.data());
	geo->m_IndexBuffer.Create(L"Index Buffer", (UINT)indices.size(), sizeof(std::uint16_t), indices.data());

	geo->DrawArgs["box"] = std::move(boxSubmesh);
	geo->DrawArgs["grid"] = std::move(gridSubmesh);
	geo->DrawArgs["sphere"] = std::move(sphereSubmesh);
	geo->DrawArgs["cylinder"] = std::move(cylinderSubmesh);
	geo->DrawArgs["quad"] = std::move(quadSubmesh);

	m_Geometry["shapeGeo"] = std::move(geo);
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
		vertices[i].tangent = box.Vertices[i].TangentU;

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

void GameApp::BuildSkullGeometry()
{
	std::ifstream fin("../Models/skull.txt");

	if (!fin)
	{
		MessageBox(0, L"../Models/skull.txt not found.", 0, 0);
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

		XMVECTOR N = XMLoadFloat3(&vertices[i].normal);
		// Generate a tangent vector so normal mapping works.  We aren't applying
		// a texture map to the skull, so we just need any tangent vector so that
		// the math works out to give us the original interpolated vertex normal.
		XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
		if (fabsf(XMVectorGetX(XMVector3Dot(N, up))) < 1.0f - 0.001f)
		{
			XMVECTOR T = XMVector3Normalize(XMVector3Cross(up, N));
			XMStoreFloat3(&vertices[i].tangent, T);
		}
		else
		{
			up = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
			XMVECTOR T = XMVector3Normalize(XMVector3Cross(N, up));
			XMStoreFloat3(&vertices[i].tangent, T);
		}

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

void GameApp::BuildFullQuadGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData quad = geoGen.CreateQuad(-1.0f, 1.0f, 2.0f, 2.0f, 0.0f); // n

	std::vector<Vertex> vertices(quad.Vertices.size());
	for (size_t i = 0; i < quad.Vertices.size(); ++i)
	{
		auto& p = quad.Vertices[i].Position;
		vertices[i].position = p;
		vertices[i].normal = quad.Vertices[i].Normal;
		vertices[i].tex = quad.Vertices[i].TexC;
		vertices[i].tangent = quad.Vertices[i].TangentU;

	}

	std::vector<std::uint16_t> indices = quad.GetIndices16();

	auto geo = std::make_unique<MeshGeometry>();
	geo->name = "fullQuadGeo";
	geo->m_VertexBuffer.Create(L"vertex buff", (UINT)vertices.size(), sizeof(Vertex), vertices.data());
	geo->m_IndexBuffer.Create(L"Index Buffer", (UINT)indices.size(), sizeof(std::uint16_t), indices.data());

	SubmeshGeometry submesh;
	submesh.IndexCount = (UINT)indices.size();
	submesh.StartIndexLocation = 0;
	submesh.BaseVertexLocation = 0;

	geo->DrawArgs["fullQuad"] = std::move(submesh);

	m_Geometry["fullQuadGeo"] = std::move(geo);
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
	grass->DiffuseMapIndex = 0;
	grass->NormalMapIndex = 0;
	grass->DiffuseAlbedo = XMFLOAT4(1.0, 1.0, 1.0, 1.0f);
	grass->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	grass->Roughness = 0.125f;

	// This is not a good water material definition, but we do not have all the rendering
	// tools we need (transparency, environment reflection), so we fake it for now.
	auto water = std::make_unique<Material>();
	water->Name = "water";
	water->DiffuseMapIndex = 1;
	water->NormalMapIndex = 0;
	water->DiffuseAlbedo = XMFLOAT4(1.0, 1.0, 1.0, 0.5f);
	water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;

	auto tile0 = std::make_unique<Material>();
	tile0->Name = "tile0";
	tile0->DiffuseMapIndex = 2;
	tile0->NormalMapIndex = 1;
	tile0->DiffuseAlbedo = XMFLOAT4(Colors::LightGray);
	tile0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	tile0->Roughness = 0.2f;

	auto stone0 = std::make_unique<Material>();
	stone0->Name = "stone0";
	stone0->DiffuseMapIndex = 3;
	stone0->NormalMapIndex = 2;
	stone0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	stone0->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
	stone0->Roughness = 0.3f;

	auto bricks0 = std::make_unique<Material>();
	bricks0->Name = "bricks0";
	bricks0->DiffuseMapIndex = 4;
	bricks0->NormalMapIndex = 2;
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bricks0->Roughness = 0.1f;

	auto wirefence = std::make_unique<Material>();
	wirefence->Name = "wirefence";
	wirefence->DiffuseMapIndex = 5;
	wirefence->NormalMapIndex = 0;
	wirefence->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wirefence->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	wirefence->Roughness = 0.25f;

	auto skullMat = std::make_unique<Material>();
	skullMat->Name = "skullMat";
	skullMat->DiffuseMapIndex = 6;
	skullMat->NormalMapIndex = 0;
	skullMat->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	skullMat->FresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05);
	skullMat->Roughness = 0.3f;

	auto mirror0 = std::make_unique<Material>();
	mirror0->Name = "mirror0";
	mirror0->DiffuseMapIndex = 7;
	mirror0->NormalMapIndex = 0;
	mirror0->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.1f, 1.0f);
	mirror0->FresnelR0 = XMFLOAT3(0.98f, 0.97f, 0.95f);
	mirror0->Roughness = 0.1f;

	auto sky = std::make_unique<Material>();
	sky->Name = "sky";
	sky->DiffuseMapIndex = 8;
	sky->NormalMapIndex = 0;
	sky->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	sky->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	sky->Roughness = 1.0f;

	m_Materials[grass->Name] = std::move(grass);
	m_Materials[tile0->Name] = std::move(tile0);
	m_Materials[water->Name] = std::move(water);
	m_Materials[stone0->Name] = std::move(stone0);
	m_Materials[bricks0->Name] = std::move(bricks0);
	m_Materials[wirefence->Name] = std::move(wirefence);
	m_Materials[skullMat->Name] = std::move(skullMat);
	m_Materials[mirror0->Name] = std::move(mirror0);
	m_Materials[sky->Name] = std::move(sky);

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
	// 需要把TextureRef对象保存下来，出了有效域对象销毁，handle无效了
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

	TextureRef iceTex = TextureManager::LoadDDSFromFile(L"ice.dds");
	if (iceTex.IsValid())
		m_Textures.push_back(iceTex);

	TextureRef cubeMap = TextureManager::LoadDDSFromFile(L"snowcube1024.dds");
	if (cubeMap.IsValid())
		m_cubeMap.push_back(cubeMap);

	Utility::Printf("Found %u diffuse textures\n", m_Textures.size());

	//m_srvs.resize(m_Textures.size());
	for (auto& t : m_Textures)
		m_srvs.push_back(t.GetSRV());

	// normal map
	TextureRef default_nmap = TextureManager::LoadDDSFromFile(L"default_nmap.dds");
	if (default_nmap.IsValid())
		m_NormalTextures.push_back(default_nmap);

	TextureRef tile_nmap = TextureManager::LoadDDSFromFile(L"tile_nmap.dds");
	if (tile_nmap.IsValid())
		m_NormalTextures.push_back(tile_nmap);

	TextureRef bricks2_nmap = TextureManager::LoadDDSFromFile(L"bricks2_nmap.dds");
	if (bricks2_nmap.IsValid())
		m_NormalTextures.push_back(bricks2_nmap);

	Utility::Printf("Found %u Normal textures\n", m_NormalTextures.size());
	for (auto& n : m_NormalTextures)
		m_Normalsrvs.push_back(n.GetSRV());
}

void GameApp::UpdatePassCB(float deltaT)
{
	// up
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

void GameApp::UpdateShadowTranform(float deltaT)
{
	DirectX::BoundingSphere mSceneBounds;
	mSceneBounds.Center = XMFLOAT3(0.0, 0.0, 0.0);
	mSceneBounds.Radius = sqrtf(10.0 * 10.0 + 10.0 * 10.0);

	m_shadowMap->SetToLightSpaceView(passConstant.Lights[0].Direction, mSceneBounds);
}

void GameApp::AnimateMaterials(float deltaT)
{
	XMFLOAT4X4 matTrans;
	XMStoreFloat4x4(&matTrans, m_WavesRitem->MatTransform);
	float& tu = matTrans(3, 0);
	float& tv = matTrans(3, 1);

	tu += 0.01f * deltaT;
	tv += 0.002f * deltaT;
	
	m_WavesRitem->MatTransform = XMLoadFloat4x4(&matTrans);
}

