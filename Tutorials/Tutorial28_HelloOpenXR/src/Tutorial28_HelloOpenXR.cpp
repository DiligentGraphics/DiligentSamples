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
#include <Windows.h>
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

XrBool32 OpenXRMessageCallbackFunction(XrDebugUtilsMessageSeverityFlagsEXT xrMessageSeverity, XrDebugUtilsMessageTypeFlagsEXT messageType, const XrDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    DEBUG_MESSAGE_SEVERITY MessageSeverity = DEBUG_MESSAGE_SEVERITY_INFO;
    if (xrMessageSeverity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    {
        MessageSeverity = DEBUG_MESSAGE_SEVERITY_ERROR;
    }
    else if (xrMessageSeverity & XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    {
        MessageSeverity = DEBUG_MESSAGE_SEVERITY_WARNING;
    }

    std::string messageTypeStr;
    if (messageType & XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT)
    {
        messageTypeStr += "GEN";
    }
    if (messageType & XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT)
    {
        if (!messageTypeStr.empty())
        {
            messageTypeStr += ",";
        }
        messageTypeStr += "SPEC";
    }
    if (messageType & XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT)
    {
        if (!messageTypeStr.empty())
        {
            messageTypeStr += ",";
        }
        messageTypeStr += "PERF";
    }

    std::string functionName = (pCallbackData->functionName) ? pCallbackData->functionName : "";
    std::string messageId    = (pCallbackData->messageId) ? pCallbackData->messageId : "";
    std::string message      = (pCallbackData->message) ? pCallbackData->message : "";

    LOG_DEBUG_MESSAGE(MessageSeverity, '[', messageTypeStr, "] ", functionName, messageId, " - ", message);

    return XrBool32{};
}

XrDebugUtilsMessengerEXT CreateOpenXRDebugUtilsMessenger(XrInstance m_xrInstance)
{
    // Fill out a XrDebugUtilsMessengerCreateInfoEXT structure specifying all severities and types.
    // Set the userCallback to OpenXRMessageCallbackFunction().
    XrDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI{XR_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debugUtilsMessengerCI.messageSeverities = XR_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugUtilsMessengerCI.messageTypes      = XR_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | XR_DEBUG_UTILS_MESSAGE_TYPE_CONFORMANCE_BIT_EXT;
    debugUtilsMessengerCI.userCallback      = (PFN_xrDebugUtilsMessengerCallbackEXT)OpenXRMessageCallbackFunction;
    debugUtilsMessengerCI.userData          = nullptr;

    // Load xrCreateDebugUtilsMessengerEXT() function pointer as it is not default loaded by the OpenXR loader.
    XrDebugUtilsMessengerEXT           debugUtilsMessenger{};
    PFN_xrCreateDebugUtilsMessengerEXT xrCreateDebugUtilsMessengerEXT;
    OPENXR_CHECK(xrGetInstanceProcAddr(m_xrInstance, "xrCreateDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)&xrCreateDebugUtilsMessengerEXT), "Failed to get InstanceProcAddr.");
    if (xrCreateDebugUtilsMessengerEXT)
    {
        // Finally create and return the XrDebugUtilsMessengerEXT.
        OPENXR_CHECK(xrCreateDebugUtilsMessengerEXT(m_xrInstance, &debugUtilsMessengerCI, &debugUtilsMessenger), "Failed to create DebugUtilsMessenger.");
    }
    return debugUtilsMessenger;
}

void DestroyOpenXRDebugUtilsMessenger(XrInstance m_xrInstance, XrDebugUtilsMessengerEXT debugUtilsMessenger)
{
    // Load xrDestroyDebugUtilsMessengerEXT() function pointer as it is not default loaded by the OpenXR loader.
    PFN_xrDestroyDebugUtilsMessengerEXT xrDestroyDebugUtilsMessengerEXT = nullptr;
    OPENXR_CHECK(xrGetInstanceProcAddr(m_xrInstance, "xrDestroyDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)&xrDestroyDebugUtilsMessengerEXT), "Failed to get InstanceProcAddr.");

    if (xrDestroyDebugUtilsMessengerEXT)
    {
        // Destroy the provided XrDebugUtilsMessengerEXT.
        OPENXR_CHECK(xrDestroyDebugUtilsMessengerEXT(debugUtilsMessenger), "Failed to destroy DebugUtilsMessenger.");
    }
}

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
        case RENDER_DEVICE_TYPE_VULKAN: return "XR_KHR_vulkan_enable";
#endif

        default:
            UNEXPECTED("Unknown device type");
            return nullptr;
    }
}

class Tutorial28_HelloOpenXR
{
public:
    Tutorial28_HelloOpenXR()
    {
    }

    ~Tutorial28_HelloOpenXR()
    {
        m_pImmediateContext->Flush();

        if (m_xrSession)
        {
            OPENXR_CHECK(xrDestroySession(m_xrSession), "Failed to destroy Session.");
        }
        if (m_DebugUtilsMessenger != XR_NULL_HANDLE)
        {
            DestroyOpenXRDebugUtilsMessenger(m_xrInstance, m_DebugUtilsMessenger);
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
            m_DebugUtilsMessenger = CreateOpenXRDebugUtilsMessenger(m_xrInstance);
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
        OPENXR_CHECK(xrGetSystem(m_xrInstance, &systemGI, &m_SystemID), "Failed to get SystemID.");

        // Get the System's properties for some general information about the hardware and the vendor.
        OPENXR_CHECK(xrGetSystemProperties(m_xrInstance, m_SystemID, &m_SystemProperties), "Failed to get SystemProperties.");
    }

    void GetViewConfigurationViews()
    {
        // Gets the View Configuration Types. The first call gets the count of the array that will be returned. The next call fills out the array.
        uint32_t viewConfigurationCount = 0;
        OPENXR_CHECK(xrEnumerateViewConfigurations(m_xrInstance, m_SystemID, 0, &viewConfigurationCount, nullptr), "Failed to enumerate View Configurations.");
        std::vector<XrViewConfigurationType> ViewConfigurations(viewConfigurationCount);
        OPENXR_CHECK(xrEnumerateViewConfigurations(m_xrInstance, m_SystemID, viewConfigurationCount, &viewConfigurationCount, ViewConfigurations.data()), "Failed to enumerate View Configurations.");

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
        OPENXR_CHECK(xrEnumerateViewConfigurationViews(m_xrInstance, m_SystemID, m_ViewConfiguration, 0, &viewConfigurationViewCount, nullptr), "Failed to enumerate ViewConfiguration Views.");
        m_ViewConfigurationViews.resize(viewConfigurationViewCount, {XR_TYPE_VIEW_CONFIGURATION_VIEW});
        OPENXR_CHECK(xrEnumerateViewConfigurationViews(m_xrInstance, m_SystemID, m_ViewConfiguration, viewConfigurationViewCount, &viewConfigurationViewCount, m_ViewConfigurationViews.data()), "Failed to enumerate ViewConfiguration Views.");
    }

    void GetEnvironmentBlendModes()
    {
        // Retrieve the available blend modes. The first call gets the count of the array that will be returned. The next call fills out the array.
        uint32_t environmentBlendModeCount = 0;
        OPENXR_CHECK(xrEnumerateEnvironmentBlendModes(m_xrInstance, m_SystemID, m_ViewConfiguration, 0, &environmentBlendModeCount, nullptr), "Failed to enumerate EnvironmentBlend Modes.");
        std::vector<XrEnvironmentBlendMode> EnvironmentBlendModes(environmentBlendModeCount);
        OPENXR_CHECK(xrEnumerateEnvironmentBlendModes(m_xrInstance, m_SystemID, m_ViewConfiguration, environmentBlendModeCount, &environmentBlendModeCount, EnvironmentBlendModes.data()), "Failed to enumerate EnvironmentBlend Modes.");

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

    bool InitializeDiligentEngine(HWND hWnd)
    {
        CreateXrInstance();
        GetXrInstanceProperties();
        GetXrSystemID();
        GetViewConfigurationViews();
        GetEnvironmentBlendModes();

        SwapChainDesc SCDesc;
        switch (m_DeviceType)
        {
#if D3D11_SUPPORTED
            case RENDER_DEVICE_TYPE_D3D11:
            {
                EngineD3D11CreateInfo EngineCI;
#    if ENGINE_DLL
                // Load the dll and import GetEngineFactoryD3D11() function
                auto* GetEngineFactoryD3D11 = LoadGraphicsEngineD3D11();
#    endif
                auto* pFactoryD3D11 = GetEngineFactoryD3D11();
                pFactoryD3D11->CreateDeviceAndContextsD3D11(EngineCI, &m_pDevice, &m_pImmediateContext);
                Win32NativeWindow Window{hWnd};
                pFactoryD3D11->CreateSwapChainD3D11(m_pDevice, m_pImmediateContext, SCDesc, FullScreenModeDesc{}, Window, &m_pSwapChain);
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

                auto* pFactoryD3D12 = GetEngineFactoryD3D12();
                pFactoryD3D12->CreateDeviceAndContextsD3D12(EngineCI, &m_pDevice, &m_pImmediateContext);
                Win32NativeWindow Window{hWnd};
                pFactoryD3D12->CreateSwapChainD3D12(m_pDevice, m_pImmediateContext, SCDesc, FullScreenModeDesc{}, Window, &m_pSwapChain);
            }
            break;
#endif


#if GL_SUPPORTED
            case RENDER_DEVICE_TYPE_GL:
            {
#    if EXPLICITLY_LOAD_ENGINE_GL_DLL
                // Load the dll and import GetEngineFactoryOpenGL() function
                auto GetEngineFactoryOpenGL = LoadGraphicsEngineOpenGL();
#    endif
                auto* pFactoryOpenGL = GetEngineFactoryOpenGL();

                EngineGLCreateInfo EngineCI;
                EngineCI.Window.hWnd = hWnd;

                pFactoryOpenGL->CreateDeviceAndSwapChainGL(EngineCI, &m_pDevice, &m_pImmediateContext, SCDesc, &m_pSwapChain);
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

                auto* pFactoryVk = GetEngineFactoryVk();
                pFactoryVk->CreateDeviceAndContextsVk(EngineCI, &m_pDevice, &m_pImmediateContext);

                if (!m_pSwapChain && hWnd != nullptr)
                {
                    Win32NativeWindow Window{hWnd};
                    pFactoryVk->CreateSwapChainVk(m_pDevice, m_pImmediateContext, SCDesc, Window, &m_pSwapChain);
                }
            }
            break;
#endif


            default:
                std::cerr << "Unknown/unsupported device type";
                return false;
                break;
        }

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
#if D3D12_SUPPORTED
            m_DeviceType = RENDER_DEVICE_TYPE_D3D12;
#elif VULKAN_SUPPORTED
            m_DeviceType = RENDER_DEVICE_TYPE_VULKAN;
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
    }

    void Render()
    {
        // Set render targets before issuing any draw command.
        // Note that Present() unbinds the back buffer if it is set as render target.
        auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
        m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Clear the back buffer
        const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
        // Let the engine perform required state transitions
        m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    void Present()
    {
        m_pSwapChain->Present();
    }

    void WindowResize(Uint32 Width, Uint32 Height)
    {
        if (m_pSwapChain)
            m_pSwapChain->Resize(Width, Height);
    }

    RENDER_DEVICE_TYPE GetDeviceType() const { return m_DeviceType; }

private:
    RefCntAutoPtr<IRenderDevice>  m_pDevice;
    RefCntAutoPtr<IDeviceContext> m_pImmediateContext;
    RefCntAutoPtr<ISwapChain>     m_pSwapChain;
    RENDER_DEVICE_TYPE            m_DeviceType = RENDER_DEVICE_TYPE_D3D11;

    XrInstance               m_xrInstance      = XR_NULL_HANDLE;
    std::vector<const char*> m_ActiveAPILayers = {};
    std::vector<std::string> m_ApiLayers       = {};

    XrDebugUtilsMessengerEXT m_DebugUtilsMessenger = XR_NULL_HANDLE;

    XrFormFactor       m_FormFactor       = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
    XrSystemId         m_SystemID         = {};
    XrSystemProperties m_SystemProperties = {XR_TYPE_SYSTEM_PROPERTIES};

    XrSession      m_xrSession          = {};
    XrSessionState m_SessionState       = XR_SESSION_STATE_UNKNOWN;
    bool           m_ApplicationRunning = true;
    bool           m_SessionRunning     = false;

    std::vector<XrViewConfigurationType> m_ApplicationViewConfigurations = {XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO, XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO};
    XrViewConfigurationType              m_ViewConfiguration             = XR_VIEW_CONFIGURATION_TYPE_MAX_ENUM;
    std::vector<XrViewConfigurationView> m_ViewConfigurationViews;

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

    std::wstring Title(L"Tutorial00: Hello Win32");
    switch (g_pTheApp->GetDeviceType())
    {
        case RENDER_DEVICE_TYPE_D3D11: Title.append(L" (D3D11)"); break;
        case RENDER_DEVICE_TYPE_D3D12: Title.append(L" (D3D12)"); break;
        case RENDER_DEVICE_TYPE_GL: Title.append(L" (GL)"); break;
        case RENDER_DEVICE_TYPE_VULKAN: Title.append(L" (VK)"); break;
    }
    // Register our window class
    WNDCLASSEX wcex = {sizeof(WNDCLASSEX), CS_HREDRAW | CS_VREDRAW, MessageProc,
                       0L, 0L, hInstance, NULL, NULL, NULL, NULL, L"SampleApp", NULL};
    RegisterClassEx(&wcex);

    // Create a window
    LONG WindowWidth  = 1280;
    LONG WindowHeight = 1024;
    RECT rc           = {0, 0, WindowWidth, WindowHeight};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);
    HWND wnd = CreateWindow(L"SampleApp", Title.c_str(),
                            WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                            rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance, NULL);
    if (!wnd)
    {
        MessageBox(NULL, L"Cannot create window", L"Error", MB_OK | MB_ICONERROR);
        return 0;
    }
    ShowWindow(wnd, nShowCmd);
    UpdateWindow(wnd);

    try
    {
        if (!g_pTheApp->InitializeDiligentEngine(wnd))
            return -1;
    }
    catch (...)
    {
        return -1;
    }

    g_pTheApp->CreateResources();

    // Main message loop
    MSG msg = {0};
    while (WM_QUIT != msg.message)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            g_pTheApp->Render();
            g_pTheApp->Present();
        }
    }

    g_pTheApp.reset();

    return (int)msg.wParam;
}

// Called every time the NativeNativeAppBase receives a message
LRESULT CALLBACK MessageProc(HWND wnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
        case WM_PAINT:
        {
            PAINTSTRUCT ps;
            BeginPaint(wnd, &ps);
            EndPaint(wnd, &ps);
            return 0;
        }
        case WM_SIZE: // Window size has been changed
            if (g_pTheApp)
            {
                g_pTheApp->WindowResize(LOWORD(lParam), HIWORD(lParam));
            }
            return 0;

        case WM_CHAR:
            if (wParam == VK_ESCAPE)
                PostQuitMessage(0);
            return 0;

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_GETMINMAXINFO:
        {
            LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;

            lpMMI->ptMinTrackSize.x = 320;
            lpMMI->ptMinTrackSize.y = 240;
            return 0;
        }

        default:
            return DefWindowProc(wnd, message, wParam, lParam);
    }
}
