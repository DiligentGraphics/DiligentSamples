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
        GetOpenXRGraphicsBinding(m_pDevice, m_pImmediateContext, &pGraphicsBinding);

        XrSessionCreateInfo sessionCI{XR_TYPE_SESSION_CREATE_INFO};
        sessionCI.next        = pGraphicsBinding->GetConstDataPtr();
        sessionCI.createFlags = 0;
        sessionCI.systemId    = m_SystemId;

        OPENXR_CHECK(xrCreateSession(m_xrInstance, &sessionCI, &m_xrSession), "Failed to create Session.");
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
                pFactoryD3D11->CreateDeviceAndContextsD3D11(EngineCI, &m_pDevice, &m_pImmediateContext);
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
                pFactoryD3D12->CreateDeviceAndContextsD3D12(EngineCI, &m_pDevice, &m_pImmediateContext);
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

                pFactoryOpenGL->CreateDeviceAndSwapChainGL(EngineCI, &m_pDevice, &m_pImmediateContext, SCDesc, &m_pSwapChain);
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
                pFactoryVk->CreateDeviceAndContextsVk(EngineCI, &m_pDevice, &m_pImmediateContext);
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
    }

    void Present()
    {
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
                    m_SessionRunning     = false;
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
                        m_SessionRunning = true;
                    }
                    if (sessionStateChanged->state == XR_SESSION_STATE_STOPPING)
                    {
                        // SessionState is stopping. End the XrSession.
                        OPENXR_CHECK(xrEndSession(m_xrSession), "Failed to end Session.");
                        m_SessionRunning = false;
                    }
                    if (sessionStateChanged->state == XR_SESSION_STATE_EXITING)
                    {
                        // SessionState is exiting. Exit the application.
                        m_SessionRunning     = false;
                        m_ApplicationRunning = false;
                    }
                    if (sessionStateChanged->state == XR_SESSION_STATE_LOSS_PENDING)
                    {
                        // SessionState is loss pending. Exit the application.
                        // It's possible to try a reestablish an XrInstance and XrSession, but we will simply exit here.
                        m_SessionRunning     = false;
                        m_ApplicationRunning = false;
                    }
                    // Store state for reference across the application.
                    m_SessionState = sessionStateChanged->state;
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
    bool               IsSessionRunning() const { return m_SessionRunning; }

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
    XrSystemId         m_SystemId         = {};
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
            g_pTheApp->Render();
        }
    }

    g_pTheApp.reset();

    return 0;
}
