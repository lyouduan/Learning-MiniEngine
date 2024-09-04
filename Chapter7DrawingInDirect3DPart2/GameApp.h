#pragma once

#include "GameCore.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "GpuBuffer.h"
#include <DirectXMath.h>

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

	RootSignature m_RootSignature;
	GraphicsPSO m_PSO;

	struct Vertex {
		DirectX::XMFLOAT3 position;
		DirectX::XMFLOAT4 color;
	};

	StructuredBuffer m_VertexBuffer;
	ByteAddressBuffer m_IndexBuffer;

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_Scissor;
	float m_aspectRatio;

	DirectX::XMMATRIX m_MVP;
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