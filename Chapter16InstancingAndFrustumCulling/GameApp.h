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
#include "Camera.h"

#include <DirectXCollision.h>

enum class RenderLayer : int
{
	Opaque = 0,
	AlphaTested,
	Transparent,
	Count
};

// render item
struct RenderItem
{
	RenderItem() = default;
	DirectX::XMMATRIX World = XMMatrixIdentity();
	DirectX::XMMATRIX TexTransform = XMMatrixIdentity();
	DirectX::XMMATRIX MatTransform = XMMatrixIdentity();

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	std::vector<Instances> inst;

	DirectX::BoundingBox Bound;

	MeshGeometry* Geo = nullptr;

	Material* Mat = nullptr;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1; 

	UINT IndexCount = 0;
	UINT InstanceCount = 0;
	UINT StartIndexLocation = 0;
	UINT BaseVertexLocation = 0;

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

	void BuildRenderItems();

	void BuildSkullGeometry();

	void BuildMaterials();
	void LoadTextures();

	void UpdateInstanceIndex(float deltaT);
	void UpdateCamera(float deltaT);

	RootSignature m_RootSignature;

	std::unordered_map<std::string, GraphicsPSO> m_PSOs;

	// List of all the render items.
	std::vector < std::unique_ptr<RenderItem>> m_AllRenders;
	//std::vector < std::unique_ptr<RenderItem>> m_LandRenders;
	std::vector<RenderItem*> m_LayerRenders[(int)RenderLayer::Count];

	// materials
	std::unordered_map<std::string, std::unique_ptr<Material>> m_Materials;

	// geometry
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_Geometry;
	// texture manager
	std::vector<TextureRef> m_Textures; // work
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_srvs;
	
	BoundingFrustum mCamFrustum;
	// switch render scene
	bool m_bFrustumCulling = true;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_Scissor;
	float m_aspectRatio;

	DirectX::XMMATRIX m_View;
	DirectX::XMMATRIX m_Projection;

	PassConstants passConstant;
	StructuredBuffer matBuffer;
	StructuredBuffer InstBuffer;

	// camera
	Math::Camera camera;

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