#pragma once

#include "GameCore.h"
#include "RootSignature.h"
#include "PipelineState.h"

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

	D3D12_VIEWPORT m_Viewport;
	D3D12_RECT m_Scissor;
};