#pragma once
#include "RootSignature.h"
#include "ColorBuffer.h"
#include "DepthBuffer.h"
#include "PipelineState.h"
#include "CommandContext.h"

class Blur
{
public:

	Blur(UINT width, UINT height, DXGI_FORMAT format, UINT mipNums = 1);
	Blur(const Blur& rhs) = delete;
	Blur& operator=(const Blur& rhs) = delete;
	~Blur();

	void CreateComputeRootSig();

	ColorBuffer& GetOutput() { return Output0; }
	DXGI_FORMAT GetFormat() const { return m_Format; }
	UINT GetWidth() const { return m_Width; }
	UINT GetHeight() const { return m_Height; }

	void Execute(DepthBuffer& input, int BlurCount);

	void GenerateMipMaps();

private:

	const int MaxBlurRadius = 5;

	std::vector<float> CalcGaussWeights(float sigma);

	ColorBuffer Output0;
	ColorBuffer Output1;

	UINT m_MipNums;
	UINT m_Width;
	UINT m_Height;
	DXGI_FORMAT m_Format;

	RootSignature m_ComputeRootSig;
	RootSignature m_MipmapRootSig;

	std::unordered_map<std::string, ComputePSO> m_PSOs;
};

