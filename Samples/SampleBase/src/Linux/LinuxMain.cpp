/*     Copyright 2015-2017 Egor Yusov
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
#include <iomanip>

#include<X11/X.h>
#include<X11/Xlib.h>

    //#include "GL/glew.h"
    #include "GL/glxew.h"
    //#include<GL/gl.h>
    //#include<GL/glx.h>
    //#include<GL/glu.h>
    
// Undef symbols defined by XLib
#ifdef Bool
# undef Bool
#endif
#ifdef True
#   undef True
#endif
#ifdef False
#   undef False
#endif

#include "SampleBase.h"
#include "RenderDeviceFactoryOpenGL.h"
#include "Timer.h"

#define TW_STATIC
#include "AntTweakBar.h"
#include "Errors.h"

using namespace Diligent;
std::unique_ptr<SampleBase> g_pSample;

XVisualInfo             *vi;
Colormap                cmap;
XSetWindowAttributes    swa;
GLXContext              glc;
XWindowAttributes       gwa;
XEvent                  xev;

// Create Direct3D device and swap chain
void InitDevice(void* Wnd, IRenderDevice **ppRenderDevice, IDeviceContext **ppImmediateContext,  ISwapChain **ppSwapChain)
{
    SwapChainDesc SCDesc;
    SCDesc.SamplesCount = 1;

    EngineCreationAttribs EngineCreationAttribs;
    GetEngineFactoryOpenGL()->CreateDeviceAndSwapChainGL(
        EngineCreationAttribs, ppRenderDevice, ppImmediateContext, SCDesc, Wnd, ppSwapChain );
}

int main(int argc, char *argv[]) {

    Display* display = XOpenDisplay(NULL);
    
    if(display == NULL) 
    {
        LOG_ERROR("\n\tcannot connect to X server\n\n");
        exit(1);
    }
            
    Window rootWnd = DefaultRootWindow(display);

    GLint attrList[] = { GLX_RGBA, GLX_DEPTH_SIZE, 24, GLX_DOUBLEBUFFER, None };
    vi = glXChooseVisual(display, 0, attrList);

    if(vi == NULL) 
    {
        LOG_ERROR("\n\tno appropriate visual found\n\n");
        exit(1);
    } 
    else 
    {
        LOG_INFO_MESSAGE("\n\tvisual %p selected\n", (void *)vi->visualid); /* %p creates hexadecimal output like in glxinfo */
    }

    XSetWindowAttributes SetWndAttrs;
    SetWndAttrs.colormap = XCreateColormap(display, rootWnd, vi->visual, AllocNone);
    SetWndAttrs.event_mask = ExposureMask | KeyPressMask;
    
    Window wnd = XCreateWindow(display, rootWnd, 0, 0, 1024, 768, 0, vi->depth, InputOutput, vi->visual, CWColormap | CWEventMask, &SetWndAttrs);

    XMapWindow(display, wnd);
    //XStoreName(dpy, win, "VERY SIMPLE APPLICATION");
    
    auto tmpCtx = glXCreateContext(display, vi, NULL, GL_TRUE);
    glXMakeCurrent(display, wnd, tmpCtx);


    glxewInit();
    int scrnum = DefaultScreen(display);
    int elemc = 0;
    GLXFBConfig *fbcfg = glXChooseFBConfig(display, scrnum, NULL, &elemc);
    if (!fbcfg)
    {
        LOG_ERROR("Couldn't get FB configs\n");
        exit(1);
    }
    else
    {
        LOG_INFO_MESSAGE("Got %d FB configs\n", elemc);
    }


    // OpenGL 3.2
    int glattr[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 3,
        GLX_CONTEXT_MINOR_VERSION_ARB, 2,
        GLX_CONTEXT_FLAGS_ARB, GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB,
        None
    };

    glc = glXCreateContextAttribsARB(display, fbcfg[0], NULL, true, glattr);

    XFree(vi);        

    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, tmpCtx);

    glXMakeCurrent(display, wnd, glc);

    GLenum err = glewInit();
    if( GLEW_OK != err ){
        LOG_ERROR( "Failed to initialize GLEW" );
        exit(1);
    }

    
/*       
        // start GLEW extension handler
        // glewExperimental = GL_TRUE;
        // glewInit();

        // // get version info
        // const GLubyte* renderer = glGetString(GL_RENDERER); // get renderer string
        // const GLubyte* version = glGetString(GL_VERSION); // version as a string
        // printf("Renderer: %s\n", renderer);
        // printf("OpenGL version supported %s\n", version);
*/
    RefCntAutoPtr<IRenderDevice> pRenderDevice;
    RefCntAutoPtr<IDeviceContext> pDeviceContext;
    Diligent::RefCntAutoPtr<ISwapChain> pSwapChain;
    // InitDevice( wnd, &pRenderDevice, &pDeviceContext, &pSwapChain, DevType );
    // g_pSwapChain = pSwapChain;

    // // Initialize AntTweakBar
    // // TW_OPENGL and TW_OPENGL_CORE were designed to select rendering with 
    // // very old GL specification. Using these modes results in applying some 
    // // odd offsets which distorts everything
    // // Latest OpenGL works very much like Direct3D11, and 
    // // Tweak Bar will never know if D3D or OpenGL is actually used
    // if (!TwInit(TW_DIRECT3D11, pRenderDevice.RawPtr(), pDeviceContext.RawPtr(), pSwapChain->GetDesc().ColorBufferFormat))
    // {
    //     MessageBoxA(wnd, TwGetLastError(), "AntTweakBar initialization failed", MB_OK|MB_ICONERROR);
    //     return 0;
    // }

    // g_pSample.reset( CreateSample(pRenderDevice, pDeviceContext, pSwapChain) );
    // g_pSample->WindowResize( g_pSwapChain->GetDesc().Width, g_pSwapChain->GetDesc().Height );
    std::string Title = "Graphics engine sample";
    // Title = g_pSample->GetSampleName();

    Timer Timer;
    auto PrevTime = Timer.GetElapsedTime();
    double filteredFrameTime = 0.0;

    while (1) {
        auto CurrTime = Timer.GetElapsedTime();
        auto ElapsedTime = CurrTime - PrevTime;
        PrevTime = CurrTime;

        XNextEvent(display, &xev);

        if (xev.type == Expose) {
            // g_pSample->Update(CurrTime, ElapsedTime);
            // g_pSample->Render();

            // // Draw tweak bars
            // // Restore default render target in case the sample has changed it
            // pDeviceContext->SetRenderTargets(0, nullptr, nullptr);
            // TwDraw();
            
            XGetWindowAttributes(display, wnd, &gwa);
                glViewport(0, 0, gwa.width, gwa.height);
                glClearColor(0.2f, 0.5f, 0.6f, 1.f);
                glClear(GL_COLOR_BUFFER_BIT);

            glXSwapBuffers(display, wnd);
            //pSwapChain->Present();
        }
        else if (xev.type == KeyPress) {
            glXMakeCurrent(display, None, NULL);
            glXDestroyContext(display, glc);
            XDestroyWindow(display, wnd);
            XCloseDisplay(display);
            break;
        }

        double filterScale = 0.2;
        filteredFrameTime = filteredFrameTime * (1.0 - filterScale) + filterScale * ElapsedTime;
        std::stringstream fpsCounterSS;
        fpsCounterSS << " - " << std::fixed << std::setprecision(1) << filteredFrameTime * 1000;
        fpsCounterSS << " ms (" << 1.0 / filteredFrameTime << " fps)";
        XStoreName(display, wnd, (Title + fpsCounterSS.str()).c_str());
    }

    g_pSample.reset();
    pSwapChain.Release();
    pDeviceContext.Release();
    pRenderDevice.Release();

    exit(0);
} 

#if 0


LRESULT CALLBACK MessageProc(HWND, UINT, WPARAM, LPARAM);

ISwapChain *g_pSwapChain;

// Main
int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR, int cmdShow)
{
#if defined(_DEBUG) || defined(DEBUG)
    _CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif

    DeviceType DevType = DeviceType::Undefined;
    std::wstring CmdLine = GetCommandLine();
    std::wstring Key = L"mode=";
    auto pos = CmdLine.find( Key );
    if( pos != std::string::npos )
    {
        pos += Key.length();
        auto Val = CmdLine.substr( pos );
        if(Val == L"D3D11")
        {
            DevType = DeviceType::D3D11;
            Title.append( L" (D3D11)" );
        }
        else if(Val == L"D3D12")
        {
            DevType = DeviceType::D3D12;
            Title.append( L" (D3D12)" );
        }
        else if(Val == L"GL")
        {
            DevType = DeviceType::OpenGL;
            Title.append( L" (OpenGL)" );
        }
        else
        {
            LOG_ERROR("Unknown device type. Only the following types are supported: D3D11, D3D12, GL")
            return -1;
        }
    }
    else
    {
        LOG_INFO_MESSAGE("Device type is not specified. Using D3D11 device");
        DevType = DeviceType::D3D11;
        Title.append( L" (D3D11)" );
    }
    // Register our window class
    WNDCLASSEX wcex = { sizeof(WNDCLASSEX), CS_HREDRAW|CS_VREDRAW, MessageProc,
                        0L, 0L, instance, NULL, NULL, NULL, NULL, L"SampleApp", NULL };
    RegisterClassEx(&wcex);

    // Create a window
    RECT rc = { 0, 0, 1280, 1024 };
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND wnd = CreateWindow(L"SampleApp", Title.c_str(), 
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 
                            rc.right-rc.left, rc.bottom-rc.top, NULL, NULL, instance, NULL);
    if (!wnd)
    {
        MessageBox(NULL, L"Cannot create window", L"Error", MB_OK|MB_ICONERROR);
        return 0;
    }
    ShowWindow(wnd, cmdShow);
    UpdateWindow(wnd);


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


            double filterScale = 0.2;
            filteredFrameTime = filteredFrameTime * (1.0 - filterScale) + filterScale * ElapsedTime;
            std::wstringstream fpsCounterSS;
            fpsCounterSS << " - " << std::fixed << std::setprecision(1) << filteredFrameTime * 1000;
            fpsCounterSS << " ms (" << 1.0 / filteredFrameTime << " fps)";
            SetWindowText(wnd, (Title + fpsCounterSS.str()).c_str());
        }
    }
    
    TwTerminate();

	//pDeviceContext->Flush();

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
        {
            struct WindowMessageData
            {
                HWND hWnd;
                UINT message;
                WPARAM wParam;
                LPARAM lParam;
            }msg{wnd, message, wParam, lParam};
            if (g_pSample != nullptr && g_pSample->HandleNativeMessage(&msg) )
                return 0;
            else
                return DefWindowProc(wnd, message, wParam, lParam);
        }
    }
}
#endif