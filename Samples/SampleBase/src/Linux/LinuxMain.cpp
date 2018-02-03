/*     Copyright 2015-2018 Egor Yusov
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

#include <GL/glx.h>
#include <GL/gl.h>

#include "AntTweakBar.h"
 
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

#include "Errors.h"

using namespace Diligent;

#ifndef GLX_CONTEXT_MAJOR_VERSION_ARB
#   define GLX_CONTEXT_MAJOR_VERSION_ARB       0x2091
#endif

#ifndef GLX_CONTEXT_MINOR_VERSION_ARB
#   define GLX_CONTEXT_MINOR_VERSION_ARB       0x2092
#endif

#ifndef GLX_CONTEXT_FLAGS_ARB
#   define GLX_CONTEXT_FLAGS_ARB               0x2094
#endif

#ifndef GLX_CONTEXT_DEBUG_BIT_ARB
#   define GLX_CONTEXT_DEBUG_BIT_ARB           0x0001
#endif

typedef GLXContext (*glXCreateContextAttribsARBProc)(Display*, GLXFBConfig, GLXContext, Bool, const int*);
 
int main (int argc, char ** argv)
{
    std::unique_ptr<SampleBase> pSample( CreateSample() );

    Display *display = XOpenDisplay(0);
  
    static int visual_attribs[] =
    {
        GLX_RENDER_TYPE,    GLX_RGBA_BIT,
        GLX_DRAWABLE_TYPE,  GLX_WINDOW_BIT,
        GLX_DOUBLEBUFFER,   true,

        // The largest available total RGBA color buffer size (sum of GLX_RED_SIZE, 
        // GLX_GREEN_SIZE, GLX_BLUE_SIZE, and GLX_ALPHA_SIZE) of at least the minimum
        // size specified for each color component is preferred.
        GLX_RED_SIZE,       8,
        GLX_GREEN_SIZE,     8,
        GLX_BLUE_SIZE,      8,
        GLX_ALPHA_SIZE,     8,

        // The largest available depth buffer of at least GLX_DEPTH_SIZE size is preferred
        GLX_DEPTH_SIZE,     24,

        //GLX_SAMPLE_BUFFERS, 1,
        GLX_SAMPLES, 1,
        None
     };
 
    int fbcount = 0;
    GLXFBConfig *fbc = glXChooseFBConfig(display, DefaultScreen(display), visual_attribs, &fbcount);
    if (!fbc)
    {
        LOG_ERROR_MESSAGE("Failed to retrieve a framebuffer config");
        return -1;
    }
 
    XVisualInfo *vi = glXGetVisualFromFBConfig(display, fbc[0]);
 
    XSetWindowAttributes swa;
    swa.colormap = XCreateColormap(display, RootWindow(display, vi->screen), vi->visual, AllocNone);
    swa.border_pixel = 0;
    swa.event_mask = 
        StructureNotifyMask |  
        ExposureMask |  
        KeyPressMask | 
        KeyReleaseMask |
        ButtonPressMask | 
        ButtonReleaseMask | 
        PointerMotionMask;
 
    Window win = XCreateWindow(display, RootWindow(display, vi->screen), 0, 0, 1024, 768, 0, vi->depth, InputOutput, vi->visual, CWBorderPixel|CWColormap|CWEventMask, &swa);
    if (!win)
    {
        LOG_ERROR_MESSAGE("Failed to create window.");
        return -1;
    }
 
    XMapWindow(display, win);
 
    glXCreateContextAttribsARBProc glXCreateContextAttribsARB = nullptr;
    {
        // Create an oldstyle context first, to get the correct function pointer for glXCreateContextAttribsARB
        GLXContext ctx_old = glXCreateContext(display, vi, 0, GL_TRUE);
        glXCreateContextAttribsARB =  (glXCreateContextAttribsARBProc)glXGetProcAddress((const GLubyte*)"glXCreateContextAttribsARB");
        glXMakeCurrent(display, None, NULL);
        glXDestroyContext(display, ctx_old);
    }
 
    if (glXCreateContextAttribsARB == nullptr)
    {
        LOG_ERROR("glXCreateContextAttribsARB entry point not found. Aborting.");
        return -1;
    }
 
    int Flags = GLX_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB;
#ifdef _DEBUG
     Flags |= GLX_CONTEXT_DEBUG_BIT_ARB;
#endif 
    
    static int context_attribs[] =
    {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 3,
        GLX_CONTEXT_FLAGS_ARB, Flags,
        None
    };
  
    GLXContext ctx = glXCreateContextAttribsARB(display, fbc[0], NULL, true, context_attribs);
    if (!ctx)
    {
        LOG_ERROR("Failed to create GL context.");
        return -1;
    }
    XFree(fbc);
    
    glXMakeCurrent(display, win, ctx);
 
    RefCntAutoPtr<IRenderDevice> pRenderDevice;
    RefCntAutoPtr<IDeviceContext> pDeviceContext;
    Diligent::RefCntAutoPtr<ISwapChain> pSwapChain;
    
    SwapChainDesc SCDesc;
    EngineGLAttribs EngineGLAttribs;
    Uint32 NumDeferredContexts = 0;
    pSample->GetEngineInitializationAttribs(DeviceType::OpenGL, EngineGLAttribs, NumDeferredContexts);
    if(NumDeferredContexts != 0)
    {
        LOG_ERROR_MESSAGE("Deferred contexts are not supported by OpenGL implementation");
        NumDeferredContexts = 0;
    }

    EngineGLAttribs.pNativeWndHandle = reinterpret_cast<void*>(static_cast<size_t>(win));
    EngineGLAttribs.pDisplay = display;
    GetEngineFactoryOpenGL()->CreateDeviceAndSwapChainGL(
        EngineGLAttribs, &pRenderDevice, &pDeviceContext, SCDesc, &pSwapChain );

    // Initialize AntTweakBar
    // TW_OPENGL and TW_OPENGL_CORE were designed to select rendering with 
    // very old GL specification. Using these modes results in applying some 
    // odd offsets which distorts everything
    // Latest OpenGL works very much like Direct3D11, and 
    // Tweak Bar will never know if D3D or OpenGL is actually used
    if (!TwInit(TW_DIRECT3D11, pRenderDevice.RawPtr(), pDeviceContext.RawPtr(), pSwapChain->GetDesc().ColorBufferFormat))
    {
        LOG_ERROR_MESSAGE("AntTweakBar initialization failed");
        return 1;
    }
    TwDefine(" TW_HELP visible=false ");

    IDeviceContext *ppContexts[] = {pDeviceContext};
    pSample->Initialize(pRenderDevice, ppContexts, NumDeferredContexts, pSwapChain);
    pSample->WindowResize( pSwapChain->GetDesc().Width, pSwapChain->GetDesc().Height );
    std::string Title = pSample->GetSampleName(); 
 
    Timer Timer;
    auto PrevTime = Timer.GetElapsedTime();
    double filteredFrameTime = 0.0;
 
    while (true) 
    {
        bool EscPressed = false;
        XEvent xev;
        // Handle all events in the queue
        while(XCheckMaskEvent(display, 0xFFFFFFFF, &xev))
        {
            TwEventX11(&xev);
            switch(xev.type)
            {
                case KeyPress:
                {
                    KeySym keysym;
                    char buffer[80];
                    int num_char = XLookupString((XKeyEvent *)&xev, buffer, _countof(buffer), &keysym, 0);
                    EscPressed = (keysym==XK_Escape);
                }
                
                case ConfigureNotify:
                {
                    XConfigureEvent &xce = reinterpret_cast<XConfigureEvent &>(xev);
                    pSwapChain->Resize(static_cast<Uint32>(xce.width), static_cast<Uint32>(xce.height));
                    pSample->WindowResize( pSwapChain->GetDesc().Width, pSwapChain->GetDesc().Height );
                    break;
                }

                default:
                {
                    if (pSample != nullptr )
                        pSample->HandleNativeMessage(&xev);
                }
            }
        }

        if(EscPressed)
            break;

        // Render the scene
        auto CurrTime = Timer.GetElapsedTime();
        auto ElapsedTime = CurrTime - PrevTime;
        PrevTime = CurrTime;

        pDeviceContext->SetRenderTargets(0, nullptr, nullptr);
        
        pSample->Update(CurrTime, ElapsedTime);
        pSample->Render();

        // Draw tweak bars
        // Restore default render target in case the sample has changed it
        pDeviceContext->SetRenderTargets(0, nullptr, nullptr);
        TwDraw();
        
        pSwapChain->Present();

        double filterScale = 0.2;
        filteredFrameTime = filteredFrameTime * (1.0 - filterScale) + filterScale * ElapsedTime;
        std::stringstream fpsCounterSS;
        fpsCounterSS << " - " << std::fixed << std::setprecision(1) << filteredFrameTime * 1000;
        fpsCounterSS << " ms (" << 1.0 / filteredFrameTime << " fps)";
        XStoreName(display, win, (Title + fpsCounterSS.str()).c_str());
    }
 
    pSample.reset();
    TwTerminate();
    pSwapChain.Release();
    pDeviceContext.Release();
    pRenderDevice.Release();
 
    ctx = glXGetCurrentContext();
    glXMakeCurrent(display, None, NULL);
    glXDestroyContext(display, ctx);
    XDestroyWindow(display, win);
    XCloseDisplay(display);
}
