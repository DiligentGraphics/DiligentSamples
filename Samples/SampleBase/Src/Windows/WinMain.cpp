/*     Copyright 2015 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
 *
 *  In no event and under no legal theory, whether in tort (including negligence), 
 *  contract, or otherwise, unless required by applicable law (such as deliberate 
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental, 
 *  or consequential damages of any character arising as a result of this License or 
 *  out of the use or inability to use the software (including but not limited to damages 
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and 
 *  all other commercial damages or losses), even if such Contributor has been advised 
 *  of the possibility of such damages.
 */

#include <memory>
#include <Windows.h>
#include "SampleBase.h"
#include "RenderDeviceFactoryD3D11.h"
#include "RenderDeviceFactoryOpenGL.h"
#include "Timer.h"

using namespace Diligent;
using namespace Diligent;

#define TW_STATIC
#include "AntTweakBar.h"

std::unique_ptr<SampleBase> g_pSample;

LRESULT CALLBACK MessageProc(HWND, UINT, WPARAM, LPARAM);

// Create Direct3D device and swap chain
void InitDevice(HWND hWnd, IRenderDevice **ppRenderDevice, IDeviceContext **ppImmediateContext,  ISwapChain **ppSwapChain, bool bUseOpenGL)
{
    EngineCreationAttribs EngineCreationAttribs;
    EngineCreationAttribs.strShaderCachePath = "bin\\tmp\\ShaderCache";
    SwapChainDesc SwapChainDesc;
    SwapChainDesc.SamplesCount = 1;
    if( bUseOpenGL )
    {
#ifdef ENGINE_DLL
        CreateDeviceAndSwapChainGLType CreateDeviceAndSwapChainGL;
        LoadGraphicsEngineOpenGL(CreateDeviceAndSwapChainGL);
#endif
        CreateDeviceAndSwapChainGL(
            EngineCreationAttribs, ppRenderDevice, ppImmediateContext, SwapChainDesc, hWnd, ppSwapChain );
    }
    else
    {
#ifdef ENGINE_DLL
        CreateDeviceAndImmediateContextD3D11Type CreateDeviceAndImmediateContextD3D11;
        CreateSwapChainD3D11Type CreateSwapChainD3D11;
        LoadGraphicsEngineD3D11(CreateDeviceAndImmediateContextD3D11, CreateSwapChainD3D11);
#endif
        CreateDeviceAndImmediateContextD3D11(EngineCreationAttribs, ppRenderDevice, ppImmediateContext);
        CreateSwapChainD3D11( *ppRenderDevice, *ppImmediateContext, SwapChainDesc, hWnd, ppSwapChain );
    }
}

ISwapChain *g_pSwapChain;

// Main
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int cmdShow)
{
#if defined(_DEBUG) || defined(DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    bool bUseOpenGL = true;
    std::wstring CmdLine = GetCommandLine();
    std::wstring Key = L"UseOpenGL=";
    auto pos = CmdLine.find( Key );
    if( pos != std::string::npos )
    {
        pos += Key.length();
        auto Val = CmdLine.substr( pos, 4 );
        bUseOpenGL = (Val == std::wstring( L"true" ));
    }

    // Register our window class
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX), CS_HREDRAW|CS_VREDRAW, MessageProc,
                        0L, 0L, instance, NULL, NULL, NULL, NULL, L"SampleApp", NULL };
    RegisterClassEx(&wcex);

    // Create a window
    RECT rc = { 0, 0, 1280, 1024 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND wnd = CreateWindow(L"SampleApp", bUseOpenGL ? L"Graphics engine sample (OpenGL)" : L"Graphics engine sample (DirectX)", 
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 
                            rc.right-rc.left, rc.bottom-rc.top, NULL, NULL, instance, NULL);
    if (!wnd)
    {
        MessageBox(NULL, L"Cannot create window", L"Error", MB_OK|MB_ICONERROR);
        return 0;
    }
    ShowWindow(wnd, cmdShow);
    UpdateWindow(wnd);

    RefCntAutoPtr<IRenderDevice> pRenderDevice;
    RefCntAutoPtr<IDeviceContext> pDeviceContext;
    Diligent::RefCntAutoPtr<ISwapChain> pSwapChain;
    InitDevice( wnd, &pRenderDevice, &pDeviceContext, &pSwapChain, bUseOpenGL );
    g_pSwapChain = pSwapChain;

    // Initialize AntTweakBar
    // TW_OPENGL and TW_OPENGL_CORE were designed to select rendering with 
    // very old GL specification. Using these modes results in applying some 
    // odd offsets which distorts everything
    // Latest OpenGL works very much like Direct3D11, and 
    // Tweak Bar will never know if D3D or OpenGL is actually used
    if (!TwInit(TW_DIRECT3D11, pRenderDevice.RawPtr(), pDeviceContext.RawPtr()))
    {
        MessageBoxA(wnd, TwGetLastError(), "AntTweakBar initialization failed", MB_OK|MB_ICONERROR);
        return 0;
    }

    g_pSample.reset( CreateSample(pRenderDevice, pDeviceContext, pSwapChain) );
    g_pSample->WindowResize( g_pSwapChain->GetDesc().Width, g_pSwapChain->GetDesc().Height );


    Timer Timer;
    auto PrevTime = Timer.GetElapsedTime();

    // Main message loop
    MSG msg = {0};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            auto CurrTime = Timer.GetElapsedTime();
            auto ElapsedTime = CurrTime - PrevTime;
            PrevTime = CurrTime;
            g_pSample->Update(CurrTime, ElapsedTime);
            g_pSample->Render();

	        // Draw tweak bars
            TwDraw();

            pSwapChain->Present();
        }
    }
    
    TwTerminate();

    g_pSample.reset();
    pRenderDevice.Release();

    return (int)msg.wParam;
}

// Called every time the application receives a message
LRESULT CALLBACK MessageProc(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    // Send event message to AntTweakBar
    if (TwEventWin(wnd, message, wParam, lParam))
        return 0; // Event has been handled by AntTweakBar
    
    switch (message) 
    {
        case WM_PAINT:
            {
                PAINTSTRUCT ps;
                BeginPaint(wnd, &ps);
                EndPaint(wnd, &ps);
                return 0;
            }
        case WM_SIZE: // Window size has been changed
            if( g_pSwapChain )
            {
                g_pSwapChain->Resize(LOWORD(lParam), HIWORD(lParam));
                g_pSample->WindowResize( g_pSwapChain->GetDesc().Width, g_pSwapChain->GetDesc().Height );
            }
            return 0;
        case WM_CHAR:
            if (wParam == VK_ESCAPE)
                PostQuitMessage(0);
            return 0;
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProc(wnd, message, wParam, lParam);
    }
}
