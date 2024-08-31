#pragma once
#include "GameCore.h"
#include "RootSignature.h"
#include "PipelineSate.h"

class RootSignature;
class GraphicsPSO;

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

	D3D12_VIEWPORT m_MainViewport;
	D3D12_RECT m_MainScissor;
};

