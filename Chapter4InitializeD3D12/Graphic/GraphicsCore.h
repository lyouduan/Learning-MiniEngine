#pragma once
#include "pch.h"

class CommandListManager;

namespace Graphics
{
#ifndef RELEASE
	extern const GUID WKPDID_D3DDebugObjectName;
#endif

	using namespace Microsoft::WRL;

	void Initialize(void);

	extern ID3D12Device* g_Device;
	extern CommandListManager g_CommandManager;

}
