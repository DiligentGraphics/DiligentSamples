/*     Copyright 2015-2019 Egor Yusov
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

#include "EngineFactory.h"
#include "RefCntAutoPtr.h"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"

namespace Diligent
{

class SampleBase
{
public:
    virtual ~SampleBase(){}

    virtual void GetEngineInitializationAttribs(DeviceType DevType, EngineCreateInfo& EngineCI);

    virtual void Initialize(IEngineFactory*     pEngineFactory,
                            IRenderDevice*      pDevice, 
                            IDeviceContext**    ppContexts, 
                            Uint32              NumDeferredCtx, 
                            ISwapChain*         pSwapChain) = 0;

    virtual void Render() = 0;
    virtual void Update(double CurrTime, double ElapsedTime) = 0;
    virtual void WindowResize(Uint32 Width, Uint32 Height){}
    virtual bool HandleNativeMessage(const void* pNativeMsgData) { return false; }
    virtual const Char* GetSampleName()const{return "Diligent Engine Sample";}
    void SetUIScale(Int32 UIScale){m_UIScale = UIScale;};
    Int32 GetUIScale()const{return m_UIScale;}
    virtual void ProcessCommandLine(const char *CmdLine){}

protected:
    RefCntAutoPtr<IEngineFactory>               m_pEngineFactory;
    RefCntAutoPtr<IRenderDevice>                m_pDevice;
    RefCntAutoPtr<IDeviceContext>               m_pImmediateContext;
    std::vector<RefCntAutoPtr<IDeviceContext> > m_pDeferredContexts;
    RefCntAutoPtr<ISwapChain>                   m_pSwapChain;
    float  m_fFPS                = 0;
    double m_LastFPSTime         = 0;
    Uint32 m_uiNumFramesRendered = 0;
    Int32  m_UIScale             = 1;
};

inline void SampleBase::Update(double CurrTime, double ElapsedTime)
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

inline void SampleBase::Initialize(IEngineFactory*  pEngineFactory,
                                   IRenderDevice*   pDevice, 
                                   IDeviceContext** ppContexts, 
                                   Uint32           NumDeferredCtx, 
                                   ISwapChain*      pSwapChain)
{
    m_pEngineFactory    = pEngineFactory;
    m_pDevice           = pDevice;
    m_pSwapChain        = pSwapChain;
    m_pImmediateContext = ppContexts[0];
    m_pDeferredContexts.resize(NumDeferredCtx);
    for (Uint32 ctx=0; ctx < NumDeferredCtx; ++ctx)
        m_pDeferredContexts[ctx] = ppContexts[1+ctx];
}

extern SampleBase* CreateSample();

}
