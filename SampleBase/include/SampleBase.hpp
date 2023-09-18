/*
 *  Copyright 2019-2023 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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
#include "RefCntAutoPtr.hpp"
#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"
#include "InputController.hpp"
#include "BasicMath.hpp"
#include "AppBase.hpp"
#include "FlagEnum.h"

namespace Diligent
{

class ImGuiImplDiligent;

struct SampleInitInfo
{
    IEngineFactory*    pEngineFactory  = nullptr;
    IRenderDevice*     pDevice         = nullptr;
    IDeviceContext**   ppContexts      = nullptr;
    Uint32             NumImmediateCtx = 1;
    Uint32             NumDeferredCtx  = 0;
    ISwapChain*        pSwapChain      = nullptr;
    ImGuiImplDiligent* pImGui          = nullptr;
};

struct DesiredApplicationSettings
{
    enum SETTING_FLAGS : Uint8
    {
        SETTING_FLAG_NONE                 = 0,
        SETTING_FLAG_VSYNC                = 1u << 0u,
        SETTING_FLAG_SHOW_UI              = 1u << 1u,
        SETTING_FLAG_SHOW_ADAPTERS_DIALOG = 1u << 2u,
        SETTING_FLAG_ADAPTER_ID           = 1u << 3u,
        SETTING_FLAG_DEVICE_TYPE          = 1u << 4u,
        SETTING_FLAG_ADAPTER_TYPE         = 1u << 5u,
        SETTING_FLAG_WINDOW_WIDTH         = 1u << 6u,
        SETTING_FLAG_WINDOW_HEIGHT        = 1u << 7u,
    };
    SETTING_FLAGS Flags = SETTING_FLAG_NONE;

    bool VSync              = false;
    bool ShowUI             = false;
    bool ShowAdaptersDialog = false;

    Uint32             AdapterId   = DEFAULT_ADAPTER_ID;
    ADAPTER_TYPE       AdapterType = ADAPTER_TYPE_UNKNOWN;
    RENDER_DEVICE_TYPE DeviceType  = RENDER_DEVICE_TYPE_UNDEFINED;

    int WindowWidth  = 0;
    int WindowHeight = 0;

    inline DesiredApplicationSettings& SetVSync(bool _VSync);
    inline DesiredApplicationSettings& SetShowUI(bool _ShowUI);
    inline DesiredApplicationSettings& SetShowAdaptersDialog(bool _ShowAdaptersDialog);
    inline DesiredApplicationSettings& SetAdapterId(Uint32 _AdapterId);
    inline DesiredApplicationSettings& SetAdapterType(ADAPTER_TYPE _AdapterType);
    inline DesiredApplicationSettings& SetDeviceType(RENDER_DEVICE_TYPE _DeviceType);
    inline DesiredApplicationSettings& SetWindowWidth(int _WindowWidth);
    inline DesiredApplicationSettings& SetWindowHeight(int _WindowHeight);
};
DEFINE_FLAG_ENUM_OPERATORS(DesiredApplicationSettings::SETTING_FLAGS);

DesiredApplicationSettings& DesiredApplicationSettings::SetVSync(bool _VSync)
{
    VSync = _VSync;
    Flags |= SETTING_FLAG_VSYNC;
    return *this;
}

DesiredApplicationSettings& DesiredApplicationSettings::SetShowUI(bool _ShowUI)
{
    ShowUI = _ShowUI;
    Flags |= SETTING_FLAG_SHOW_UI;
    return *this;
}

DesiredApplicationSettings& DesiredApplicationSettings::SetShowAdaptersDialog(bool _ShowAdaptersDialog)
{
    ShowAdaptersDialog = _ShowAdaptersDialog;
    Flags |= SETTING_FLAG_SHOW_ADAPTERS_DIALOG;
    return *this;
}

DesiredApplicationSettings& DesiredApplicationSettings::SetAdapterId(Uint32 _AdapterId)
{
    AdapterId = _AdapterId;
    Flags |= SETTING_FLAG_ADAPTER_ID;
    return *this;
}

DesiredApplicationSettings& DesiredApplicationSettings::SetAdapterType(ADAPTER_TYPE _AdapterType)
{
    AdapterType = _AdapterType;
    Flags |= SETTING_FLAG_ADAPTER_TYPE;
    return *this;
}

DesiredApplicationSettings& DesiredApplicationSettings::SetDeviceType(RENDER_DEVICE_TYPE _DeviceType)
{
    DeviceType = _DeviceType;
    Flags |= SETTING_FLAG_DEVICE_TYPE;
    return *this;
}

DesiredApplicationSettings& DesiredApplicationSettings::SetWindowWidth(int _WindowWidth)
{
    WindowWidth = _WindowWidth;
    Flags |= SETTING_FLAG_WINDOW_WIDTH;
    return *this;
}

DesiredApplicationSettings& DesiredApplicationSettings::SetWindowHeight(int _WindowHeight)
{
    WindowHeight = _WindowHeight;
    Flags |= SETTING_FLAG_WINDOW_HEIGHT;
    return *this;
}


class SampleBase
{
public:
    virtual ~SampleBase() {}

    virtual DesiredApplicationSettings GetDesiredApplicationSettings(bool IsInitialization) { return DesiredApplicationSettings{}; }

    struct ModifyEngineInitInfoAttribs
    {
        IEngineFactory* const    pFactory;
        const RENDER_DEVICE_TYPE DeviceType;

        EngineCreateInfo& EngineCI;
        SwapChainDesc&    SCDesc;
    };
    virtual void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs);

    virtual void Initialize(const SampleInitInfo& InitInfo) = 0;

    virtual void Render()                                    = 0;
    virtual void Update(double CurrTime, double ElapsedTime) = 0;
    virtual void PreWindowResize() {}
    virtual void WindowResize(Uint32 Width, Uint32 Height) {}
    virtual bool HandleNativeMessage(const void* pNativeMsgData) { return false; }

    virtual const Char* GetSampleName() const { return "Diligent Engine Sample"; }

    using CommandLineStatus = AppBase::CommandLineStatus;
    virtual CommandLineStatus ProcessCommandLine(int argc, const char* const* argv) { return CommandLineStatus::OK; }

    InputController& GetInputController()
    {
        return m_InputController;
    }

    void ResetSwapChain(ISwapChain* pNewSwapChain)
    {
        m_pSwapChain = pNewSwapChain;
    }

protected:
    // Returns projection matrix adjusted to the current screen orientation
    float4x4 GetAdjustedProjectionMatrix(float FOV, float NearPlane, float FarPlane) const;

    // Returns pretransform matrix that matches the current screen rotation
    float4x4 GetSurfacePretransformMatrix(const float3& f3CameraViewAxis) const;

    RefCntAutoPtr<IEngineFactory>              m_pEngineFactory;
    RefCntAutoPtr<IRenderDevice>               m_pDevice;
    RefCntAutoPtr<IDeviceContext>              m_pImmediateContext;
    std::vector<RefCntAutoPtr<IDeviceContext>> m_pDeferredContexts;
    RefCntAutoPtr<ISwapChain>                  m_pSwapChain;
    ImGuiImplDiligent*                         m_pImGui = nullptr;

    float  m_fSmoothFPS         = 0;
    double m_LastFPSTime        = 0;
    Uint32 m_NumFramesRendered  = 0;
    Uint32 m_CurrentFrameNumber = 0;

    // Pixel shader output needs to be manually converted to gamma space
    bool m_ConvertPSOutputToGamma = false;

    InputController m_InputController;
};

inline void SampleBase::Update(double CurrTime, double ElapsedTime)
{
    ++m_NumFramesRendered;
    ++m_CurrentFrameNumber;
    static const double dFPSInterval = 0.5;
    if (CurrTime - m_LastFPSTime > dFPSInterval)
    {
        m_fSmoothFPS        = static_cast<float>(m_NumFramesRendered / (CurrTime - m_LastFPSTime));
        m_NumFramesRendered = 0;
        m_LastFPSTime       = CurrTime;
    }
}

extern SampleBase* CreateSample();

} // namespace Diligent
