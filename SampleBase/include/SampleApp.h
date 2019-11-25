/*     Copyright 2019 Diligent Graphics LLC
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

#pragma once

#include <vector>
#include <string>
#include <memory>

#include "NativeAppBase.h"
#include "RefCntAutoPtr.h"
#include "EngineFactory.h"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "SampleBase.h"
#include "ScreenCapture.h"
#include "Image.h"

namespace Diligent
{

class ImGuiImplDiligent;

class SampleApp : public NativeAppBase
{
public:
    SampleApp();
    ~SampleApp();
    virtual void        ProcessCommandLine(const char* CmdLine) override final;
    virtual const char* GetAppTitle() const override final { return m_AppTitle.c_str(); }
    virtual void        Update(double CurrTime, double ElapsedTime) override;
    virtual void        WindowResize(int width, int height) override;
    virtual void        Render() override;
    virtual void        Present() override;
    virtual void        SelectDeviceType(){};

    virtual void GetDesiredInitialWindowSize(int& width, int& height) override final
    {
        width  = m_InitialWindowWidth;
        height = m_InitialWindowHeight;
    }

protected:
    void InitializeDiligentEngine(
#if PLATFORM_LINUX
        void* display,
#endif
        void* NativeWindowHandle);
    void InitializeSample();
    void UpdateAdaptersDialog();

    virtual void SetFullscreenMode(const DisplayModeAttribs& DisplayMode)
    {
        m_bFullScreenMode = true;
        m_pSwapChain->SetFullscreenMode(DisplayMode);
    }
    virtual void SetWindowedMode()
    {
        m_bFullScreenMode = false;
        m_pSwapChain->SetWindowedMode();
    }

    DeviceType                                 m_DeviceType = DeviceType::Undefined;
    RefCntAutoPtr<IEngineFactory>              m_pEngineFactory;
    RefCntAutoPtr<IRenderDevice>               m_pDevice;
    RefCntAutoPtr<IDeviceContext>              m_pImmediateContext;
    std::vector<RefCntAutoPtr<IDeviceContext>> m_pDeferredContexts;
    RefCntAutoPtr<ISwapChain>                  m_pSwapChain;
    HardwareAdapterAttribs                     m_AdapterAttribs;
    std::vector<DisplayModeAttribs>            m_DisplayModes;

    std::unique_ptr<SampleBase> m_TheSample;

    int         m_InitialWindowWidth  = 0;
    int         m_InitialWindowHeight = 0;
    int         m_ValidationLevel     = -1;
    std::string m_AppTitle;
    Uint32      m_AdapterId = 0;
    std::string m_AdapterDetailsString;
    int         m_SelectedDisplayMode = 0;
    bool        m_bVSync              = false;
    bool        m_bFullScreenMode     = false;
    bool        m_bShowAdaptersDialog = true;
    double      m_CurrentTime         = 0;

    struct ScreenCaptureInfo
    {
        bool             AllowCapture = false;
        std::string      Directory;
        std::string      FileName        = "frame";
        double           CaptureFPS      = 30;
        double           LastCaptureTime = 0;
        Uint32           FramesToCapture = 0;
        Uint32           CurrentFrame    = 0;
        EImageFileFormat FileFormat      = EImageFileFormat::png;
        int              JpegQuality     = 95;
        bool             KeepAlpha       = false;

    } m_ScreenCaptureInfo;
    std::unique_ptr<ScreenCapture> m_pScreenCapture;

    std::unique_ptr<ImGuiImplDiligent> m_pImGui;
};

} // namespace Diligent
