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

#include "Renderer.h"
#include "AntTweakBar.h"
#include "Errors.h"
#include "RenderDeviceFactoryOpenGL.h"

using namespace Diligent;

Renderer::Renderer()
{
    pSample.reset( CreateSample() );
}

Renderer::~Renderer()
{
    pSample.reset();
    TwTerminate();
    pSwapChain.Release();
    pDeviceContext.Release();
    pRenderDevice.Release();
}

void Renderer::Init(
#if PLATFORM_IOS
                    void  *layer
#endif
)
{
    SwapChainDesc SCDesc;
    EngineGLAttribs CreationAttribs;
#if PLATFORM_IOS
    CreationAttribs.pNativeWndHandle = layer;
#endif

    Uint32 NumDeferredContexts = 0;
    pSample->GetEngineInitializationAttribs(DeviceType::OpenGL, CreationAttribs, NumDeferredContexts);
    if(NumDeferredContexts != 0)
    {
        LOG_ERROR_MESSAGE("Deferred contexts are not supported by OpenGL implementation");
        NumDeferredContexts = 0;
    }

    // On MacOS, we attach to active GL context initialized by the application
    GetEngineFactoryOpenGL()->CreateDeviceAndSwapChainGL(CreationAttribs, &pRenderDevice, &pDeviceContext, SCDesc, &pSwapChain );

#if PLATFORM_MACOS
    // Set font scaling
    TwDefine(" GLOBAL fontscaling=2");
    pSample->SetUIScale(2);
#endif
    // Initialize AntTweakBar
    // TW_OPENGL and TW_OPENGL_CORE were designed to select rendering with
    // very old GL specification. Using these modes results in applying some
    // odd offsets which distorts everything
    // Latest OpenGL works very much like Direct3D11, and
    // Tweak Bar will never know if D3D or OpenGL is actually used
    if (!TwInit(TW_DIRECT3D11, pRenderDevice.RawPtr(), pDeviceContext.RawPtr(), pSwapChain->GetDesc().ColorBufferFormat))
    {
        LOG_ERROR_MESSAGE("AntTweakBar initialization failed");
    }
    TwDefine(" TW_HELP visible=false ");
    
    auto width = pSwapChain->GetDesc().Width;
    auto height = pSwapChain->GetDesc().Height;
    IDeviceContext *ppContexts[] = {pDeviceContext};
    pSample->Initialize(pRenderDevice, ppContexts, NumDeferredContexts, pSwapChain);
    pSample->WindowResize( width, height );
    std::string Title = pSample->GetSampleName();
    TwWindowSize(width, height);
    
    PrevTime = timer.GetElapsedTime();
}

void Renderer::WindowResize(int width, int height)
{
    pSwapChain->Resize(width, height);
    // On iOS, width and height are zeroes
    const auto& SCDesc = pSwapChain->GetDesc();
    pSample->WindowResize( SCDesc.Width, SCDesc.Height );
    TwWindowSize(SCDesc.Width, SCDesc.Height);
}

void Renderer::Render()
{
    // Render the scene
    auto CurrTime = timer.GetElapsedTime();
    auto ElapsedTime = CurrTime - PrevTime;
    PrevTime = CurrTime;
    
    pDeviceContext->SetRenderTargets(0, nullptr, nullptr);
    
    pSample->Update(CurrTime, ElapsedTime);
    pSample->Render();
    
    // Draw tweak bars
    // Restore default render target in case the sample has changed it
    pDeviceContext->SetRenderTargets(0, nullptr, nullptr);

    // Handle all TwBar events here as the event handlers call draw commands
    // and thus cannot be used in the UI thread
    while(!TwBarEvents.empty())
    {
        const auto& event = TwBarEvents.front();
        switch (event.type)
        {
            case TwEvent::LMB_PRESSED:
            case TwEvent::RMB_PRESSED:
                TwMouseButton(TW_MOUSE_PRESSED, event.type == TwEvent::LMB_PRESSED ? TW_MOUSE_LEFT : TW_MOUSE_RIGHT);
                break;

            case TwEvent::LMB_RELEASED:
            case TwEvent::RMB_RELEASED:
                TwMouseButton(TW_MOUSE_RELEASED, event.type == TwEvent::LMB_RELEASED ? TW_MOUSE_LEFT : TW_MOUSE_RIGHT);
                break;

            case TwEvent::MOUSE_MOVE:
                TwMouseMotion(event.mouseX, event.mouseY);
                break;

            case TwEvent::KEY_PRESSED:
                TwKeyPressed(event.key, 0);
                break;
        }
        TwBarEvents.pop();
    }
    TwDraw();
    
    // On MacOS, present is performed by the app
#if PLATFORM_IOS
    pSwapChain->Present();
#endif
}

void Renderer::OnMouseDown(int button)
{
    TwBarEvents.emplace(button == 1 ? TwEvent::LMB_PRESSED : TwEvent::RMB_PRESSED);
}

void Renderer::OnMouseUp(int button)
{
    TwBarEvents.emplace(button == 1 ? TwEvent::LMB_RELEASED : TwEvent::RMB_RELEASED);
}

void Renderer::OnMouseMove(int x, int y)
{
    TwBarEvents.emplace(x, y);
}

void Renderer::OnKeyPressed(int key)
{
    TwBarEvents.emplace(key);
}

