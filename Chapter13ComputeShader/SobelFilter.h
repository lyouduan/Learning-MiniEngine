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
		m_SobelBuffer.Destroy();
		m_AddBuffer.Destroy();
		m_PSOSobel.DesytroyAll();
		m_PSOAdd.DesytroyAll();
		m_RootSig.DestroyAll();
	}

	void Initialize(const std::wstring& name, UINT width, UINT height, DXGI_FORMAT format, const Microsoft::WRL::ComPtr<ID3DBlob>& sobelCS, const Microsoft::WRL::ComPtr<ID3DBlob>& addCS);

	void Execute(GpuResource& input);

	ColorBuffer& GetSobelResult() { return m_SobelBuffer; }
	ColorBuffer& GetAddResult() { return m_AddBuffer; }
private:

	UINT m_Width;
	UINT m_Height;
	DXGI_FORMAT m_Format;

	ColorBuffer m_SobelBuffer;
	ColorBuffer m_AddBuffer;

	// pso
	ComputePSO m_PSOSobel;
	ComputePSO m_PSOAdd;

	// root signature
	RootSignature m_RootSig;
};

