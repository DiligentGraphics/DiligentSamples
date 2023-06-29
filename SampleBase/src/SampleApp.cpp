/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <cmath>

#include "PlatformDefinitions.h"
#include "SampleApp.hpp"
#include "Errors.hpp"
#include "StringTools.hpp"
#include "MapHelper.hpp"
#include "Image.h"
#include "FileWrapper.hpp"
#include "CommandLineParser.hpp"
#include "GraphicsAccessories.hpp"

#if D3D11_SUPPORTED
#    include "EngineFactoryD3D11.h"
#endif

#if D3D12_SUPPORTED
#    include "EngineFactoryD3D12.h"
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
#    include "EngineFactoryOpenGL.h"
#endif

#if VULKAN_SUPPORTED
#    include "EngineFactoryVk.h"
#endif

#if METAL_SUPPORTED
#    include "EngineFactoryMtl.h"
#endif

#include "imgui.h"
#include "ImGuiImplDiligent.hpp"
#include "ImGuiUtils.hpp"

namespace Diligent
{

SampleApp::SampleApp() :
    m_TheSample{CreateSample()},
    m_AppTitle{m_TheSample->GetSampleName()}
{
    UpdateAppSettings(true);
}

SampleApp::~SampleApp()
{
    m_pImGui.reset();
    m_TheSample.reset();

    if (!m_pDeviceContexts.empty())
    {
        for (Uint32 q = 0; q < m_NumImmediateContexts; ++q)
            m_pDeviceContexts[q]->Flush();
        m_pDeviceContexts.clear();
    }
    m_NumImmediateContexts = 0;
    m_pSwapChain.Release();
    m_pDevice.Release();
}

void SampleApp::UpdateAppSettings(bool IsInitialization)
{
    const auto DesiredSettings = m_TheSample->GetDesiredApplicationSettings(IsInitialization);

    if (IsInitialization)
    {
        if ((DesiredSettings.Flags & DesiredApplicationSettings::SETTING_FLAG_ADAPTER_ID) != 0)
            m_AdapterId = DesiredSettings.AdapterId;

        if ((DesiredSettings.Flags & DesiredApplicationSettings::SETTING_FLAG_ADAPTER_TYPE) != 0)
            m_AdapterType = DesiredSettings.AdapterType;

        if ((DesiredSettings.Flags & DesiredApplicationSettings::SETTING_FLAG_DEVICE_TYPE) != 0)
            m_DeviceType = DesiredSettings.DeviceType;

        if ((DesiredSettings.Flags & DesiredApplicationSettings::SETTING_FLAG_WINDOW_WIDTH) != 0)
            m_InitialWindowWidth = DesiredSettings.WindowWidth;

        if ((DesiredSettings.Flags & DesiredApplicationSettings::SETTING_FLAG_WINDOW_HEIGHT) != 0)
            m_InitialWindowHeight = DesiredSettings.WindowHeight;
    }

    if ((DesiredSettings.Flags & DesiredApplicationSettings::SETTING_FLAG_VSYNC) != 0)
        m_bVSync = DesiredSettings.VSync;

    if ((DesiredSettings.Flags & DesiredApplicationSettings::SETTING_FLAG_SHOW_UI) != 0)
        m_bShowUI = DesiredSettings.ShowUI;

    if ((DesiredSettings.Flags & DesiredApplicationSettings::SETTING_FLAG_SHOW_ADAPTERS_DIALOG) != 0)
        m_bShowAdaptersDialog = DesiredSettings.ShowAdaptersDialog;
}

void SampleApp::InitializeDiligentEngine(const NativeWindow* pWindow)
{
    if (m_ScreenCaptureInfo.AllowCapture)
        m_SwapChainInitDesc.Usage |= SWAP_CHAIN_USAGE_COPY_SOURCE;

#if PLATFORM_MACOS
    // We need at least 3 buffers in Metal to avoid massive
    // performance degradation in full screen mode.
    // https://github.com/KhronosGroup/MoltenVK/issues/808
    m_SwapChainInitDesc.BufferCount = 3;
#endif

    Uint32 NumImmediateContexts = 0;

#if D3D11_SUPPORTED || D3D12_SUPPORTED || VULKAN_SUPPORTED
    auto FindAdapter = [this](auto* pFactory, Version GraphicsAPIVersion, GraphicsAdapterInfo& AdapterAttribs) {
        Uint32 NumAdapters = 0;
        pFactory->EnumerateAdapters(GraphicsAPIVersion, NumAdapters, nullptr);
        std::vector<GraphicsAdapterInfo> Adapters(NumAdapters);
        if (NumAdapters > 0)
            pFactory->EnumerateAdapters(GraphicsAPIVersion, NumAdapters, Adapters.data());
        else
            LOG_ERROR_AND_THROW("Failed to find compatible hardware adapters");

        auto AdapterId = m_AdapterId;
        if (AdapterId != DEFAULT_ADAPTER_ID)
        {
            if (AdapterId < Adapters.size())
            {
                m_AdapterType = Adapters[AdapterId].Type;
            }
            else
            {
                LOG_ERROR_MESSAGE("Adapter ID (", AdapterId, ") is invalid. Only ", Adapters.size(), " compatible ", (Adapters.size() == 1 ? "adapter" : "adapters"), " present in the system");
                AdapterId = DEFAULT_ADAPTER_ID;
            }
        }

        if (AdapterId == DEFAULT_ADAPTER_ID && m_AdapterType != ADAPTER_TYPE_UNKNOWN)
        {
            for (Uint32 i = 0; i < Adapters.size(); ++i)
            {
                if (Adapters[i].Type == m_AdapterType)
                {
                    AdapterId = i;
                    break;
                }
            }
            if (AdapterId == DEFAULT_ADAPTER_ID)
                LOG_WARNING_MESSAGE("Unable to find the requested adapter type. Using default adapter.");
        }

        if (AdapterId == DEFAULT_ADAPTER_ID)
        {
            m_AdapterType = ADAPTER_TYPE_UNKNOWN;
            for (Uint32 i = 0; i < Adapters.size(); ++i)
            {
                const auto& AdapterInfo = Adapters[i];
                const auto  AdapterType = AdapterInfo.Type;
                static_assert((ADAPTER_TYPE_DISCRETE > ADAPTER_TYPE_INTEGRATED &&
                               ADAPTER_TYPE_INTEGRATED > ADAPTER_TYPE_SOFTWARE &&
                               ADAPTER_TYPE_SOFTWARE > ADAPTER_TYPE_UNKNOWN),
                              "Unexpected ADAPTER_TYPE enum ordering");
                if (AdapterType > m_AdapterType)
                {
                    // Prefer Discrete over Integrated over Software
                    m_AdapterType = AdapterType;
                    AdapterId     = i;
                }
                else if (AdapterType == m_AdapterType)
                {
                    // Select adapter with more memory
                    const auto& NewAdapterMem   = AdapterInfo.Memory;
                    const auto  NewTotalMemory  = NewAdapterMem.LocalMemory + NewAdapterMem.HostVisibleMemory + NewAdapterMem.UnifiedMemory;
                    const auto& CurrAdapterMem  = Adapters[AdapterId].Memory;
                    const auto  CurrTotalMemory = CurrAdapterMem.LocalMemory + CurrAdapterMem.HostVisibleMemory + CurrAdapterMem.UnifiedMemory;
                    if (NewTotalMemory > CurrTotalMemory)
                    {
                        AdapterId = i;
                    }
                }
            }
        }

        if (AdapterId != DEFAULT_ADAPTER_ID)
        {
            AdapterAttribs = Adapters[AdapterId];
            LOG_INFO_MESSAGE("Using adapter ", AdapterId, ": '", AdapterAttribs.Description, "'");
        }

        return AdapterId;
    };
#endif

    std::vector<IDeviceContext*> ppContexts;
    switch (m_DeviceType)
    {
#if D3D11_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D11:
        {
#    if ENGINE_DLL
            // Load the dll and import GetEngineFactoryD3D11() function
            auto GetEngineFactoryD3D11 = LoadGraphicsEngineD3D11();
#    endif
            auto* pFactoryD3D11 = GetEngineFactoryD3D11();
            m_pEngineFactory    = pFactoryD3D11;

            EngineD3D11CreateInfo EngineCI;
            EngineCI.GraphicsAPIVersion = {11, 0};

#    ifdef DILIGENT_DEBUG
            EngineCI.SetValidationLevel(VALIDATION_LEVEL_2);
#    endif
            if (m_ValidationLevel >= 0)
                EngineCI.SetValidationLevel(static_cast<VALIDATION_LEVEL>(m_ValidationLevel));

            EngineCI.AdapterId = FindAdapter(pFactoryD3D11, EngineCI.GraphicsAPIVersion, m_AdapterAttribs);
            m_TheSample->ModifyEngineInitInfo({pFactoryD3D11, m_DeviceType, EngineCI, m_SwapChainInitDesc});

            if (m_AdapterType != ADAPTER_TYPE_SOFTWARE && EngineCI.AdapterId != DEFAULT_ADAPTER_ID)
            {
                // Display mode enumeration fails with error for software adapter
                Uint32 NumDisplayModes = 0;
                pFactoryD3D11->EnumerateDisplayModes(EngineCI.GraphicsAPIVersion, EngineCI.AdapterId, 0, TEX_FORMAT_RGBA8_UNORM_SRGB, NumDisplayModes, nullptr);
                m_DisplayModes.resize(NumDisplayModes);
                pFactoryD3D11->EnumerateDisplayModes(EngineCI.GraphicsAPIVersion, EngineCI.AdapterId, 0, TEX_FORMAT_RGBA8_UNORM_SRGB, NumDisplayModes, m_DisplayModes.data());
            }

            NumImmediateContexts = std::max(1u, EngineCI.NumImmediateContexts);
            ppContexts.resize(size_t{NumImmediateContexts} + size_t{EngineCI.NumDeferredContexts});
            pFactoryD3D11->CreateDeviceAndContextsD3D11(EngineCI, &m_pDevice, ppContexts.data());
            if (!m_pDevice)
            {
                LOG_ERROR_AND_THROW("Unable to initialize Diligent Engine in Direct3D11 mode. The API may not be available, "
                                    "or required features may not be supported by this GPU/driver/OS version.");
            }

            if (pWindow != nullptr)
                pFactoryD3D11->CreateSwapChainD3D11(m_pDevice, ppContexts[0], m_SwapChainInitDesc, FullScreenModeDesc{}, *pWindow, &m_pSwapChain);
        }
        break;
#endif

#if D3D12_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D12:
        {
#    if ENGINE_DLL
            // Load the dll and import GetEngineFactoryD3D12() function
            auto GetEngineFactoryD3D12 = LoadGraphicsEngineD3D12();
#    endif
            auto* pFactoryD3D12 = GetEngineFactoryD3D12();
            if (!pFactoryD3D12->LoadD3D12())
            {
                LOG_ERROR_AND_THROW("Failed to load Direct3D12");
            }
            m_pEngineFactory = pFactoryD3D12;

            EngineD3D12CreateInfo EngineCI;
            EngineCI.GraphicsAPIVersion = {11, 0};
            if (m_ValidationLevel >= 0)
                EngineCI.SetValidationLevel(static_cast<VALIDATION_LEVEL>(m_ValidationLevel));

            try
            {
                EngineCI.AdapterId = FindAdapter(pFactoryD3D12, EngineCI.GraphicsAPIVersion, m_AdapterAttribs);
            }
            catch (...)
            {
#    if D3D11_SUPPORTED
                LOG_ERROR_MESSAGE("Failed to find Direct3D12-compatible hardware adapters. Attempting to initialize the engine in Direct3D11 mode.");
                m_DeviceType = RENDER_DEVICE_TYPE_D3D11;
                InitializeDiligentEngine(pWindow);
                return;
#    else
                throw;
#    endif
            }

            m_TheSample->ModifyEngineInitInfo({pFactoryD3D12, m_DeviceType, EngineCI, m_SwapChainInitDesc});

            if (m_AdapterType != ADAPTER_TYPE_SOFTWARE && EngineCI.AdapterId != DEFAULT_ADAPTER_ID)
            {
                // Display mode enumeration fails with error for software adapter
                Uint32 NumDisplayModes = 0;
                pFactoryD3D12->EnumerateDisplayModes(EngineCI.GraphicsAPIVersion, EngineCI.AdapterId, 0, TEX_FORMAT_RGBA8_UNORM_SRGB, NumDisplayModes, nullptr);
                m_DisplayModes.resize(NumDisplayModes);
                pFactoryD3D12->EnumerateDisplayModes(EngineCI.GraphicsAPIVersion, EngineCI.AdapterId, 0, TEX_FORMAT_RGBA8_UNORM_SRGB, NumDisplayModes, m_DisplayModes.data());
            }

            NumImmediateContexts = std::max(1u, EngineCI.NumImmediateContexts);
            ppContexts.resize(NumImmediateContexts + EngineCI.NumDeferredContexts);
            pFactoryD3D12->CreateDeviceAndContextsD3D12(EngineCI, &m_pDevice, ppContexts.data());
            if (!m_pDevice)
            {
                LOG_ERROR_AND_THROW("Unable to initialize Diligent Engine in Direct3D12 mode. The API may not be available, "
                                    "or required features may not be supported by this GPU/driver/OS version.");
            }

            if (!m_pSwapChain && pWindow != nullptr)
                pFactoryD3D12->CreateSwapChainD3D12(m_pDevice, ppContexts[0], m_SwapChainInitDesc, FullScreenModeDesc{}, *pWindow, &m_pSwapChain);
        }
        break;
#endif

#if GL_SUPPORTED || GLES_SUPPORTED
        case RENDER_DEVICE_TYPE_GL:
        case RENDER_DEVICE_TYPE_GLES:
        {
#    if !PLATFORM_MACOS
            VERIFY_EXPR(pWindow != nullptr);
#    endif
#    if EXPLICITLY_LOAD_ENGINE_GL_DLL
            // Load the dll and import GetEngineFactoryOpenGL() function
            auto GetEngineFactoryOpenGL = LoadGraphicsEngineOpenGL();
#    endif
            auto* pFactoryOpenGL = GetEngineFactoryOpenGL();
            m_pEngineFactory     = pFactoryOpenGL;

            EngineGLCreateInfo EngineCI;
            EngineCI.Window = *pWindow;

            if (m_ValidationLevel >= 0)
                EngineCI.SetValidationLevel(static_cast<VALIDATION_LEVEL>(m_ValidationLevel));

            m_TheSample->ModifyEngineInitInfo({pFactoryOpenGL, m_DeviceType, EngineCI, m_SwapChainInitDesc});

            if (m_bForceNonSeprblProgs)
                EngineCI.Features.SeparablePrograms = DEVICE_FEATURE_STATE_DISABLED;
            if (EngineCI.NumDeferredContexts != 0)
            {
                LOG_WARNING_MESSAGE("Deferred contexts are not supported in OpenGL mode");
                EngineCI.NumDeferredContexts = 0;
            }

            NumImmediateContexts = 1;
            ppContexts.resize(NumImmediateContexts + EngineCI.NumDeferredContexts);
            pFactoryOpenGL->CreateDeviceAndSwapChainGL(EngineCI, &m_pDevice, ppContexts.data(), m_SwapChainInitDesc, &m_pSwapChain);
            if (!m_pDevice)
            {
                LOG_ERROR_AND_THROW("Unable to initialize Diligent Engine in OpenGL mode. The API may not be available, "
                                    "or required features may not be supported by this GPU/driver/OS version.");
            }
        }
        break;
#endif

#if VULKAN_SUPPORTED
        case RENDER_DEVICE_TYPE_VULKAN:
        {
#    if EXPLICITLY_LOAD_ENGINE_VK_DLL
            // Load the dll and import GetEngineFactoryVk() function
            auto GetEngineFactoryVk = LoadGraphicsEngineVk();
#    endif
            EngineVkCreateInfo EngineCI;
            if (m_ValidationLevel >= 0)
                EngineCI.SetValidationLevel(static_cast<VALIDATION_LEVEL>(m_ValidationLevel));

            const char* const ppIgnoreDebugMessages[] = //
                {
                    // Validation Performance Warning: [ UNASSIGNED-CoreValidation-Shader-OutputNotConsumed ]
                    // vertex shader writes to output location 1.0 which is not consumed by fragment shader
                    "UNASSIGNED-CoreValidation-Shader-OutputNotConsumed" //
                };
            EngineCI.ppIgnoreDebugMessageNames = ppIgnoreDebugMessages;
            EngineCI.IgnoreDebugMessageCount   = _countof(ppIgnoreDebugMessages);

            auto* pFactoryVk = GetEngineFactoryVk();
            m_pEngineFactory = pFactoryVk;

            EngineCI.AdapterId = FindAdapter(pFactoryVk, EngineCI.GraphicsAPIVersion, m_AdapterAttribs);
            m_TheSample->ModifyEngineInitInfo({pFactoryVk, m_DeviceType, EngineCI, m_SwapChainInitDesc});

            NumImmediateContexts = std::max(1u, EngineCI.NumImmediateContexts);
            ppContexts.resize(NumImmediateContexts + EngineCI.NumDeferredContexts);
            pFactoryVk->CreateDeviceAndContextsVk(EngineCI, &m_pDevice, ppContexts.data());
            if (!m_pDevice)
            {
                LOG_ERROR_AND_THROW("Unable to initialize Diligent Engine in Vulkan mode. The API may not be available, "
                                    "or required features may not be supported by this GPU/driver/OS version.");
            }

            if (!m_pSwapChain && pWindow != nullptr)
                pFactoryVk->CreateSwapChainVk(m_pDevice, ppContexts[0], m_SwapChainInitDesc, *pWindow, &m_pSwapChain);
        }
        break;
#endif


#if METAL_SUPPORTED
        case RENDER_DEVICE_TYPE_METAL:
        {
            EngineMtlCreateInfo EngineCI;
            if (m_ValidationLevel >= 0)
                EngineCI.SetValidationLevel(static_cast<VALIDATION_LEVEL>(m_ValidationLevel));

            auto* pFactoryMtl = GetEngineFactoryMtl();
            m_pEngineFactory  = pFactoryMtl;

            m_TheSample->ModifyEngineInitInfo({pFactoryMtl, m_DeviceType, EngineCI, m_SwapChainInitDesc});

            NumImmediateContexts = std::max(1u, EngineCI.NumImmediateContexts);
            ppContexts.resize(NumImmediateContexts + EngineCI.NumDeferredContexts);
            pFactoryMtl->CreateDeviceAndContextsMtl(EngineCI, &m_pDevice, ppContexts.data());
            if (!m_pDevice)
            {
                LOG_ERROR_AND_THROW("Unable to initialize Diligent Engine in Metal mode. The API may not be available, "
                                    "or required features may not be supported by this GPU/driver/OS version.");
            }

            if (!m_pSwapChain && pWindow != nullptr)
                pFactoryMtl->CreateSwapChainMtl(m_pDevice, ppContexts[0], m_SwapChainInitDesc, *pWindow, &m_pSwapChain);
        }
        break;
#endif

        default:
            LOG_ERROR_AND_THROW("Unknown device type");
            break;
    }

    m_AppTitle.append(" (");
    m_AppTitle.append(GetRenderDeviceTypeString(m_DeviceType));
    m_AppTitle.append(", API ");
    m_AppTitle.append(std::to_string(DILIGENT_API_VERSION));
    m_AppTitle.push_back(')');

    m_NumImmediateContexts = NumImmediateContexts;
    m_pDeviceContexts.resize(ppContexts.size());
    for (size_t i = 0; i < ppContexts.size(); ++i)
        m_pDeviceContexts[i].Attach(ppContexts[i]);

    if (m_ScreenCaptureInfo.AllowCapture)
    {
        if (m_GoldenImgMode != GoldenImageMode::None)
        {
            // Capture only one frame
            m_ScreenCaptureInfo.FramesToCapture = 1;
        }

        m_pScreenCapture.reset(new ScreenCapture(m_pDevice));
    }
}

void SampleApp::InitializeSample()
{
#if PLATFORM_WIN32
    if (!m_DisplayModes.empty())
    {
        const HWND hDesktop = GetDesktopWindow();

        RECT rc;
        GetWindowRect(hDesktop, &rc);
        Uint32 ScreenWidth  = static_cast<Uint32>(rc.right - rc.left);
        Uint32 ScreenHeight = static_cast<Uint32>(rc.bottom - rc.top);
        for (int i = 0; i < static_cast<int>(m_DisplayModes.size()); ++i)
        {
            if (ScreenWidth == m_DisplayModes[i].Width && ScreenHeight == m_DisplayModes[i].Height)
            {
                m_SelectedDisplayMode = i;
                break;
            }
        }
    }
#endif

    const auto& SCDesc = m_pSwapChain->GetDesc();

    m_MaxFrameLatency = SCDesc.BufferCount;

    std::vector<IDeviceContext*> ppContexts(m_pDeviceContexts.size());
    for (size_t ctx = 0; ctx < m_pDeviceContexts.size(); ++ctx)
        ppContexts[ctx] = m_pDeviceContexts[ctx];

    SampleInitInfo InitInfo;
    InitInfo.pEngineFactory  = m_pEngineFactory;
    InitInfo.pDevice         = m_pDevice;
    InitInfo.ppContexts      = ppContexts.data();
    InitInfo.NumImmediateCtx = m_NumImmediateContexts;
    VERIFY_EXPR(m_pDeviceContexts.size() >= m_NumImmediateContexts);
    InitInfo.NumDeferredCtx = static_cast<Uint32>(m_pDeviceContexts.size()) - m_NumImmediateContexts;
    InitInfo.pSwapChain     = m_pSwapChain;
    InitInfo.pImGui         = m_pImGui.get();
    m_TheSample->Initialize(InitInfo);

    m_TheSample->WindowResize(SCDesc.Width, SCDesc.Height);
}

void SampleApp::UpdateAdaptersDialog()
{
#if PLATFORM_WIN32 || PLATFORM_LINUX
    const auto& SCDesc = m_pSwapChain->GetDesc();

    Uint32 AdaptersWndWidth = std::min(330u, SCDesc.Width);
    ImGui::SetNextWindowSize(ImVec2(static_cast<float>(AdaptersWndWidth), 0), ImGuiCond_Always);
    ImGui::SetNextWindowPos(ImVec2(static_cast<float>(std::max(SCDesc.Width - AdaptersWndWidth, 10U) - 10), 10), ImGuiCond_Always);
    ImGui::SetNextWindowCollapsed(true, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Adapters", nullptr, ImGuiWindowFlags_NoResize))
    {
        if (m_AdapterAttribs.Type != ADAPTER_TYPE_UNKNOWN)
        {
            ImGui::TextDisabled("Adapter: %s (%d MB)", m_AdapterAttribs.Description, static_cast<int>(m_AdapterAttribs.Memory.LocalMemory >> 20));
        }

        if (!m_DisplayModes.empty())
        {
            std::vector<const char*> DisplayModes(m_DisplayModes.size());
            std::vector<std::string> DisplayModeStrings(m_DisplayModes.size());
            for (int i = 0; i < static_cast<int>(m_DisplayModes.size()); ++i)
            {
                static constexpr const char* ScalingModeStr[] =
                    {
                        ""
                        " Centered",
                        " Stretched" //
                    };
                const auto& Mode = m_DisplayModes[i];

                std::stringstream ss;

                float RefreshRate = static_cast<float>(Mode.RefreshRateNumerator) / static_cast<float>(Mode.RefreshRateDenominator);
                ss << Mode.Width << "x" << Mode.Height << "@" << std::fixed << std::setprecision(2) << RefreshRate << " Hz" << ScalingModeStr[static_cast<int>(Mode.Scaling)];
                DisplayModeStrings[i] = ss.str();
                DisplayModes[i]       = DisplayModeStrings[i].c_str();
            }

            ImGui::SetNextItemWidth(220);
            ImGui::Combo("Display Modes", &m_SelectedDisplayMode, DisplayModes.data(), static_cast<int>(DisplayModes.size()));
        }

        if (m_bFullScreenMode)
        {
            if (ImGui::Button("Go Windowed"))
            {
                SetWindowedMode();
            }
        }
        else
        {
            if (!m_DisplayModes.empty())
            {
                if (ImGui::Button("Go Full Screen"))
                {
                    const auto& SelectedMode = m_DisplayModes[m_SelectedDisplayMode];
                    SetFullscreenMode(SelectedMode);
                }
            }
        }

        ImGui::Checkbox("VSync", &m_bVSync);

        if (m_pDevice->GetDeviceInfo().IsD3DDevice())
        {
            // clang-format off
            std::pair<Uint32, const char*> FrameLatencies[] = 
            {
                {1, "1"},
                {2, "2"},
                {3, "3"},
                {4, "4"},
                {5, "5"},
                {6, "6"},
                {7, "7"},
                {8, "8"},
                {9, "9"},
                {10, "10"}
            };
            // clang-format on

            if (SCDesc.BufferCount <= _countof(FrameLatencies) && m_MaxFrameLatency <= _countof(FrameLatencies))
            {
                ImGui::SetNextItemWidth(120);
                auto NumFrameLatencyItems = std::max(std::max(m_MaxFrameLatency, SCDesc.BufferCount), Uint32{4});
                if (ImGui::Combo("Max frame latency", &m_MaxFrameLatency, FrameLatencies, NumFrameLatencyItems))
                {
                    m_pSwapChain->SetMaximumFrameLatency(m_MaxFrameLatency);
                }
            }
            else
            {
                // 10+ buffer swap chain or frame latency? Something is not quite right
            }
        }
    }
    ImGui::End();
#endif
}


// Command line example to capture frames:
//
//     --mode d3d11 --adapters_dialog 0 --capture_path . --capture_fps 15 --capture_name frame -w 640 -h 480 --capture_format png --capture_quality 100 --capture_frames 3 --capture_alpha 0
//
// Image magick command to create animated gif:
//
//     magick convert  -delay 6  -loop 0 -layers Optimize -compress LZW -strip -resize 240x180   frame*.png   Animation.gif
//
SampleApp::CommandLineStatus SampleApp::ProcessCommandLine(int argc, const char* const* argv)
{
    if (argc == 0)
        return CommandLineStatus::OK;

    if (argv == nullptr)
    {
        UNEXPECTED("argv is null when argc (", argc, ") is not zero");
        return CommandLineStatus::Error;
    }

    CommandLineParser ArgsParser{argc, argv};

    ArgsParser.Parse("mode", 'm',
                     [&](const char* ArgVal) {
                         if (StrCmpNoCase(ArgVal, "d3d11_sw") == 0)
                         {
                             m_DeviceType  = RENDER_DEVICE_TYPE_D3D11;
                             m_AdapterType = ADAPTER_TYPE_SOFTWARE;
                             return true;
                         }
                         else if (StrCmpNoCase(ArgVal, "d3d12_sw") == 0)
                         {
                             m_DeviceType  = RENDER_DEVICE_TYPE_D3D12;
                             m_AdapterType = ADAPTER_TYPE_SOFTWARE;
                             return true;
                         }
                         else if (StrCmpNoCase(ArgVal, "vk_sw") == 0)
                         {
                             m_DeviceType  = RENDER_DEVICE_TYPE_VULKAN;
                             m_AdapterType = ADAPTER_TYPE_SOFTWARE;
                             return true;
                         }

                         const std::vector<std::pair<const char*, RENDER_DEVICE_TYPE>> DeviceTypeEnumVals =
                             {
                                 {"d3d11", RENDER_DEVICE_TYPE_D3D11},
                                 {"d3d12", RENDER_DEVICE_TYPE_D3D12},
                                 {"gl", RENDER_DEVICE_TYPE_GL},
                                 {"gles", RENDER_DEVICE_TYPE_GLES},
                                 {"vk", RENDER_DEVICE_TYPE_VULKAN},
                                 {"mtl", RENDER_DEVICE_TYPE_METAL} //
                             };
                         return ArgsParser.ParseEnum("mode", 'm', DeviceTypeEnumVals, m_DeviceType);
                     });

#if !D3D11_SUPPORTED
    if (m_DeviceType == RENDER_DEVICE_TYPE_D3D11)
    {
        m_DeviceType = RENDER_DEVICE_TYPE_UNDEFINED;
        LOG_ERROR_MESSAGE("Direct3D11 is not supported. Please select another device type");
    }
#endif
#if !D3D12_SUPPORTED
    if (m_DeviceType == RENDER_DEVICE_TYPE_D3D12)
    {
        m_DeviceType = RENDER_DEVICE_TYPE_UNDEFINED;
        LOG_ERROR_MESSAGE("Direct3D12 is not supported. Please select another device type");
    }
#endif
#if !GL_SUPPORTED
    if (m_DeviceType == RENDER_DEVICE_TYPE_GL)
    {
        m_DeviceType = RENDER_DEVICE_TYPE_UNDEFINED;
        LOG_ERROR_MESSAGE("OpenGL is not supported. Please select another device type");
    }
#endif
#if !GLES_SUPPORTED
    if (m_DeviceType == RENDER_DEVICE_TYPE_GLES)
    {
        m_DeviceType = RENDER_DEVICE_TYPE_UNDEFINED;
        LOG_ERROR_MESSAGE("OpenGLES is not supported. Please select another device type");
    }
#endif
#if !VULKAN_SUPPORTED
    if (m_DeviceType == RENDER_DEVICE_TYPE_VULKAN)
    {
        m_DeviceType = RENDER_DEVICE_TYPE_UNDEFINED;
        LOG_ERROR_MESSAGE("Vulkan is not supported. Please select another device type");
    }
#endif
#if !METAL_SUPPORTED
    if (m_DeviceType == RENDER_DEVICE_TYPE_METAL)
    {
        m_DeviceType = RENDER_DEVICE_TYPE_UNDEFINED;
        LOG_ERROR_MESSAGE("Metal is not supported. Please select another device type");
    }
#endif

    if (ArgsParser.Parse("capture_path", m_ScreenCaptureInfo.Directory))
        m_ScreenCaptureInfo.AllowCapture = true;

    if (ArgsParser.Parse("capture_name", m_ScreenCaptureInfo.FileName))
        m_ScreenCaptureInfo.AllowCapture = true;

    ArgsParser.Parse("capture_fps", m_ScreenCaptureInfo.CaptureFPS);
    ArgsParser.Parse("capture_frames", m_ScreenCaptureInfo.FramesToCapture);

    {
        const std::vector<std::pair<const char*, IMAGE_FILE_FORMAT>> FileFmtEnumVals =
            {
                {"jpeg", IMAGE_FILE_FORMAT_JPEG},
                {"jpg", IMAGE_FILE_FORMAT_JPEG},
                {"png", IMAGE_FILE_FORMAT_PNG} //
            };
        ArgsParser.ParseEnum("capture_format", '\0', FileFmtEnumVals, m_ScreenCaptureInfo.FileFormat);
    }

    ArgsParser.Parse("capture_quality", m_ScreenCaptureInfo.JpegQuality);
    ArgsParser.Parse("capture_alpha", m_ScreenCaptureInfo.KeepAlpha);
    ArgsParser.Parse("width", 'w', m_InitialWindowWidth);
    ArgsParser.Parse("height", 'h', m_InitialWindowHeight);
    ArgsParser.Parse("validation", m_ValidationLevel);

    ArgsParser.Parse("adapter", '\0',
                     [&](const char* ArgVal) {
                         if (StrCmpNoCase(ArgVal, "sw") == 0)
                         {
                             m_AdapterType = ADAPTER_TYPE_SOFTWARE;
                         }
                         else
                         {
                             auto AdapterId = atoi(ArgVal);
                             VERIFY_EXPR(AdapterId >= 0);
                             m_AdapterId = static_cast<Uint32>(AdapterId >= 0 ? AdapterId : 0);
                         }
                         return true;
                     });

    ArgsParser.Parse("adapters_dialog", m_bShowAdaptersDialog);
    ArgsParser.Parse("show_ui", m_bShowUI);

    {
        const std::vector<std::pair<const char*, GoldenImageMode>> GoldenImgModeEnumVals =
            {
                {"none", GoldenImageMode::None},
                {"capture", GoldenImageMode::Capture},
                {"compare", GoldenImageMode::Compare},
                {"compare_update", GoldenImageMode::CompareUpdate} //
            };
        ArgsParser.ParseEnum("golden_image_mode", '\0', GoldenImgModeEnumVals, m_GoldenImgMode);
    }

    ArgsParser.Parse("golden_image_tolerance", m_GoldenImgPixelTolerance);
    ArgsParser.Parse("vsync", m_bVSync);
    ArgsParser.Parse("non_separable_progs", m_bForceNonSeprblProgs);


    if (m_DeviceType == RENDER_DEVICE_TYPE_UNDEFINED)
    {
        SelectDeviceType();
        if (m_DeviceType == RENDER_DEVICE_TYPE_UNDEFINED)
        {
#if D3D12_SUPPORTED
            m_DeviceType = RENDER_DEVICE_TYPE_D3D12;
#elif VULKAN_SUPPORTED
            m_DeviceType = RENDER_DEVICE_TYPE_VULKAN;
#elif D3D11_SUPPORTED
            m_DeviceType = RENDER_DEVICE_TYPE_D3D11;
#elif GL_SUPPORTED || GLES_SUPPORTED
            m_DeviceType = RENDER_DEVICE_TYPE_GL;
#endif
        }
    }

    return m_TheSample->ProcessCommandLine(ArgsParser.ArgC(), ArgsParser.ArgV());
}

void SampleApp::WindowResize(int width, int height)
{
    if (m_pSwapChain)
    {
        m_TheSample->PreWindowResize();
        m_pSwapChain->Resize(width, height);
        auto SCWidth  = m_pSwapChain->GetDesc().Width;
        auto SCHeight = m_pSwapChain->GetDesc().Height;
        m_TheSample->WindowResize(SCWidth, SCHeight);
    }
}

void SampleApp::Update(double CurrTime, double ElapsedTime)
{
    m_CurrentTime = CurrTime;

    UpdateAppSettings(false);

    if (m_pImGui)
    {
        const auto& SCDesc = m_pSwapChain->GetDesc();
        m_pImGui->NewFrame(SCDesc.Width, SCDesc.Height, SCDesc.PreTransform);
        if (m_bShowAdaptersDialog)
        {
            UpdateAdaptersDialog();
        }
    }
    if (m_pDevice)
    {
        m_TheSample->Update(CurrTime, ElapsedTime);
        m_TheSample->GetInputController().ClearState();
    }
}

void SampleApp::Render()
{
    if (m_NumImmediateContexts == 0 || !m_pSwapChain)
        return;

    auto* pCtx = GetImmediateContext();
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
    pCtx->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    m_TheSample->Render();

    // Restore default render target in case the sample has changed it
    pCtx->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    if (m_pImGui)
    {
        if (m_bShowUI)
        {
            // No need to call EndFrame as ImGui::Render calls it automatically
            m_pImGui->Render(pCtx);
        }
        else
        {
            m_pImGui->EndFrame();
        }
    }
}

void SampleApp::CompareGoldenImage(const std::string& FileName, ScreenCapture::CaptureInfo& Capture)
{
    RefCntAutoPtr<Image> pGoldenImg;
    CreateImageFromFile(FileName.c_str(), &pGoldenImg, nullptr);
    if (!pGoldenImg)
    {
        LOG_ERROR_MESSAGE("Failed to load golden image from file ", FileName);
        m_ExitCode = 2;
        return;
    }

    const auto& TexDesc       = Capture.pTexture->GetDesc();
    const auto& GoldenImgDesc = pGoldenImg->GetDesc();
    if (GoldenImgDesc.Width != TexDesc.Width)
    {
        LOG_ERROR_MESSAGE("Golden image width (", GoldenImgDesc.Width, ") does not match the captured image width (", TexDesc.Width, ")");
        m_ExitCode = 3;
        return;
    }
    if (GoldenImgDesc.Height != TexDesc.Height)
    {
        LOG_ERROR_MESSAGE("Golden image height (", GoldenImgDesc.Height, ") does not match the captured image height (", TexDesc.Height, ")");
        m_ExitCode = 4;
        return;
    }

    auto* const pCtx = GetImmediateContext();

    MappedTextureSubresource TexData;
    pCtx->MapTextureSubresource(Capture.pTexture, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, TexData);
    auto CapturedPixels = Image::ConvertImageData(TexDesc.Width, TexDesc.Height,
                                                  reinterpret_cast<const Uint8*>(TexData.pData), static_cast<Uint32>(TexData.Stride),
                                                  TexDesc.Format, TEX_FORMAT_RGBA8_UNORM, false /*Keep alpha*/);
    pCtx->UnmapTextureSubresource(Capture.pTexture, 0, 0);

    auto* pGoldenImgPixels = reinterpret_cast<const Uint8*>(pGoldenImg->GetData()->GetDataPtr());


    size_t NumBadPixels = 0;
    for (size_t row = 0; row < TexDesc.Height; ++row)
    {
        for (size_t col = 0; col < TexDesc.Width; ++col)
        {
            const auto* SrcPixel = &CapturedPixels[(col + row * size_t{TexDesc.Width}) * 3u];
            const auto* DstPixel = pGoldenImgPixels + row * size_t{GoldenImgDesc.RowStride} + col * size_t{GoldenImgDesc.NumComponents};
            if (std::abs(int{SrcPixel[0]} - int{DstPixel[0]}) > m_GoldenImgPixelTolerance ||
                std::abs(int{SrcPixel[1]} - int{DstPixel[1]}) > m_GoldenImgPixelTolerance ||
                std::abs(int{SrcPixel[2]} - int{DstPixel[2]}) > m_GoldenImgPixelTolerance)
                ++NumBadPixels;
        }
    }
    if (NumBadPixels == 0)
        LOG_INFO_MESSAGE(TextColorCode::Green, GetAppTitle(), ": golden image validation PASSED.", TextColorCode::Default);
    else
        LOG_ERROR_MESSAGE(GetAppTitle(), ": golden image validation FAILED: ", NumBadPixels, " inconsistent pixels found.");

    m_ExitCode = NumBadPixels > 0 ? 10 : 0;
}

void SampleApp::SaveScreenCapture(const std::string& FileName, ScreenCapture::CaptureInfo& Capture)
{
    auto* const pCtx = GetImmediateContext();

    MappedTextureSubresource TexData;
    pCtx->MapTextureSubresource(Capture.pTexture, 0, 0, MAP_READ, MAP_FLAG_DO_NOT_WAIT, nullptr, TexData);
    const auto& TexDesc = Capture.pTexture->GetDesc();

    Image::EncodeInfo Info;
    Info.Width       = TexDesc.Width;
    Info.Height      = TexDesc.Height;
    Info.TexFormat   = TexDesc.Format;
    Info.KeepAlpha   = m_ScreenCaptureInfo.KeepAlpha;
    Info.pData       = TexData.pData;
    Info.Stride      = static_cast<Uint32>(TexData.Stride);
    Info.FileFormat  = m_ScreenCaptureInfo.FileFormat;
    Info.JpegQuality = m_ScreenCaptureInfo.JpegQuality;

    RefCntAutoPtr<IDataBlob> pEncodedImage;
    Image::Encode(Info, &pEncodedImage);
    pCtx->UnmapTextureSubresource(Capture.pTexture, 0, 0);

    FileWrapper pFile(FileName.c_str(), EFileAccessMode::Overwrite);
    if (pFile)
    {
        auto res = pFile->Write(pEncodedImage->GetDataPtr(), pEncodedImage->GetSize());
        if (!res)
        {
            LOG_ERROR_MESSAGE("Failed to write screen capture file '", FileName, "'.");
            m_ExitCode = 5;
        }
        pFile.Close();
    }
    else
    {
        LOG_ERROR_MESSAGE("Failed to create screen capture file '", FileName, "'. Verify that the directory exists and the app has sufficient rights to write to this directory.");
        m_ExitCode = 6;
    }

    // Do NOT set exit code to 0! We must not clear the previous error code.
}

void SampleApp::Present()
{
    if (!m_pSwapChain)
        return;

    auto* const pCtx = GetImmediateContext();

    if (m_pScreenCapture && m_ScreenCaptureInfo.FramesToCapture > 0)
    {
        if (m_CurrentTime - m_ScreenCaptureInfo.LastCaptureTime >= 1.0 / m_ScreenCaptureInfo.CaptureFPS)
        {
            pCtx->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
            m_pScreenCapture->Capture(m_pSwapChain, pCtx, m_ScreenCaptureInfo.CurrentFrame);

            m_ScreenCaptureInfo.LastCaptureTime = m_CurrentTime;

            --m_ScreenCaptureInfo.FramesToCapture;
            ++m_ScreenCaptureInfo.CurrentFrame;

            if (m_GoldenImgMode != GoldenImageMode::None)
            {
                VERIFY(m_ScreenCaptureInfo.FramesToCapture == 0, "Only single frame is expected to be captured in golden image capture/comparison modes");
                // Idle the context to make the capture available
                pCtx->WaitForIdle();
                if (!m_pScreenCapture->HasCapture())
                {
                    LOG_ERROR_MESSAGE("Screen capture is not available after idling the context");
                    m_ExitCode = 1;
                }
            }
        }
    }

    m_pSwapChain->Present(m_bVSync ? 1 : 0);

    if (m_pScreenCapture)
    {
        while (auto Capture = m_pScreenCapture->GetCapture())
        {
            std::string FileName;
            {
                std::stringstream FileNameSS;
                if (!m_ScreenCaptureInfo.Directory.empty())
                {
                    FileNameSS << m_ScreenCaptureInfo.Directory;
                    if (m_ScreenCaptureInfo.Directory.back() != '/')
                        FileNameSS << '/';
                }
                FileNameSS << m_ScreenCaptureInfo.FileName;
                if (m_GoldenImgMode == GoldenImageMode::None)
                {
                    FileNameSS << std::setw(3) << std::setfill('0') << Capture.Id;
                }
                FileNameSS << (m_ScreenCaptureInfo.FileFormat == IMAGE_FILE_FORMAT_JPEG ? ".jpg" : ".png");
                FileName = FileNameSS.str();
            }

            if (m_GoldenImgMode == GoldenImageMode::Compare || m_GoldenImgMode == GoldenImageMode::CompareUpdate)
            {
                CompareGoldenImage(FileName, Capture);
            }

            if (m_GoldenImgMode == GoldenImageMode::None ||
                m_GoldenImgMode == GoldenImageMode::Capture ||
                m_GoldenImgMode == GoldenImageMode::CompareUpdate)
            {
                SaveScreenCapture(FileName, Capture);
            }

            m_pScreenCapture->RecycleStagingTexture(std::move(Capture.pTexture));
        }
    }
}

} // namespace Diligent
