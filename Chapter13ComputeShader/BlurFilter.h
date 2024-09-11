#pragma once
#include "ColorBuffer.h"
#include "RootSignature.h"
#include "PipelineState.h"

class BlurFilter
{
public:
	
	BlurFilter(){}

	// delete copy constructor
	BlurFilter(const BlurFilter& rhs) = delete;
	BlurFilter& operator=(const BlurFilter& rhs) = delete;
	~BlurFilter() = default;

	ColorBuffer& GetBlurMap() { return m_BlurMap0; }

	void Initialize(const std::wstring& name, UINT width, UINT height, DXGI_FORMAT format, 
		const Microsoft::WRL::ComPtr<ID3DBlob>& VertBinary, 
		const Microsoft::WRL::ComPtr<ID3DBlob>& HorzBinary);

	void Execute(GpuResource& input, int blurCount);
private:
	
	std::vector<float> CalcGaussWeights(float sigma);

	// max filter count
	const int MaxBlurRadius = 5;

	// size of uav
	UINT m_Width = 0;
	UINT m_Height = 0;
	DXGI_FORMAT m_Format = DXGI_FORMAT_R8G8B8A8_UNORM;

	//  create two uav for two pass filter
	ColorBuffer m_BlurMap0;
	ColorBuffer m_BlurMap1;

	ComputePSO m_PSOVert;
	ComputePSO m_PSOHorz;
	RootSignature m_ComputeRootSig;
};

