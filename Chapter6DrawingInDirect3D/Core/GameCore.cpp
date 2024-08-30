#include "GameCore.h"
#include "GraphicsCore.h"
#include "Display.h"

using namespace Graphics;

namespace GameCore
{
	using namespace Graphics;

	bool gIsSupending = false;

	void InitializeApplication(IGameApp& game)
	{
		Graphics::Initialize();

		game.Startup();
	}

	
	void TerminateApplication(IGameApp& game)
	{
		game.Cleanup();
	}

	void UpdateApplication(IGameApp& game)
	{
		game.Update(0);
		// render
		game.RenderScene();


		// present
		Display::Present();
	}

	void IGameApp::IsDone(void)
	{
		// to do
	}

	HWND g_hWnd = nullptr;

	LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

	int RunApplication(IGameApp& app, const wchar_t* className, HINSTANCE hInst, int nCmdShow) 
	{
		// Register class
		WNDCLASSEX wcex;
		wcex.cbSize = sizeof(WNDCLASSEX);
		wcex.style = CS_HREDRAW | CS_VREDRAW;
		wcex.lpfnWndProc = WndProc;
		wcex.cbClsExtra = 0;
		wcex.cbWndExtra = 0;
		wcex.hInstance = hInst;
		wcex.hIcon = LoadIcon(hInst, IDI_APPLICATION);
		wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
		wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
		wcex.lpszMenuName = nullptr;
		wcex.lpszClassName = className;
		wcex.hIconSm = LoadIcon(hInst, IDI_APPLICATION);
		ASSERT(0 != RegisterClassEx(&wcex), "Unable to register a window");

		// Create window
		RECT rc = { 0, 0, (LONG)g_DisplayWidth, (LONG)g_DisplayHeight };
		//RECT rc = { 0, 0, (LONG)800, (LONG)600 };
		AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

		g_hWnd = CreateWindow(className, className, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
			rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);

		ASSERT(g_hWnd != 0);

		// initialize DirectX
		InitializeApplication(app);

		ShowWindow(g_hWnd, nCmdShow);

		MSG msg = {};

		while (msg.message != WM_QUIT)
		{
			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				UpdateApplication(app);
			}
		}

		TerminateApplication(app);
		Graphics::Shutdown();

		return 0;
	}

	//--------------------------------------------------------------------------------------
	// Called every time the application receives a message
	//--------------------------------------------------------------------------------------
	LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_SIZE:
			//Display::Resize((UINT)(UINT64)lParam & 0xFFFF, (UINT)(UINT64)lParam >> 16);
			break;

		case WM_DESTROY:
			PostQuitMessage(0);
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}

		return 0;
	}
}