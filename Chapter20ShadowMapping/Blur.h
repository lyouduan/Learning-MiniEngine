#pragma once
#include "RootSignature.h"
#include "ColorBuffer.h"
#include "DepthBuffer.h"
#include "PipelineState.h"

class Blur
{
public:

	Blur(UINT width, UINT height, DXGI_FORMAT format);
	Blur(const Blur& rhs) = delete;
	Blur& operator=(const Blur& rhs) = delete;
	~Blur();

	void CreateComputeRootSig();

	ColorBuffer& GetOutput() { return Output1; }
	DXGI_FORMAT GetFormat() const { return m_Format; }
	UINT GetWidth() const { return m_Width; }
	UINT GetHeight() const { return m_Height; }

	void Execute(DepthBuffer& input, int BlurCount);

private:

	const int MaxBlurRadius = 5;

	std::vector<float> CalcGaussWeights(float sigma);

	ColorBuffer Output0;
	ColorBuffer Output1;

	UINT m_Width;
	UINT m_Height;
	DXGI_FORMAT m_Format;

	RootSignature m_ComputeRootSig;
	ComputePSO m_HorzPSO;
	ComputePSO m_VertPSO;
	ComputePSO m_VsmPSO;
};

