#pragma once
#include "ColorBuffer.h"
#include "PipelineState.h"
#include "RootSignature.h"

class SobelFilter
{
public:

	SobelFilter() {}

	SobelFilter(const SobelFilter& rhs) = delete;
	SobelFilter& operator=(const SobelFilter& rhs) = delete;
	~SobelFilter() = default;

	void Destroy() 
	{ 
		Output.Destroy(); 
		m_PSO.DesytroyAll();
		m_RootSig.DestroyAll();
	}

	void Initialize(const std::wstring& name, UINT width, UINT height, DXGI_FORMAT format, const Microsoft::WRL::ComPtr<ID3DBlob>& Binary);

	void Execute(ColorBuffer& input);

	ColorBuffer& GetOutput() { return Output; }
private:

	UINT m_Width;
	UINT m_Height;
	DXGI_FORMAT m_Format;

	ColorBuffer Output;
	ColorBuffer input;

	// pso
	ComputePSO m_PSO;

	// root signature
	RootSignature m_RootSig;
};

