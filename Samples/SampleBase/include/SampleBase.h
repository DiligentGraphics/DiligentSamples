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

#include "RefCntAutoPtr.h"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"

class SampleBase
{
public:
    virtual ~SampleBase(){}

    virtual void GetEngineInitializationAttribs(Diligent::DeviceType DevType, Diligent::EngineCreationAttribs &Attribs, Diligent::Uint32 &NumDeferredContexts);

    virtual void Initialize(Diligent::IRenderDevice *pDevice, 
                            Diligent::IDeviceContext **ppContexts, 
                            Diligent::Uint32 NumDeferredCtx, 
                            Diligent::ISwapChain *pSwapChain) = 0;

    virtual void Render() = 0;
    virtual void Update(double CurrTime, double ElapsedTime) = 0;
    virtual void WindowResize(Diligent::Uint32 Width, Diligent::Uint32 Height){}
    virtual bool HandleNativeMessage(const void *pNativeMsgData) { return false; }
    virtual const Diligent::Char* GetSampleName()const{return "Diligent Engine Sample";}
    void SetUIScale(Diligent::Int32 UIScale){m_UIScale = UIScale;};

protected:
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> m_pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_pImmediateContext;
    std::vector<Diligent::RefCntAutoPtr<Diligent::IDeviceContext> > m_pDeferredContexts;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain> m_pSwapChain;
    float m_fFPS = 0;
    double m_LastFPSTime = 0;
    Diligent::Uint32 m_uiNumFramesRendered = 0;
    Diligent::Int32 m_UIScale = 1;
};

inline void SampleBase::Update( double CurrTime, double ElapsedTime )
{
    ++m_uiNumFramesRendered;
    static const double dFPSInterval = 0.5;
    if( CurrTime - m_LastFPSTime > dFPSInterval )
    {
        m_fFPS = static_cast<float>(m_uiNumFramesRendered / (CurrTime - m_LastFPSTime));
        m_uiNumFramesRendered = 0;
        m_LastFPSTime = CurrTime;
    }
}

inline void SampleBase::Initialize(Diligent::IRenderDevice *pDevice, 
                                   Diligent::IDeviceContext **ppContexts, 
                                   Diligent::Uint32 NumDeferredCtx, 
                                   Diligent::ISwapChain *pSwapChain)
{
    m_pDevice = pDevice;
    m_pSwapChain = pSwapChain;
    m_pImmediateContext = ppContexts[0];
    m_pDeferredContexts.resize(NumDeferredCtx);
    for(Diligent::Uint32 ctx=0; ctx < NumDeferredCtx; ++ctx)
        m_pDeferredContexts[ctx] = ppContexts[1+ctx];
}

extern SampleBase* CreateSample();
