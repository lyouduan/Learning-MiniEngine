#pragma once

#include "GameCore.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "GpuBuffer.h"
#include <DirectXMath.h>
#include "Waves.h"

// render item
struct RenderItem
{
	RenderItem() = default;
	DirectX::XMMATRIX world;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	// buffer
	StructuredBuffer m_VertexBuffer;
	ByteAddressBuffer m_IndexBuffer;

	// params of DrawIndexedInstanced
	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	int  BaseVertexLocation = 0;
};

class DepthBuffer;
class GameApp : public GameCore::IGameApp
{
public:

	GameApp(void);

	virtual void Startup(void) override;
	virtual void Cleanup(void) override;

	virtual void Update(float deltaT) override;
	virtual void RenderScene(void) override;

private:

	void BuildLandGeometry();
	void BuildWavesGeometry();
	float GetHillsHeight(float x, float z)const;
	void UpdateWaves(float deltaT);

	RootSignature m_RootSignature;
	GraphicsPSO m_PSO;
	

	struct Vertex {
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
	};

	// waves
	std::unique_ptr<Waves> mWaves;
	RenderItem* m_WavesRitem;
	std::vector<Vertex> m_VerticesWaves;

	// List of all the render items.
	std::vector < std::unique_ptr<RenderItem>> m_AllRenders;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_Scissor;
	float m_aspectRatio;

	DirectX::XMMATRIX m_View;
	DirectX::XMMATRIX m_Projection;
	DirectX::XMFLOAT4X4 m_MVP;
	float m_radius = 5.0f;
	// x方向弧度
	float m_xRotate = 0.0f;
	float m_xLast = 0.0f;
	float m_xDiff = 0.0f;
	// y方向弧度
	float m_yRotate = 0.0f;
	float m_yLast = 0.0f;
	float m_yDiff = 0.0f;
};