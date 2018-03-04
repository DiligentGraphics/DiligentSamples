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

#include <sstream>

#include "PlatformDefinitions.h"
#include "SampleApp.h"
#include "Errors.h"
#include "StringTools.h"

#if D3D11_SUPPORTED
#   include "RenderDeviceFactoryD3D11.h"
#endif

#if D3D12_SUPPORTED
#   include "RenderDeviceFactoryD3D12.h"
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
#   include "RenderDeviceFactoryOpenGL.h"
#endif

#include "AntTweakBar.h"

using namespace Diligent;

SampleApp::SampleApp() :
    m_TheSample(CreateSample()),
    m_AppTitle(m_TheSample->GetSampleName())
{
}

SampleApp::~SampleApp()
{
    TwTerminate();
    m_TheSample.reset();

    //pDeviceContext->Flush();
    m_pDeferredContexts.clear();
    m_pImmediateContext.Release();
    m_pSwapChain.Release();
    m_pDevice.Release();
}


void SampleApp::InitializeDiligentEngine(
#if PLATFORM_LINUX
        void *display,
#endif
    void *NativeWindowHandle
    )
{
    SwapChainDesc SCDesc;
    SCDesc.SamplesCount = 1;
    Uint32 NumDeferredCtx = 0;
    std::vector<IDeviceContext*> ppContexts;
    switch (m_DeviceType)
    {
#if D3D11_SUPPORTED
        case DeviceType::D3D11:
        {
            EngineD3D11Attribs DeviceAttribs;
            m_TheSample->GetEngineInitializationAttribs(m_DeviceType, DeviceAttribs, NumDeferredCtx);

#if ENGINE_DLL
            GetEngineFactoryD3D11Type GetEngineFactoryD3D11 = nullptr;
            // Load the dll and import GetEngineFactoryD3D11() function
            LoadGraphicsEngineD3D11(GetEngineFactoryD3D11);
#endif
            ppContexts.resize(1 + NumDeferredCtx);
            auto *pFactoryD3D11 = GetEngineFactoryD3D11();
            pFactoryD3D11->CreateDeviceAndContextsD3D11(DeviceAttribs, &m_pDevice, ppContexts.data(), NumDeferredCtx);

            if(NativeWindowHandle != nullptr)
                pFactoryD3D11->CreateSwapChainD3D11(m_pDevice, ppContexts[0], SCDesc, NativeWindowHandle, &m_pSwapChain);
        }
        break;
#endif

#if D3D12_SUPPORTED
        case DeviceType::D3D12:
        {
#if ENGINE_DLL
            GetEngineFactoryD3D12Type GetEngineFactoryD3D12 = nullptr;
            // Load the dll and import GetEngineFactoryD3D12() function
            LoadGraphicsEngineD3D12(GetEngineFactoryD3D12);
#endif
            EngineD3D12Attribs EngD3D12Attribs;
            m_TheSample->GetEngineInitializationAttribs(m_DeviceType, EngD3D12Attribs, NumDeferredCtx);
            ppContexts.resize(1 + NumDeferredCtx);
            auto *pFactoryD3D12 = GetEngineFactoryD3D12();
            pFactoryD3D12->CreateDeviceAndContextsD3D12(EngD3D12Attribs, &m_pDevice, ppContexts.data(), NumDeferredCtx);

            if (!m_pSwapChain && NativeWindowHandle != nullptr)
                pFactoryD3D12->CreateSwapChainD3D12(m_pDevice, ppContexts[0], SCDesc, NativeWindowHandle, &m_pSwapChain);
        }
        break;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
        case DeviceType::OpenGL:
        case DeviceType::OpenGLES:
        {
#if !PLATFORM_MACOS
            VERIFY_EXPR(NativeWindowHandle != nullptr);
#endif
#if ENGINE_DLL && (PLATFORM_WIN32 || PLATFORM_UNIVERSAL_WINDOWS)
            // Declare function pointer
            GetEngineFactoryOpenGLType GetEngineFactoryOpenGL = nullptr;
            // Load the dll and import GetEngineFactoryOpenGL() function
            LoadGraphicsEngineOpenGL(GetEngineFactoryOpenGL);
#endif
            auto *pFactoryOpenGL = GetEngineFactoryOpenGL();
            EngineGLAttribs CreationAttribs;
            CreationAttribs.pNativeWndHandle = NativeWindowHandle;
#if PLATFORM_LINUX
            CreationAttribs.pDisplay = display;
#endif
            m_TheSample->GetEngineInitializationAttribs(m_DeviceType, CreationAttribs, NumDeferredCtx);
            if (NumDeferredCtx != 0)
            {
                LOG_ERROR_MESSAGE("Deferred contexts are not supported in OpenGL mode");
                NumDeferredCtx = 0;
            }
            ppContexts.resize(1 + NumDeferredCtx);
            pFactoryOpenGL->CreateDeviceAndSwapChainGL(
                CreationAttribs, &m_pDevice, ppContexts.data(), SCDesc, &m_pSwapChain);
        }
        break;
#endif

        default:
            LOG_ERROR_AND_THROW("Unknown device type");
            break;
    }

    m_pImmediateContext.Attach(ppContexts[0]);
    m_pDeferredContexts.resize(NumDeferredCtx);
    for (Diligent::Uint32 ctx = 0; ctx < NumDeferredCtx; ++ctx)
        m_pDeferredContexts[ctx].Attach(ppContexts[1 + ctx]);
}

void SampleApp::InitializeSample()
{
    auto UIScale = m_TheSample->GetUIScale();
    if(UIScale != 1)
    {
        std::stringstream fontscaling;
        fontscaling << " GLOBAL fontscaling=" << UIScale;
        TwDefine(fontscaling.str().c_str());
    }

    // Initialize AntTweakBar
    // TW_OPENGL and TW_OPENGL_CORE were designed to select rendering with 
    // very old GL specification. Using these modes results in applying some 
    // odd offsets which distorts everything
    // Latest OpenGL works very much like Direct3D11, and 
    // Tweak Bar will never know if D3D or OpenGL is actually used
    const auto& SCDesc = m_pSwapChain->GetDesc();
    if (!TwInit(TW_DIRECT3D11, m_pDevice.RawPtr(), m_pImmediateContext.RawPtr(), SCDesc.ColorBufferFormat))
    {
        LOG_ERROR_MESSAGE("AntTweakBar initialization failed");
    }
    TwDefine(" TW_HELP visible=false ");

    std::vector<IDeviceContext*> ppContexts(1 + m_pDeferredContexts.size());
    ppContexts[0] = m_pImmediateContext;
    Uint32 NumDeferredCtx = static_cast<Uint32>(m_pDeferredContexts.size());
    for (size_t ctx = 0; ctx < m_pDeferredContexts.size(); ++ctx)
        ppContexts[1 + ctx] = m_pDeferredContexts[ctx];
    m_TheSample->Initialize(m_pDevice, ppContexts.data(), NumDeferredCtx, m_pSwapChain);

    m_TheSample->WindowResize(SCDesc.Width, SCDesc.Height);
    TwWindowSize(SCDesc.Width, SCDesc.Height);
}

void SampleApp::ProcessCommandLine(const char *CmdLine)
{
    const auto* Key = "mode=";
    const auto *pos = strstr(CmdLine, Key);
    if (pos != nullptr)
    {
        pos += strlen(Key);
        if (_stricmp(pos, "D3D11") == 0)
        {
            m_DeviceType = DeviceType::D3D11;
        }
        else if (_stricmp(pos, "D3D12") == 0)
        {
            m_DeviceType = DeviceType::D3D12;
        }
        else if (_stricmp(pos, "GL") == 0)
        {
            m_DeviceType = DeviceType::OpenGL;
        }
        else
        {
            LOG_ERROR_AND_THROW("Unknown device type. Only the following types are supported: D3D11, D3D12, GL");
        }
    }
    else
    {
        LOG_INFO_MESSAGE("Device type is not specified. Using D3D11 device");
        m_DeviceType = DeviceType::D3D11;
    }

    switch (m_DeviceType)
    {
        case DeviceType::D3D11: m_AppTitle.append(" (D3D11)"); break;
        case DeviceType::D3D12: m_AppTitle.append(" (D3D12)"); break;
        case DeviceType::OpenGL: m_AppTitle.append(" (OpenGL)"); break;
        default: UNEXPECTED("Unknown device type");
    }
}

void SampleApp::WindowResize(int width, int height)
{
    if (m_pSwapChain)
    {
        m_pSwapChain->Resize(width, height);
        auto SCWidth = m_pSwapChain->GetDesc().Width;
        auto SCHeight = m_pSwapChain->GetDesc().Height;
        m_TheSample->WindowResize(SCWidth, SCHeight);
        TwWindowSize(SCWidth, SCHeight);
    }
}

void SampleApp::Update(double CurrTime, double ElapsedTime)
{
    m_TheSample->Update(CurrTime, ElapsedTime);
}

void SampleApp::Render()
{
    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr);
    m_TheSample->Render();

    // Draw tweak bars
    // Restore default render target in case the sample has changed it
    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr);
    TwDraw();
}

void SampleApp::Present()
{
    m_pSwapChain->Present();
}
