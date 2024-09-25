#include "pch.h"
#include "GameCore.h"
#include "GraphicsCore.h"
#include "Display.h"
#include "GameInput.h"
#include "CommandListManager.h"

namespace GameCore
{
    using namespace Graphics;

    void InitializeApplication(IGameApp& game)
    {
        Graphics::Initialize();
        GameInput::Initialize();

        game.Startup();
    }

    void TerminateApplication(IGameApp& game)
    {
        g_CommandManager.IdleGPU();

        game.Cleanup();

        GameInput::Shutdown();
    }

    void UpdateApplication(IGameApp& game)
    {
        game.Update(0.1);
        game.RenderScene();
        GameInput::Update(0.1);

        Display::Present();
    }

    bool IGameApp::IsDone(void)
    {
        return false;
    }

    HWND g_hWnd = nullptr;

    LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

    int RunApplication(IGameApp& app, const wchar_t* className, HINSTANCE hInst, int nCmdShow)
    {
        // register class
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

        RECT rc = { 0, 0, (LONG)Graphics::g_DisplayWidth, (LONG)Graphics::g_DisplayHeight };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

        g_hWnd = CreateWindow(className, className, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);

        ASSERT(g_hWnd != 0);

        InitializeApplication(app);

        ShowWindow(g_hWnd, nCmdShow/*SW_SHOWDEFAULT*/);

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