#pragma once

#include "pch.h"

namespace GameCore
{
	extern bool gIsSupending;

	class IGameApp
	{
	public:
		// this function can be used to initialize application state and will run after essential
		// hardware resource are allocated. Some state that does not depend on these resource
		// should still be initialized in the constructor such as pointers and flags
		virtual void Startup(void) = 0;
		virtual void Cleanup(void) = 0;


		// Decide if you want the app to exit
		virtual void IsDone(void);

		// the update method will be invoked once per frame. Both state updating and scene
		// rendering should be handled by this method
		virtual void Update(float deltaT) = 0;

		// Official rendering pass
		virtual void RenderScene(void) = 0;

		// Optional UI (overlay) rendering pass.  This is LDR.  The buffer is already cleared.
		//virtual void RenderUI(class GraphicsContext&) {};

		// Override this in applications that use DirectX Raytracing to require a DXR-capable device.
		//virtual bool RequiresRaytracingSupport() const { return false; }
	};
}

namespace GameCore
{
	int RunApplication(IGameApp& app, const wchar_t* className, HINSTANCE hInst, int nCmdShow);
}

#define CREATE_APPLICATION( app_class ) \
    int WINAPI wWinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE /*hPrevInstance*/, _In_ LPWSTR /*lpCmdLine*/, _In_ int nCmdShow) \
    { \
        return GameCore::RunApplication( app_class(), L#app_class, hInstance, nCmdShow ); \
    }

