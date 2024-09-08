#pragma once

#include "GameCore.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "GpuBuffer.h"
#include <DirectXMath.h>
#include "Waves.h"
#include "d3dUtil.h"
#include <memory>
#include "TextureManager.h"

enum class RenderLayer : int
{
	Opaque = 0,
	AlphaTested,
	Transparent,
	Mirrors,
	Reflected,
	Count
};

// render item
struct RenderItem
{
	RenderItem() = default;
	DirectX::XMMATRIX World = XMMatrixIdentity();
	DirectX::XMMATRIX TexTransform = XMMatrixIdentity();

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	MeshGeometry* Geo = nullptr;

	Material* Mat = nullptr;

	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	UINT BaseVertexLocation = 0;

	D3D12_CPU_DESCRIPTOR_HANDLE srv; // point to shader resource view
};

class GraphicsContext;

class GameApp : public GameCore::IGameApp
{
public:

	GameApp(void);

	virtual void Startup(void) override;
	virtual void Cleanup(void) override;

	virtual void Update(float deltaT) override;
	virtual void RenderScene(void) override;

private:

	void DrawRenderItems(GraphicsContext& gfxContext, std::vector<RenderItem*>& items);

	void BuildRoomRenderItems();

	void BuildSkullGeometry();
	void BuildRoomGeometry();

	void BuildMaterials();
	void LoadTextures();

	void UpdatePassCB(float deltaT);
	void UpdateReflectedPassCB(float deltaT);
	void UpdateSkull(float deltaT);

	RootSignature m_RootSignature;

	std::unordered_map<std::string, GraphicsPSO> m_PSOs;

	// reflect skull
	RenderItem* mSkullRitem;
	RenderItem* mReflectedSkullRitem;
	RenderItem* mFloorRitem;

	DirectX::XMFLOAT3 mSkullTranslation = { 0.0f, 1.0f, -5.0f };

	// List of all the render items.
	std::vector<RenderItem*> m_RItemLayer[(int)RenderLayer::Count];
	std::vector<std::unique_ptr<RenderItem>> m_AllRenders;

	// materials
	std::unordered_map<std::string, std::unique_ptr<Material>> m_Materials;
	// geometry
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_Geometry;
	// texture manager
	//std::unordered_map<std::string, D3D12_CPU_DESCRIPTOR_HANDLE> m_Textures; // 直接引用handle不行？
	std::unordered_map<std::string, TextureRef> m_Textures; // work

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_Scissor;
	float m_aspectRatio;

	DirectX::XMMATRIX m_View;
	DirectX::XMMATRIX m_Projection;

	PassConstants passConstant;
	PassConstants reflectedPassConstant;

	float m_radius = 5.0f;
	// x方向弧度
	float m_xRotate = 0.0f;
	float m_xLast = 0.0f;
	float m_xDiff = 0.0f;
	// y方向弧度
	float m_yRotate = 0.0f;
	float m_yLast = 0.0f;
	float m_yDiff = 0.0f;

	float mSunTheta = 0.25f * DirectX::XM_PI;
	float mSunPhi = DirectX::XM_PIDIV4;
};