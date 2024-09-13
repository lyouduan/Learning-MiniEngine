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

	MeshGeometry* Geo = nullptr;

	Material* Mat = nullptr;

	// Index into GPU constant buffer corresponding to the ObjectCB for this render item.
	UINT ObjCBIndex = -1; 

	UINT IndexCount = 0;
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

	void DrawRenderItems(GraphicsContext& gfxContext, std::vector<std::unique_ptr<RenderItem>>& items);

	void BuildLandRenderItems();
	void BuildShapeRenderItems();
	void BuildLandGeometry();
	void BuildWavesGeometry();
	void BuildShapeGeometry();
	void BuildBoxGeometry();
	void BuildSkullGeometry();

	float GetHillsHeight(float x, float z)const;
	DirectX::XMFLOAT3 GetHillsNormal(float x, float z)const;
	void UpdateWaves(float deltaT);
	void AnimateMaterials(float deltaT);

	void BuildMaterials();
	void LoadTextures();

	void UpdateCamera(float deltaT);

	RootSignature m_RootSignature;

	std::unordered_map<std::string, GraphicsPSO> m_PSOs;

	// switch render scene
	bool m_bRenderShapes = true;

	// waves
	std::unique_ptr<Waves> mWaves;
	RenderItem* m_WavesRitem;
	std::vector<Vertex> m_VerticesWaves;

	// List of all the render items.
	std::vector < std::unique_ptr<RenderItem>> m_ShapeRenders;
	//std::vector < std::unique_ptr<RenderItem>> m_LandRenders;
	std::vector<std::unique_ptr<RenderItem>> m_LandRenders[(int)RenderLayer::Count];

	// materials
	std::unordered_map<std::string, std::unique_ptr<Material>> m_Materials;

	// geometry
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_Geometry;
	// texture manager
	std::vector<TextureRef> m_Textures; // work
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> m_srvs;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_Scissor;
	float m_aspectRatio;

	DirectX::XMMATRIX m_View;
	DirectX::XMMATRIX m_Projection;

	PassConstants passConstant;
	StructuredBuffer matBuffer;

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