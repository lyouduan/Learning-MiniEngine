#pragma once
#include "GameCore.h"

class Game : public GameCore::IGameApp
{
public:
	Game() {}

	virtual void Startup(void) override;
	virtual void Cleanup(void) override;
	
	virtual void Update(float deltaT) override;
	virtual void RenderScene(void) override;

private:

	D3D12_VIEWPORT m_MainViewport;
	D3D12_RECT m_MainScissor;
};

