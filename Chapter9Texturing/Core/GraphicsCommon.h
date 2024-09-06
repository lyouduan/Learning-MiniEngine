#pragma once

#include "pch.h"

class CommandSignature;
class RootSignature;
class GraphicsPSO;

namespace Graphics
{
	void InitializeCommonState(void);
	void DestroyCommonState(void);

    extern D3D12_RASTERIZER_DESC RasterizerDefault;
    extern D3D12_RASTERIZER_DESC RasterizerDefaultMsaa;
    extern D3D12_RASTERIZER_DESC RasterizerDefaultCCw;
    extern D3D12_RASTERIZER_DESC RasterizerDefaultCwMsaa;
    extern D3D12_RASTERIZER_DESC RasterizerTwoSided;
    extern D3D12_RASTERIZER_DESC RasterizerTwoSidedMsaa;
    extern D3D12_RASTERIZER_DESC RasterizerShadow;
    extern D3D12_RASTERIZER_DESC RasterizerShadowCW;
    extern D3D12_RASTERIZER_DESC RasterizerShadowTwoSided;

    extern D3D12_BLEND_DESC BlendNoColorWrite;		// XXX
    extern D3D12_BLEND_DESC BlendDisable;			// 1, 0
    extern D3D12_BLEND_DESC BlendPreMultiplied;		// 1, 1-SrcA
    extern D3D12_BLEND_DESC BlendTraditional;		// SrcA, 1-SrcA
    extern D3D12_BLEND_DESC BlendAdditive;			// 1, 1
    extern D3D12_BLEND_DESC BlendTraditionalAdditive;// SrcA, 1

    extern D3D12_DEPTH_STENCIL_DESC DepthStateDisabled;
    extern D3D12_DEPTH_STENCIL_DESC DepthStateReadWrite;
    extern D3D12_DEPTH_STENCIL_DESC DepthStateReadOnly;
    extern D3D12_DEPTH_STENCIL_DESC DepthStateReadOnlyReversed;
    extern D3D12_DEPTH_STENCIL_DESC DepthStateTestEqual;

    extern CommandSignature DispatchIndirectCommandSignature;
    extern CommandSignature DrawIndirectCommandSignature;
    

    extern RootSignature g_CommonRS;
    extern GraphicsPSO g_DownsampleDepthPSO;
};

