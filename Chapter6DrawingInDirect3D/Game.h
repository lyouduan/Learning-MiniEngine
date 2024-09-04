#pragma once
#include "GameCore.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "GpuBuffer.h"
#include "GraphicsCommon.h"
#include "Color.h"

#include <DirectXMath.h>

using namespace DirectX;
using namespace Math;
class RootSignature;
class GraphicsPSO;
class StructuredBuffer;
class ByteAddressBuffer;


class Game : public GameCore::IGameApp
{
public:
	Game();

	virtual void Startup(void) override;
	virtual void Cleanup(void) override;
	
	virtual void Update(float deltaT) override;
	virtual void RenderScene(void) override;

private:

	RootSignature m_RootSignature;
	GraphicsPSO m_PSO;
	StructuredBuffer m_VertexBuffer;
	ByteAddressBuffer m_IndexBuffer;
	
	//XMMATRIX m_MVP;
	XMMATRIX m_MVP;

	Color m_color;
	D3D12_VIEWPORT m_MainViewport;
	D3D12_RECT m_MainScissor;

	// camera

	// 半径
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

