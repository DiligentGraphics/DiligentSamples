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

#pragma once 

#include <vector>
#include <string>

#include "NativeAppBase.h"
#include "RefCntAutoPtr.h"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "SampleBase.h"

class SampleApp : public NativeAppBase
{
public:
    SampleApp();
    ~SampleApp();
    virtual void ProcessCommandLine(const char *CmdLine)override final;
    virtual const char* GetAppTitle()const override final { return m_AppTitle.c_str(); }
    virtual void Update(double CurrTime, double ElapsedTime)override final;
    virtual void WindowResize(int width, int height)override final;
    virtual void Render()override;
    virtual void Present()override;

protected:
    void InitializeDiligentEngine(
#if PLATFORM_LINUX
        void *display,
#endif
        void *NativeWindowHandle
    );
    void InitializeSample();

    virtual void SetFullscreenMode(const Diligent::DisplayModeAttribs &DisplayMode)
    { 
        m_bFullScreenMode = true;
        m_pSwapChain->SetFullscreenMode(DisplayMode); 
    }
    virtual void SetWindowedMode()
    { 
        m_bFullScreenMode = false;
        m_pSwapChain->SetWindowedMode(); 
    }

    Diligent::DeviceType m_DeviceType = Diligent::DeviceType::Undefined;
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> m_pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_pImmediateContext;
    std::vector<Diligent::RefCntAutoPtr<Diligent::IDeviceContext> > m_pDeferredContexts;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain> m_pSwapChain;
    Diligent::HardwareAdapterAttribs m_AdapterAttribs;
    std::vector<Diligent::DisplayModeAttribs> m_DisplayModes;

    std::unique_ptr<SampleBase> m_TheSample;
    std::string m_AppTitle;
    Diligent::Int32 m_UIScale = 1;
    std::string m_AdapterDetailsString;
    int m_SelectedDisplayMode = 0;
    bool m_bVSync = false;
    bool m_bFullScreenMode = false;
};
