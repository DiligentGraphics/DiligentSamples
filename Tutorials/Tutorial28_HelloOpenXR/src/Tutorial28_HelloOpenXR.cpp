/*
 *  Copyright 2024 Diligent Graphics LLC
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

#include <memory>
#include <iomanip>
#include <iostream>
#include <vector>

#ifndef NOMINMAX
#    define NOMINMAX
#endif
#include <crtdbg.h>

#if D3D11_SUPPORTED
#    include "EngineFactoryD3D11.h"
#endif

#if D3D12_SUPPORTED
#    include "EngineFactoryD3D12.h"
#endif

#if GL_SUPPORTED
#    include "EngineFactoryOpenGL.h"
#endif

#if VULKAN_SUPPORTED
#    include "EngineFactoryVk.h"
#endif

#include "RenderDevice.h"
#include "DeviceContext.h"
#include "SwapChain.h"

#include "RefCntAutoPtr.hpp"
#include "StringTools.h"
#include "OpenXRUtilities.h"
#include "GraphicsUtilities.h"
#include "GraphicsAccessories.hpp"
#include "GraphicsTypesX.hpp"
#include "MapHelper.hpp"

#include "../../Common/src/TexturedCube.hpp"

#include <openxr/openxr.h>

using namespace Diligent;

inline const char* GetXRErrorString(XrInstance xrInstance, XrResult result)
{
    static char string[XR_MAX_RESULT_STRING_SIZE];
    xrResultToString(xrInstance, result, string);
    return string;
}

#define OPENXR_CHECK(x, y)                                                                                                                    \
    do                                                                                                                                        \
    {                                                                                                                                         \
        XrResult result = (x);                                                                                                                \
        CHECK_ERR(XR_SUCCEEDED(result), "OPENXR: ", int(result), "(", (m_xrInstance ? GetXRErrorString(m_xrInstance, result) : ""), ") ", y); \
    } while (false)

const char* GetGraphicsAPIInstanceExtensionString(RENDER_DEVICE_TYPE Type)
{
    switch (Type)
    {
#if D3D11_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D11: return "XR_KHR_D3D11_enable";
#endif

#if D3D12_SUPPORTED
        case RENDER_DEVICE_TYPE_D3D12: return "XR_KHR_D3D12_enable";
#endif

#if GL_SUPPORTED
        case RENDER_DEVICE_TYPE_GL: return "XR_KHR_opengl_enable";
#endif

#if VULKAN_SUPPORTED
        case RENDER_DEVICE_TYPE_VULKAN: return "XR_KHR_vulkan_enable2";
#endif

        default:
            UNEXPECTED("Unknown device type");
            return nullptr;
    }
}

namespace HLSL
{

struct Constants
{
    float4x4 WorldViewProj;
    float4x4 Model;
    float4   Color;
};

} // namespace HLSL

class Tutorial28_HelloOpenXR
{
public:
    Tutorial28_HelloOpenXR()
    {
    }

    ~Tutorial28_HelloOpenXR()
    {
        m_pImmediateContext->Flush();

        // Make sure that the swap chains are not used by the GPU before they are destroyed
        m_Device.IdleGPU();
        DestroyXrSwapchains();

        if (m_LocalSpace)
        {
            OPENXR_CHECK(xrDestroySpace(m_LocalSpace), "Failed to destroy Space.");
        }
        if (m_xrSession)
        {
            OPENXR_CHECK(xrDestroySession(m_xrSession), "Failed to destroy Session.");
        }
        if (m_DebugUtilsMessenger)
        {
            DestroyOpenXRDebugUtilsMessenger(m_DebugUtilsMessenger);
        }
        if (m_xrInstance)
        {
            OPENXR_CHECK(xrDestroyInstance(m_xrInstance), "Failed to destroy Instance.");
        }
    }

    void CreateXrInstance()
    {
        // Fill out an XrApplicationInfo structure detailing the names and OpenXR version.
        // The application/engine name and version are user-definied. These may help IHVs or runtimes.
        XrApplicationInfo AI{};
        strcpy_s(AI.applicationName, XR_MAX_APPLICATION_NAME_SIZE, "Diligent Engine - Hello OpenXR Tutorial");
        AI.applicationVersion = 1;
        strcpy_s(AI.engineName, XR_MAX_ENGINE_NAME_SIZE, "Diligent Engine");
        AI.engineVersion = DILIGENT_API_VERSION;
        AI.apiVersion    = XR_CURRENT_API_VERSION;

        // Get all the API Layers from the OpenXR runtime.
        uint32_t                          apiLayerCount = 0;
        std::vector<XrApiLayerProperties> apiLayerProperties;
        OPENXR_CHECK(xrEnumerateApiLayerProperties(0, &apiLayerCount, nullptr), "Failed to enumerate ApiLayerProperties.");
        apiLayerProperties.resize(apiLayerCount, {XR_TYPE_API_LAYER_PROPERTIES});
        OPENXR_CHECK(xrEnumerateApiLayerProperties(apiLayerCount, &apiLayerCount, apiLayerProperties.data()), "Failed to enumerate ApiLayerProperties.");

        // Check the requested API layers against the ones from the OpenXR. If found add it to the Active API Layers.
        for (const std::string& requestLayer : m_ApiLayers)
        {
            for (const XrApiLayerProperties& layerProperty : apiLayerProperties)
            {
                if (requestLayer.c_str() == layerProperty.layerName)
                {
                    m_ActiveAPILayers.push_back(requestLayer.c_str());
                    break;
                }
            }
        }

        // Get all the Instance Extensions from the OpenXR instance.
        uint32_t                           extensionCount = 0;
        std::vector<XrExtensionProperties> extensionProperties;
        OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr), "Failed to enumerate InstanceExtensionProperties.");
        extensionProperties.resize(extensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
        OPENXR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr, extensionCount, &extensionCount, extensionProperties.data()), "Failed to enumerate InstanceExtensionProperties.");

        // Check the requested Instance Extensions against the ones from the OpenXR runtime.
        // If an extension is found add it to Active Instance Extensions.
        auto CheckExtension = [&extensionProperties](const char* extensionName) -> bool {
            for (const XrExtensionProperties& extensionProperty : extensionProperties)
            {
                if (strcmp(extensionName, extensionProperty.extensionName) == 0)
                {
                    return true;
                }
            }
            return false;
        };

        // Add additional instance layers/extensions
        const char* GraphicsAPIInstanceExtension = GetGraphicsAPIInstanceExtensionString(m_DeviceType);
        if (!CheckExtension(GraphicsAPIInstanceExtension))
        {
            LOG_ERROR_AND_THROW("OpenXR instance does not support required graphics API extension ", GraphicsAPIInstanceExtension);
        }
        std::vector<const char*> InstanceExtensions{GraphicsAPIInstanceExtension};

        bool DebugUtilsMessengerEnabled = CheckExtension(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
        if (DebugUtilsMessengerEnabled)
        {
            InstanceExtensions.push_back(XR_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        // Fill out an XrInstanceCreateInfo structure and create an XrInstance.
        XrInstanceCreateInfo instanceCI{XR_TYPE_INSTANCE_CREATE_INFO};
        instanceCI.createFlags           = 0;
        instanceCI.applicationInfo       = AI;
        instanceCI.enabledApiLayerCount  = static_cast<uint32_t>(m_ActiveAPILayers.size());
        instanceCI.enabledApiLayerNames  = m_ActiveAPILayers.data();
        instanceCI.enabledExtensionCount = static_cast<uint32_t>(InstanceExtensions.size());
        instanceCI.enabledExtensionNames = InstanceExtensions.data();
        OPENXR_CHECK(xrCreateInstance(&instanceCI, &m_xrInstance), "Failed to create Instance.");

        if (DebugUtilsMessengerEnabled)
        {
            constexpr XrDebugUtilsMessageSeverityFlagsEXT xrMessageSeverities =
                XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

            m_DebugUtilsMessenger = CreateOpenXRDebugUtilsMessenger(m_xrInstance, xrMessageSeverities);
        }
    }

    void GetXrInstanceProperties()
    {
        // Get the instance's properties and log the runtime name and version.
        XrInstanceProperties instanceProperties{XR_TYPE_INSTANCE_PROPERTIES};
        OPENXR_CHECK(xrGetInstanceProperties(m_xrInstance, &instanceProperties), "Failed to get InstanceProperties.");

        LOG_INFO_MESSAGE("OpenXR Runtime: ", instanceProperties.runtimeName, " - ",
                         XR_VERSION_MAJOR(instanceProperties.runtimeVersion), ".",
                         XR_VERSION_MINOR(instanceProperties.runtimeVersion), ".",
                         XR_VERSION_PATCH(instanceProperties.runtimeVersion));
    }

    void GetXrSystemID()
    {
        // Get the XrSystemId from the instance and the supplied XrFormFactor.
        XrSystemGetInfo systemGI{XR_TYPE_SYSTEM_GET_INFO};
        systemGI.formFactor = m_FormFactor;
        OPENXR_CHECK(xrGetSystem(m_xrInstance, &systemGI, &m_SystemId), "Failed to get SystemID.");

        // Get the System's properties for some general information about the hardware and the vendor.
        OPENXR_CHECK(xrGetSystemProperties(m_xrInstance, m_SystemId, &m_SystemProperties), "Failed to get SystemProperties.");
    }

    void GetViewConfigurationViews()
    {
        // Gets the View Configuration Types. The first call gets the count of the array that will be returned. The next call fills out the array.
        uint32_t viewConfigurationCount = 0;
        OPENXR_CHECK(xrEnumerateViewConfigurations(m_xrInstance, m_SystemId, 0, &viewConfigurationCount, nullptr), "Failed to enumerate View Configurations.");
        std::vector<XrViewConfigurationType> ViewConfigurations(viewConfigurationCount);
        OPENXR_CHECK(xrEnumerateViewConfigurations(m_xrInstance, m_SystemId, viewConfigurationCount, &viewConfigurationCount, ViewConfigurations.data()), "Failed to enumerate View Configurations.");

        // Pick the first application supported View Configuration Type con supported by the hardware.
        for (const XrViewConfigurationType& viewConfiguration : m_ApplicationViewConfigurations)
        {
            if (std::find(ViewConfigurations.begin(), ViewConfigurations.end(), viewConfiguration) != ViewConfigurations.end())
            {
                m_ViewConfiguration = viewConfiguration;
                break;
            }
        }
        if (m_ViewConfiguration == XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM)
        {
            LOG_WARNING_MESSAGE("Failed to find a view configuration type. Defaulting to XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO.");
            m_ViewConfiguration = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        }

        // Gets the View Configuration Views. The first call gets the count of the array that will be returned. The next call fills out the array.
        uint32_t viewConfigurationViewCount = 0;
        OPENXR_CHECK(xrEnumerateViewConfigurationViews(m_xrInstance, m_SystemId, m_ViewConfiguration, 0, &viewConfigurationViewCount, nullptr), "Failed to enumerate ViewConfiguration Views.");
        m_ViewConfigurationViews.resize(viewConfigurationViewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        OPENXR_CHECK(xrEnumerateViewConfigurationViews(m_xrInstance, m_SystemId, m_ViewConfiguration, viewConfigurationViewCount, &viewConfigurationViewCount, m_ViewConfigurationViews.data()), "Failed to enumerate ViewConfiguration Views.");
    }

    void GetEnvironmentBlendModes()
    {
        // Retrieve the available blend modes. The first call gets the count of the array that will be returned. The next call fills out the array.
        uint32_t environmentBlendModeCount = 0;
        OPENXR_CHECK(xrEnumerateEnvironmentBlendModes(m_xrInstance, m_SystemId, m_ViewConfiguration, 0, &environmentBlendModeCount, nullptr), "Failed to enumerate EnvironmentBlend Modes.");
        std::vector<XrEnvironmentBlendMode> EnvironmentBlendModes(environmentBlendModeCount);
        OPENXR_CHECK(xrEnumerateEnvironmentBlendModes(m_xrInstance, m_SystemId, m_ViewConfiguration, environmentBlendModeCount, &environmentBlendModeCount, EnvironmentBlendModes.data()), "Failed to enumerate EnvironmentBlend Modes.");

        // Pick the first application supported blend mode supported by the hardware.
        for (const XrEnvironmentBlendMode& environmentBlendMode : {XR_ENVIRONMENT_BLEND_MODE_OPAQUE, XR_ENVIRONMENT_BLEND_MODE_ADDITIVE})
        {
            if (std::find(EnvironmentBlendModes.begin(), EnvironmentBlendModes.end(), environmentBlendMode) != EnvironmentBlendModes.end())
            {
                m_EnvironmentBlendMode = environmentBlendMode;
                break;
            }
        }
        if (m_EnvironmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM)
        {
            LOG_INFO_MESSAGE("Failed to find a compatible blend mode. Defaulting to XR_ENVIRONMENT_BLEND_MODE_OPAQUE.");
            m_EnvironmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        }
    }

    void CreateXrSession()
    {
        RefCntAutoPtr<IDataBlob> pGraphicsBinding;
        GetOpenXRGraphicsBinding(m_Device, m_pImmediateContext, &pGraphicsBinding);

        XrSessionCreateInfo sessionCI{XR_TYPE_SESSION_CREATE_INFO};
        sessionCI.next        = pGraphicsBinding->GetConstDataPtr();
        sessionCI.createFlags = 0;
        sessionCI.systemId    = m_SystemId;

        OPENXR_CHECK(xrCreateSession(m_xrInstance, &sessionCI, &m_xrSession), "Failed to create Session.");
    }

    void CreateXrReferenceSpace()
    {
        // Fill out an XrReferenceSpaceCreateInfo structure and create a reference XrSpace, specifying a Local space with an identity pose as the origin.
        XrReferenceSpaceCreateInfo referenceSpaceCI{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        referenceSpaceCI.referenceSpaceType   = XR_REFERENCE_SPACE_TYPE_LOCAL;
        referenceSpaceCI.poseInReferenceSpace = {{0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, 0.0f, 0.0f}};
        OPENXR_CHECK(xrCreateReferenceSpace(m_xrSession, &referenceSpaceCI, &m_LocalSpace), "Failed to create ReferenceSpace.");
    }

    void CreateXrSwapchains()
    {
        uint32_t formatCount = 0;
        OPENXR_CHECK(xrEnumerateSwapchainFormats(m_xrSession, 0, &formatCount, nullptr), "Failed to enumerate Swapchain Formats");
        std::vector<int64_t> formats(formatCount);
        OPENXR_CHECK(xrEnumerateSwapchainFormats(m_xrSession, formatCount, &formatCount, formats.data()), "Failed to enumerate Swapchain Formats");

        // xrEnumerateSwapchainFormats returns an array of API-specific formats ordered by preference
        int64_t NativeColorFormat = 0;
        int64_t NativeDepthFormat = 0;
        for (int64_t NativeFormat : formats)
        {
            const TEXTURE_FORMAT        Format     = GetTextureFormatFromNative(NativeFormat, m_DeviceType);
            const TextureFormatAttribs& FmtAttribs = GetTextureFormatAttribs(Format);
            if (FmtAttribs.IsDepthStencil())
            {
                if (NativeDepthFormat == 0)
                {
                    m_DepthFormat     = Format;
                    NativeDepthFormat = NativeFormat;
                }
            }
            else
            {
                if (NativeColorFormat == 0)
                {
                    m_ColorFormat     = Format;
                    NativeColorFormat = NativeFormat;
                }
            }

            if (NativeColorFormat != 0 && NativeDepthFormat != 0)
                break;
        }

        if (NativeColorFormat == 0)
        {
            LOG_ERROR_AND_THROW("Failed to find a compatible color format for Swapchain");
        }
        if (NativeDepthFormat == 0)
        {
            LOG_ERROR_AND_THROW("Failed to find a compatible depth format for Swapchain");
        }

        //Resize the SwapchainInfo to match the number of view in the View Configuration.
        m_ColorSwapchains.resize(m_ViewConfigurationViews.size());
        m_DepthSwapchains.resize(m_ViewConfigurationViews.size());

        // Per view, create a color and depth swapchain, and their associated image views.
        for (size_t i = 0; i < m_ViewConfigurationViews.size(); i++)
        {
            auto CreateSwapchain = [this](const XrViewConfigurationView& Config, int64_t NativeFormat, TEXTURE_FORMAT Format, bool IsDepth) {
                SwapchainInfo Swapchain;

                XrSwapchainCreateInfo swapchainCI{XR_TYPE_SWAPCHAIN_CREATE_INFO};
                swapchainCI.createFlags = 0;
                swapchainCI.usageFlags =
                    (IsDepth ? XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT) |
                    XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
                swapchainCI.format      = NativeFormat;
                swapchainCI.sampleCount = Config.recommendedSwapchainSampleCount; // Use the recommended values from the XrViewConfigurationView.
                swapchainCI.width       = Config.recommendedImageRectWidth;
                swapchainCI.height      = Config.recommendedImageRectHeight;
                swapchainCI.faceCount   = 1;
                swapchainCI.arraySize   = 1;
                swapchainCI.mipCount    = 1;
                OPENXR_CHECK(xrCreateSwapchain(m_xrSession, &swapchainCI, &Swapchain.Swapchain),
                             (IsDepth ? "Failed to create depth swapchain" : "Failed to create color swapchain"));

                // Get the number of images in the swapchain.
                uint32_t SwapchainImageCount = 0;
                OPENXR_CHECK(xrEnumerateSwapchainImages(Swapchain.Swapchain, 0, &SwapchainImageCount, nullptr), "Failed to enumerate swapchain Images.");
                // Allocate the memory for the swapchain image data.
                RefCntAutoPtr<IDataBlob> pSwapchainImageData;
                AllocateOpenXRSwapchainImageData(m_DeviceType, SwapchainImageCount, &pSwapchainImageData);
                // Get the swapchain image data.
                OPENXR_CHECK(xrEnumerateSwapchainImages(Swapchain.Swapchain, SwapchainImageCount, &SwapchainImageCount,
                                                        pSwapchainImageData->GetDataPtr<XrSwapchainImageBaseHeader>()),
                             "Failed to enumerate swapchain Images.");

                Swapchain.Views.resize(SwapchainImageCount);
                for (uint32_t j = 0; j < SwapchainImageCount; j++)
                {
                    std::string Name = (IsDepth ? "Depth Swapchain Image " : "Color Swapchain Image ") + std::to_string(j);
                    TextureDesc ImgDesc;
                    ImgDesc.Name      = Name.c_str();
                    ImgDesc.Type      = RESOURCE_DIM_TEX_2D;
                    ImgDesc.Format    = Format;
                    ImgDesc.Width     = swapchainCI.width;
                    ImgDesc.Height    = swapchainCI.height;
                    ImgDesc.MipLevels = 1;
                    ImgDesc.BindFlags = (IsDepth ? BIND_DEPTH_STENCIL : BIND_RENDER_TARGET) | BIND_SHADER_RESOURCE;

                    RefCntAutoPtr<ITexture> pImage;
                    GetOpenXRSwapchainImage(m_Device, pSwapchainImageData->GetConstDataPtr<XrSwapchainImageBaseHeader>(), j, ImgDesc, &pImage);

                    TextureViewDesc ViewDesc;
                    ViewDesc.ViewType = IsDepth ? TEXTURE_VIEW_DEPTH_STENCIL : TEXTURE_VIEW_RENDER_TARGET;
                    pImage->CreateView(ViewDesc, &Swapchain.Views[j]);
                    VERIFY_EXPR(Swapchain.Views[j] != nullptr);
                }

                return Swapchain;
            };

            m_ColorSwapchains[i] = CreateSwapchain(m_ViewConfigurationViews[i], NativeColorFormat, m_ColorFormat, false);
            m_DepthSwapchains[i] = CreateSwapchain(m_ViewConfigurationViews[i], NativeDepthFormat, m_DepthFormat, true);
        }
    }

    void DestroyXrSwapchains()
    {
        // Per view in the view configuration:
        for (size_t i = 0; i < m_ViewConfigurationViews.size(); i++)
        {
            SwapchainInfo& ColorSwapchain = m_ColorSwapchains[i];
            SwapchainInfo& DepthSwapchain = m_DepthSwapchains[i];

            ColorSwapchain.Views.clear();
            DepthSwapchain.Views.clear();

            // Destroy the swapchains.
            OPENXR_CHECK(xrDestroySwapchain(ColorSwapchain.Swapchain), "Failed to destroy Color Swapchain");
            OPENXR_CHECK(xrDestroySwapchain(DepthSwapchain.Swapchain), "Failed to destroy Depth Swapchain");
        }
    }

    bool Initialize()
    {
        CreateXrInstance();
        GetXrInstanceProperties();
        GetXrSystemID();
        GetViewConfigurationViews();
        GetEnvironmentBlendModes();

        if (!InitializeDiligentEngine())
            return false;

        CreateXrSession();
        CreateXrReferenceSpace();
        CreateXrSwapchains();

        return true;
    }

    bool InitializeDiligentEngine()
    {
        OpenXRAttribs XRAttribs;
        static_assert(sizeof(XRAttribs.Instance) == sizeof(m_xrInstance), "XrInstance size mismatch");
        memcpy(&XRAttribs.Instance, &m_xrInstance, sizeof(m_xrInstance));
        static_assert(sizeof(XRAttribs.SystemId) == sizeof(m_SystemId), "XrSystemID size mismatch");
        memcpy(&XRAttribs.SystemId, &m_SystemId, sizeof(m_SystemId));
        XRAttribs.GetInstanceProcAddr = xrGetInstanceProcAddr;

        RefCntAutoPtr<IRenderDevice> pDevice;
        switch (m_DeviceType)
        {
#if D3D11_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D11:
            {
                EngineD3D11CreateInfo EngineCI;
                EngineCI.pXRAttribs = &XRAttribs;
#    if ENGINE_DLL
                // Load the dll and import GetEngineFactoryD3D11() function
                auto* GetEngineFactoryD3D11 = LoadGraphicsEngineD3D11();
#    endif
                auto* pFactoryD3D11 = GetEngineFactoryD3D11();
                pFactoryD3D11->CreateDeviceAndContextsD3D11(EngineCI, &pDevice, &m_pImmediateContext);
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
                EngineD3D12CreateInfo EngineCI;
                EngineCI.pXRAttribs = &XRAttribs;

                auto* pFactoryD3D12 = GetEngineFactoryD3D12();
                pFactoryD3D12->CreateDeviceAndContextsD3D12(EngineCI, &pDevice, &m_pImmediateContext);
            }
            break;
#endif


#if GL_SUPPORTED
            case RENDER_DEVICE_TYPE_GL:
            {
#    if 0
#        if EXPLICITLY_LOAD_ENGINE_GL_DLL
                // Load the dll and import GetEngineFactoryOpenGL() function
                auto GetEngineFactoryOpenGL = LoadGraphicsEngineOpenGL();
#        endif
                auto* pFactoryOpenGL = GetEngineFactoryOpenGL();

                EngineGLCreateInfo EngineCI;
                EngineCI.pXRAttribs  = &XRAttribs;
                EngineCI.Window.hWnd = hWnd;

                pFactoryOpenGL->CreateDeviceAndSwapChainGL(EngineCI, &pDevice, &m_pImmediateContext, SCDesc, &m_pSwapChain);
#    endif
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
                EngineCI.pXRAttribs = &XRAttribs;

                auto* pFactoryVk = GetEngineFactoryVk();
                pFactoryVk->CreateDeviceAndContextsVk(EngineCI, &pDevice, &m_pImmediateContext);
            }
            break;
#endif

            default:
                std::cerr << "Unknown/unsupported device type";
                return false;
                break;
        }

        m_Device = pDevice;

        return true;
    }

    bool ProcessCommandLine(const char* CmdLine)
    {
        const char* mode = nullptr;

        const char* Keys[] = {"--mode ", "--mode=", "-m "};
        for (size_t i = 0; i < _countof(Keys); ++i)
        {
            const auto* Key = Keys[i];
            if ((mode = strstr(CmdLine, Key)) != nullptr)
            {
                mode += strlen(Key);
                break;
            }
        }

        if (mode != nullptr)
        {
            while (*mode == ' ')
                ++mode;

            if (_stricmp(mode, "D3D11") == 0)
            {
#if D3D11_SUPPORTED
                m_DeviceType = RENDER_DEVICE_TYPE_D3D11;
#else
                std::cerr << "Direct3D11 is not supported. Please select another device type";
                return false;
#endif
            }
            else if (_stricmp(mode, "D3D12") == 0)
            {
#if D3D12_SUPPORTED
                m_DeviceType = RENDER_DEVICE_TYPE_D3D12;
#else
                std::cerr << "Direct3D12 is not supported. Please select another device type";
                return false;
#endif
            }
            else if (_stricmp(mode, "GL") == 0)
            {
#if GL_SUPPORTED
                m_DeviceType = RENDER_DEVICE_TYPE_GL;
#else
                std::cerr << "OpenGL is not supported. Please select another device type";
                return false;
#endif
            }
            else if (_stricmp(mode, "VK") == 0)
            {
#if VULKAN_SUPPORTED
                m_DeviceType = RENDER_DEVICE_TYPE_VULKAN;
#else
                std::cerr << "Vulkan is not supported. Please select another device type";
                return false;
#endif
            }
            else
            {
                std::cerr << mode << " is not a valid device type. Only the following types are supported: D3D11, D3D12, GL, VK";
                return false;
            }
        }
        else
        {
#if VULKAN_SUPPORTED
            m_DeviceType = RENDER_DEVICE_TYPE_VULKAN;
#elif D3D12_SUPPORTED
            m_DeviceType = RENDER_DEVICE_TYPE_D3D12;
#elif D3D11_SUPPORTED
            m_DeviceType = RENDER_DEVICE_TYPE_D3D11;
#elif GL_SUPPORTED
            m_DeviceType = RENDER_DEVICE_TYPE_GL;
#endif
        }
        return true;
    }

    void CreateResources()
    {
        TexturedCube::VERTEX_COMPONENT_FLAGS CubeVertexComponents =
            TexturedCube::VERTEX_COMPONENT_FLAG_POSITION |
            TexturedCube::VERTEX_COMPONENT_FLAG_NORMAL;
        m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_Device, CubeVertexComponents);
        m_CubeIndexBuffer  = TexturedCube::CreateIndexBuffer(m_Device);

        GraphicsPipelineStateCreateInfoX PSOCreateInfo{"Cube PSO"};
        PSOCreateInfo
            .AddRenderTarget(m_ColorFormat)
            .SetDepthFormat(m_DepthFormat)
            .SetPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

        PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
        // Enable depth testing
        PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.CompileFlags   = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        m_Device.GetEngineFactory()->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

        // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
        constexpr bool UseCombinedTextureSamplers = true;

        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc       = {"Cube VS", SHADER_TYPE_VERTEX, UseCombinedTextureSamplers};
            ShaderCI.EntryPoint = "main";
            ShaderCI.FilePath   = "cube.vsh";

            pVS = m_Device.CreateShader(ShaderCI);
            VERIFY_EXPR(pVS);
        }

        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc       = {"Cube PS", SHADER_TYPE_PIXEL, UseCombinedTextureSamplers};
            ShaderCI.EntryPoint = "main";
            ShaderCI.FilePath   = "cube.psh";

            pPS = m_Device.CreateShader(ShaderCI);
            VERIFY_EXPR(pPS);
        }

        InputLayoutDescX InputLayout{
            // Attribute 0 - vertex position
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            // Attribute 1 - vertex normal
            LayoutElement{1, 0, 3, VT_FLOAT32, False},
        };

        PSOCreateInfo
            .AddShader(pVS)
            .AddShader(pPS)
            .SetInputLayout(InputLayout);

        PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType        = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
        PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableMergeStages = SHADER_TYPE_VS_PS;

        m_PSO = m_Device.CreateGraphicsPipelineState(PSOCreateInfo);
        VERIFY_EXPR(m_PSO);

        m_PSO->CreateShaderResourceBinding(&m_SRB, true);
        VERIFY_EXPR(m_SRB);

        m_Constants = m_Device.CreateBuffer("Constants", sizeof(HLSL::Constants), USAGE_DYNAMIC);
        m_SRB->GetVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_Constants);
    }

    void RenderFrame()
    {
        // Get the XrFrameState for timing and rendering info.
        XrFrameState    frameState{XR_TYPE_FRAME_STATE};
        XrFrameWaitInfo frameWaitInfo{XR_TYPE_FRAME_WAIT_INFO};
        OPENXR_CHECK(xrWaitFrame(m_xrSession, &frameWaitInfo, &frameState), "Failed to wait for XR Frame.");

        // Tell the OpenXR compositor that the application is beginning the frame.
        XrFrameBeginInfo frameBeginInfo{XR_TYPE_FRAME_BEGIN_INFO};
        OPENXR_CHECK(xrBeginFrame(m_xrSession, &frameBeginInfo), "Failed to begin the XR Frame.");

        // Variables for rendering and layer composition.
        RenderLayerInfo renderLayerInfo;
        renderLayerInfo.PredictedDisplayTime = frameState.predictedDisplayTime;

        // Check that the session is active and that we should render.
        bool sessionActive = (m_xrSessionState == XR_SESSION_STATE_SYNCHRONIZED ||
                              m_xrSessionState == XR_SESSION_STATE_VISIBLE ||
                              m_xrSessionState == XR_SESSION_STATE_FOCUSED);
        if (sessionActive && frameState.shouldRender)
        {
            // Render the stereo image and associate one of swapchain images with the XrCompositionLayerProjection structure.
            if (RenderLayer(renderLayerInfo))
            {
                renderLayerInfo.Layers.push_back(reinterpret_cast<XrCompositionLayerBaseHeader*>(&renderLayerInfo.LayerProjection));
            }
        }

        // Tell OpenXR that we are finished with this frame; specifying its display time, environment blending and layers.
        XrFrameEndInfo frameEndInfo{XR_TYPE_FRAME_END_INFO};
        frameEndInfo.displayTime          = frameState.predictedDisplayTime;
        frameEndInfo.environmentBlendMode = m_EnvironmentBlendMode;
        frameEndInfo.layerCount           = static_cast<uint32_t>(renderLayerInfo.Layers.size());
        frameEndInfo.layers               = renderLayerInfo.Layers.data();
        OPENXR_CHECK(xrEndFrame(m_xrSession, &frameEndInfo), "Failed to end the XR Frame.");
    }

private:
    struct RenderLayerInfo;

public:
    bool RenderLayer(RenderLayerInfo& renderLayerInfo)
    {
        // Locate the views from the view configuration within the (reference) space at the display time.
        std::vector<XrView> views(m_ViewConfigurationViews.size(), {XR_TYPE_VIEW});

        XrViewState      viewState{XR_TYPE_VIEW_STATE}; // Will contain information on whether the position and/or orientation is valid and/or tracked.
        XrViewLocateInfo viewLocateInfo{XR_TYPE_VIEW_LOCATE_INFO};
        viewLocateInfo.viewConfigurationType = m_ViewConfiguration;
        viewLocateInfo.displayTime           = renderLayerInfo.PredictedDisplayTime;
        viewLocateInfo.space                 = m_LocalSpace;
        uint32_t viewCount                   = 0;
        if (xrLocateViews(m_xrSession, &viewLocateInfo, &viewState, static_cast<uint32_t>(views.size()), &viewCount, views.data()) != XR_SUCCESS)
        {
            LOG_INFO_MESSAGE("Failed to locate Views.");
            return false;
        }

        // Resize the layer projection views to match the view count. The layer projection views are used in the layer projection.
        renderLayerInfo.LayerProjectionViews.resize(viewCount, {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW});

        // Per view in the view configuration:
        for (uint32_t i = 0; i < viewCount; i++)
        {
            SwapchainInfo& colorSwapchain = m_ColorSwapchains[i];
            SwapchainInfo& depthSwapchain = m_DepthSwapchains[i];

            // Acquire and wait for an image from the swapchains.
            // Get the image index of an image in the swapchains.
            // The timeout is infinite.
            uint32_t                    colorImageIndex = 0;
            uint32_t                    depthImageIndex = 0;
            XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
            OPENXR_CHECK(xrAcquireSwapchainImage(colorSwapchain.Swapchain, &acquireInfo, &colorImageIndex), "Failed to acquire Image from the Color Swapchian");
            OPENXR_CHECK(xrAcquireSwapchainImage(depthSwapchain.Swapchain, &acquireInfo, &depthImageIndex), "Failed to acquire Image from the Depth Swapchian");

            XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
            waitInfo.timeout                  = XR_INFINITE_DURATION;
            OPENXR_CHECK(xrWaitSwapchainImage(colorSwapchain.Swapchain, &waitInfo), "Failed to wait for Image from the Color Swapchain");
            OPENXR_CHECK(xrWaitSwapchainImage(depthSwapchain.Swapchain, &waitInfo), "Failed to wait for Image from the Depth Swapchain");

            // Get the width and height and construct the viewport and scissors.
            const uint32_t width  = m_ViewConfigurationViews[i].recommendedImageRectWidth;
            const uint32_t height = m_ViewConfigurationViews[i].recommendedImageRectHeight;

            // Fill out the XrCompositionLayerProjectionView structure specifying the pose and fov from the view.
            // This also associates the swapchain image with this layer projection view.
            XrCompositionLayerProjectionView& layerProjectionView = renderLayerInfo.LayerProjectionViews[i];
            layerProjectionView                                   = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            layerProjectionView.pose                              = views[i].pose;
            layerProjectionView.fov                               = views[i].fov;
            layerProjectionView.subImage.swapchain                = colorSwapchain.Swapchain;
            layerProjectionView.subImage.imageRect.offset.x       = 0;
            layerProjectionView.subImage.imageRect.offset.y       = 0;
            layerProjectionView.subImage.imageRect.extent.width   = static_cast<int32_t>(width);
            layerProjectionView.subImage.imageRect.extent.height  = static_cast<int32_t>(height);
            layerProjectionView.subImage.imageArrayIndex          = 0; // Useful for multiview rendering.

            ITextureView* pRTV = colorSwapchain.Views[colorImageIndex];
            ITextureView* pDSV = depthSwapchain.Views[depthImageIndex];

            // Swap chain images acquired by xrAcquireSwapchainImage are guaranteed to be in
            // COLOR_ATTACHMENT_OPTIMAL/DEPTH_STENCIL_ATTACHMENT_OPTIMAL state.
            pRTV->GetTexture()->SetState(RESOURCE_STATE_RENDER_TARGET);
            pDSV->GetTexture()->SetState(RESOURCE_STATE_DEPTH_WRITE);

            m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            float4 Gray{0.17f, 0.17f, 0.17f, 1.00f};
            float4 Black{0.00f, 0.00f, 0.00f, 1.00f};
            m_pImmediateContext->ClearRenderTarget(pRTV, m_EnvironmentBlendMode == XR_ENVIRONMENT_BLEND_MODE_OPAQUE ? Gray.Data() : Black.Data(),
                                                   RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // Compute the view-projection transform.
            // All matrices (including OpenXR's) are column-major, right-handed.
            const float NearZ             = 0.05f;
            const float FarZ              = 100.0f;
            const bool  NegativeOneToOneZ = m_DeviceType == RENDER_DEVICE_TYPE_GL || m_DeviceType == RENDER_DEVICE_TYPE_GLES;
            float4x4    CameraProj        = float4x4::Projection(PI_F / 2.f, 1.0, NearZ, FarZ, NegativeOneToOneZ);

            const XrQuaternionf& orientation = views[i].pose.orientation;
            const XrVector3f&    position    = views[i].pose.position;

            float4x4 CameraWorld =
                QuaternionF{orientation.x, orientation.y, orientation.z, orientation.w}.ToMatrix() *
                float4x4::Translation(-position.x, -position.y, -position.z);

            float4x4 CameraView     = CameraWorld.Inverse();
            float4x4 CameraViewProj = CameraView * CameraProj;

            IBuffer* pVBs[] = {m_CubeVertexBuffer};
            m_pImmediateContext->SetVertexBuffers(0, _countof(pVBs), pVBs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            m_pImmediateContext->SetPipelineState(m_PSO);
            m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // Draw a floor. Scale it by 2 in the X and Z, and 0.1 in the Y,
            RenderCuboid({0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, -m_ViewHeightM, 0.0f}, {2.0f, 0.1f, 2.0f}, {0.4f, 0.5f, 0.5f}, CameraViewProj);
            // Draw a "table".
            RenderCuboid({0.0f, 0.0f, 0.0f, 1.0f}, {0.0f, -m_ViewHeightM + 0.9f, -0.7f}, {1.0f, 0.2f, 1.0f}, {0.6f, 0.6f, 0.4f}, CameraViewProj);

            m_pImmediateContext->Flush();
            m_pImmediateContext->FinishFrame();
            m_Device.ReleaseStaleResources();

            // Swap chain images must be in COLOR_ATTACHMENT_OPTIMAL/DEPTH_STENCIL_ATTACHMENT_OPTIMAL state
            // when they are released by xrReleaseSwapchainImage.

            // Give the swapchain image back to OpenXR, allowing the compositor to use the image.
            XrSwapchainImageReleaseInfo releaseInfo{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
            OPENXR_CHECK(xrReleaseSwapchainImage(colorSwapchain.Swapchain, &releaseInfo), "Failed to release Image back to the Color Swapchain");
            OPENXR_CHECK(xrReleaseSwapchainImage(depthSwapchain.Swapchain, &releaseInfo), "Failed to release Image back to the Depth Swapchain");
        }

        // Fill out the XrCompositionLayerProjection structure for usage with xrEndFrame().
        renderLayerInfo.LayerProjection.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT | XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
        renderLayerInfo.LayerProjection.space      = m_LocalSpace;
        renderLayerInfo.LayerProjection.viewCount  = static_cast<uint32_t>(renderLayerInfo.LayerProjectionViews.size());
        renderLayerInfo.LayerProjection.views      = renderLayerInfo.LayerProjectionViews.data();

        return true;
    }

    void RenderCuboid(QuaternionF rotation, float3 position, float3 scale, float3 color, const float4x4& CameraViewProj)
    {
        {
            MapHelper<HLSL::Constants> CBConstants{m_pImmediateContext, m_Constants, MAP_WRITE, MAP_FLAG_DISCARD};
            CBConstants->WorldViewProj = float4x4::Scale(scale * 0.5) * float4x4::Translation(position) * CameraViewProj;
            CBConstants->Model         = float4x4::Identity();
            CBConstants->Color         = float4{color.x, color.y, color.z, 1.0f};
        }

        m_pImmediateContext->DrawIndexed({36, VT_UINT32, DRAW_FLAG_VERIFY_ALL});
    }

    void PollSystemEvents()
    {
    }

    void PollEvents()
    {
        // Poll OpenXR for a new event.
        XrEventDataBuffer eventData{XR_TYPE_EVENT_DATA_BUFFER};
        auto              XrPollEvents = [&]() -> bool {
            eventData = {XR_TYPE_EVENT_DATA_BUFFER};
            return xrPollEvent(m_xrInstance, &eventData) == XR_SUCCESS;
        };

        while (XrPollEvents())
        {
            switch (eventData.type)
            {
                // Log the number of lost events from the runtime.
                case XR_TYPE_EVENT_DATA_EVENTS_LOST:
                {
                    XrEventDataEventsLost* eventsLost = reinterpret_cast<XrEventDataEventsLost*>(&eventData);
                    LOG_INFO_MESSAGE("OPENXR: Events Lost: ", eventsLost->lostEventCount);
                    break;
                }
                // Log that an instance loss is pending and shutdown the application.
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                {
                    XrEventDataInstanceLossPending* instanceLossPending = reinterpret_cast<XrEventDataInstanceLossPending*>(&eventData);
                    LOG_INFO_MESSAGE("OPENXR: Instance Loss Pending at: ", instanceLossPending->lossTime);
                    m_xrSessionRunning   = false;
                    m_ApplicationRunning = false;
                    break;
                }
                // Log that the interaction profile has changed.
                case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                {
                    XrEventDataInteractionProfileChanged* interactionProfileChanged = reinterpret_cast<XrEventDataInteractionProfileChanged*>(&eventData);
                    LOG_INFO_MESSAGE("OPENXR: Interaction Profile changed for Session: ", interactionProfileChanged->session);
                    if (interactionProfileChanged->session != m_xrSession)
                    {
                        LOG_INFO_MESSAGE("XrEventDataInteractionProfileChanged for unknown Session");
                        break;
                    }
                    break;
                }
                // Log that there's a reference space change pending.
                case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
                {
                    XrEventDataReferenceSpaceChangePending* referenceSpaceChangePending = reinterpret_cast<XrEventDataReferenceSpaceChangePending*>(&eventData);
                    LOG_INFO_MESSAGE("OPENXR: Reference Space Change pending for Session: ", referenceSpaceChangePending->session);
                    if (referenceSpaceChangePending->session != m_xrSession)
                    {
                        LOG_INFO_MESSAGE("XrEventDataReferenceSpaceChangePending for unknown Session");
                        break;
                    }
                    break;
                }
                // Session State changes:
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED:
                {
                    XrEventDataSessionStateChanged* sessionStateChanged = reinterpret_cast<XrEventDataSessionStateChanged*>(&eventData);
                    if (sessionStateChanged->session != m_xrSession)
                    {
                        LOG_INFO_MESSAGE("XrEventDataSessionStateChanged for unknown Session");
                        break;
                    }

                    if (sessionStateChanged->state == XR_SESSION_STATE_READY)
                    {
                        // SessionState is ready. Begin the XrSession using the XrViewConfigurationType.
                        XrSessionBeginInfo sessionBeginInfo{XR_TYPE_SESSION_BEGIN_INFO};
                        sessionBeginInfo.primaryViewConfigurationType = m_ViewConfiguration;
                        OPENXR_CHECK(xrBeginSession(m_xrSession, &sessionBeginInfo), "Failed to begin Session.");
                        m_xrSessionRunning = true;
                    }
                    if (sessionStateChanged->state == XR_SESSION_STATE_STOPPING)
                    {
                        // SessionState is stopping. End the XrSession.
                        OPENXR_CHECK(xrEndSession(m_xrSession), "Failed to end Session.");
                        m_xrSessionRunning = false;
                    }
                    if (sessionStateChanged->state == XR_SESSION_STATE_EXITING)
                    {
                        // SessionState is exiting. Exit the application.
                        m_xrSessionRunning   = false;
                        m_ApplicationRunning = false;
                    }
                    if (sessionStateChanged->state == XR_SESSION_STATE_LOSS_PENDING)
                    {
                        // SessionState is loss pending. Exit the application.
                        // It's possible to try a reestablish an XrInstance and XrSession, but we will simply exit here.
                        m_xrSessionRunning   = false;
                        m_ApplicationRunning = false;
                    }
                    // Store state for reference across the application.
                    m_xrSessionState = sessionStateChanged->state;
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
    }

    RENDER_DEVICE_TYPE GetDeviceType() const { return m_DeviceType; }
    bool               IsRunning() const { return m_ApplicationRunning; }
    bool               IsSessionRunning() const { return m_xrSessionRunning; }

private:
    RenderDeviceX_N               m_Device;
    RefCntAutoPtr<IDeviceContext> m_pImmediateContext;
    RefCntAutoPtr<ISwapChain>     m_pSwapChain;
    RENDER_DEVICE_TYPE            m_DeviceType = RENDER_DEVICE_TYPE_D3D11;

    XrInstance               m_xrInstance      = XR_NULL_HANDLE;
    std::vector<const char*> m_ActiveAPILayers = {};
    std::vector<std::string> m_ApiLayers       = {};

    XrDebugUtilsMessengerEXT m_DebugUtilsMessenger = XR_NULL_HANDLE;

    XrFormFactor       m_FormFactor       = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId         m_SystemId         = {};
    XrSystemProperties m_SystemProperties = {XR_TYPE_SYSTEM_PROPERTIES};

    XrSession      m_xrSession          = {};
    XrSessionState m_xrSessionState     = XR_SESSION_STATE_UNKNOWN;
    bool           m_ApplicationRunning = true;
    bool           m_xrSessionRunning   = false;

    std::vector<XrViewConfigurationType> m_ApplicationViewConfigurations = {XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO};
    XrViewConfigurationType              m_ViewConfiguration             = XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM;
    std::vector<XrViewConfigurationView> m_ViewConfigurationViews;

    TEXTURE_FORMAT m_ColorFormat = TEX_FORMAT_UNKNOWN;
    TEXTURE_FORMAT m_DepthFormat = TEX_FORMAT_UNKNOWN;
    struct SwapchainInfo
    {
        XrSwapchain                              Swapchain = XR_NULL_HANDLE;
        std::vector<RefCntAutoPtr<ITextureView>> Views;
    };
    std::vector<SwapchainInfo> m_ColorSwapchains = {};
    std::vector<SwapchainInfo> m_DepthSwapchains = {};

    XrEnvironmentBlendMode m_EnvironmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_MAX_ENUM;

    XrSpace m_LocalSpace = XR_NULL_HANDLE;
    struct RenderLayerInfo
    {
        XrTime                                        PredictedDisplayTime = 0;
        std::vector<XrCompositionLayerBaseHeader*>    Layers;
        XrCompositionLayerProjection                  LayerProjection = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        std::vector<XrCompositionLayerProjectionView> LayerProjectionViews;
    };

    // In STAGE space, viewHeightM should be 0. In LOCAL space, it should be offset downwards, below the viewer's initial position.
    float m_ViewHeightM = 1.5f;

    RefCntAutoPtr<IBuffer>                m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer>                m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer>                m_Constants;
    RefCntAutoPtr<IPipelineState>         m_PSO;
    RefCntAutoPtr<IShaderResourceBinding> m_SRB;
};

std::unique_ptr<Tutorial28_HelloOpenXR> g_pTheApp;

LRESULT CALLBACK MessageProc(HWND, UINT, WPARAM, LPARAM);
// Main
int WINAPI WinMain(_In_ HINSTANCE     hInstance,
                   _In_opt_ HINSTANCE hPrevInstance,
                   _In_ LPSTR         lpCmdLine,
                   _In_ int           nShowCmd)
{
#if defined(_DEBUG) || defined(DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    g_pTheApp.reset(new Tutorial28_HelloOpenXR);

    const auto* cmdLine = GetCommandLineA();
    if (!g_pTheApp->ProcessCommandLine(cmdLine))
        return -1;

    try
    {
        if (!g_pTheApp->Initialize())
            return -1;
    }
    catch (...)
    {
        return -1;
    }

    g_pTheApp->CreateResources();

    // Main loop
    while (g_pTheApp->IsRunning())
    {
        g_pTheApp->PollSystemEvents();
        g_pTheApp->PollEvents();
        if (g_pTheApp->IsSessionRunning())
        {
            g_pTheApp->RenderFrame();
        }
    }

    g_pTheApp.reset();

    return 0;
}
