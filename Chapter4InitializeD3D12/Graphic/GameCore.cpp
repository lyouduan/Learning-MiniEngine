//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
// Developed by Minigraph
//
// Author:  James Stanard 
//

#include "pch.h"
#include "GameCore.h"
#include <shellapi.h>

#pragma comment(lib, "runtimeobject.lib") 

namespace GameCore
{

    bool gIsSupending = false;

    void InitializeApplication( IGameApp& game )
    {
        game.Startup();
    }

    void TerminateApplication( IGameApp& game )
    {
        game.Cleanup();
    }

    void UpdateApplication( IGameApp& game )
    {
        
        game.Update(10);
        game.RenderScene();

    }

    HWND g_hWnd = nullptr;

    LRESULT CALLBACK WndProc( HWND, UINT, WPARAM, LPARAM );

    int RunApplication( IGameApp& app, const wchar_t* className, HINSTANCE hInst, int nCmdShow )
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
        assert(0 != RegisterClassEx(&wcex) && "Unable to register a window");

        // Create window
        //RECT rc = { 0, 0, (LONG)g_DisplayWidth, (LONG)g_DisplayHeight };
        RECT rc = { 0,0, (LONG)800, (LONG)600 };
        AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

        g_hWnd = CreateWindow(className, className, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
            rc.right - rc.left, rc.bottom - rc.top, nullptr, nullptr, hInst, nullptr);

        assert(g_hWnd != 0);

        InitializeApplication(app);

        ShowWindow( g_hWnd, nCmdShow/*SW_SHOWDEFAULT*/ );

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

        return 0;
    }

    //--------------------------------------------------------------------------------------
    // Called every time the application receives a message
    //--------------------------------------------------------------------------------------
    LRESULT CALLBACK WndProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
    {
        switch( message )
        {
        case WM_SIZE:
            break;

        case WM_DESTROY:
            PostQuitMessage(0);
            break;

        default:
            return DefWindowProc( hWnd, message, wParam, lParam );
        }

        return 0;
    }
}
