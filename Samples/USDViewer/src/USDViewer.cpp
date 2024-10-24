/*
 *  Copyright 2023-2024 Diligent Graphics LLC
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

#include "USDViewer.hpp"

#include <array>

#include "HnRenderBuffer.hpp"
#include "CommandLineParser.hpp"
#include "GraphicsUtilities.h"
#include "MapHelper.hpp"
#include "TextureUtilities.h"
#include "AdvancedMath.hpp"
#include "PBR_Renderer.hpp"
#include "FileSystem.hpp"
#include "Timer.hpp"
#include "RenderStateCache.h"
#include "ThreadPool.hpp"

#include "Tasks/HnReadRprimIdTask.hpp"
#include "Tasks/HnRenderBoundBoxTask.hpp"
#include "HnTokens.hpp"
#include "HnCamera.hpp"
#include "HnLight.hpp"
#include "GfTypeConversions.hpp"
#include "ScopedDebugGroup.hpp"
#include "ScreenSpaceReflection.hpp"
#include "ScreenSpaceAmbientOcclusion.hpp"
#include "TemporalAntiAliasing.hpp"
#include "ToneMapping.hpp"

#include "imgui.h"
#include "ImGuiUtils.hpp"
#include "ImGuizmo.h"

#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/property.h"
#include "pxr/usd/usdGeom/metrics.h"
#include "pxr/usd/usdGeom/camera.h"
#include "pxr/usd/usdGeom/imageable.h"
#include "pxr/usd/usdGeom/tokens.h"
#include "pxr/usd/usdLux/distantLight.h"
#include "pxr/usd/usdLux/sphereLight.h"
#include "pxr/usd/usdLux/domeLight.h"
#include "pxr/usd/usdLux/shadowAPI.h"
#include "pxr/base/gf/rotation.h"


namespace Diligent
{

namespace HLSL
{

namespace
{
#include "Shaders/Common/public/BasicStructures.fxh"
#include "Shaders/PostProcess/ToneMapping/public/ToneMappingStructures.fxh"
} // namespace

} // namespace HLSL


// clang-format off
const std::pair<const char*, const char*> DefaultUSDModels[] =
{
    {"Apple Vision Pro", "usd/AppleVisionPro.usdz"},
    {"Carbon Frame Bike", "usd/CarbonBike.usdz"},
    {"Kitchen", "usd/Kitchen.usd"},
    {"Porsche", "usd/Porsche.usdz"},
    {"Cube", "cube.usd"}
};
// clang-format on

SampleBase* CreateSample()
{
    return new USDViewer();
}

void USDViewer::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);
    // We do not need the swap chain's depth buffer.
    Attribs.SCDesc.DepthBufferFormat = TEX_FORMAT_UNKNOWN;

#if VULKAN_SUPPORTED
    if (Attribs.DeviceType == RENDER_DEVICE_TYPE_VULKAN)
    {
        EngineVkCreateInfo& EngineVkCI = static_cast<EngineVkCreateInfo&>(Attribs.EngineCI);
        EngineVkCI.DynamicHeapSize     = 16 << 20;
    }
#endif

#if WEBGPU_SUPPORTED
    if (Attribs.DeviceType == RENDER_DEVICE_TYPE_WEBGPU)
    {
        EngineWebGPUCreateInfo& EngineWgpuCI = static_cast<EngineWebGPUCreateInfo&>(Attribs.EngineCI);
        EngineWgpuCI.DynamicHeapSize         = 16 << 20;
    }
#endif
}

void USDViewer::UpdateModelsList(const std::string& Dir)
{
    m_Models.clear();
    for (size_t i = 0; i < _countof(DefaultUSDModels); ++i)
    {
        m_Models.push_back(ModelInfo{DefaultUSDModels[i].first, DefaultUSDModels[i].second});
    }

#if PLATFORM_WIN32 || PLATFORM_LINUX || PLATFORM_MACOS
    if (!Dir.empty())
    {
        auto SearchRes = FileSystem::SearchRecursive(Dir.c_str(), "*.usd*");
        for (const auto& File : SearchRes)
        {
            m_Models.push_back(ModelInfo{File.Name, Dir + FileSystem::SlashSymbol + File.Name});
        }
    }
#endif
}

SampleBase::CommandLineStatus USDViewer::ProcessCommandLine(int argc, const char* const* argv)
{
    CommandLineParser ArgsParser{argc, argv};
    ArgsParser.Parse("usd_path", 'u', m_UsdFileName);
    ArgsParser.Parse("vertex_pool", m_UseVertexPool);
    ArgsParser.Parse("index_pool", m_UseIndexPool);
    ArgsParser.Parse("atlas_dim", m_TextureAtlasDim);
    ArgsParser.Parse("texture_compress_mode", m_TextureCompressMode);
    ArgsParser.Parse("shader_cache", m_EnableShaderCache);
    ArgsParser.Parse("async_texture_loading", m_AsyncTextureLoading);
    LOG_INFO_MESSAGE("USD Viewer Arguments:",
                     "\n    USD Path:        ", m_UsdFileName,
                     "\n    Use vertex pool: ", m_UseVertexPool ? "Yes" : "No",
                     "\n    Use index pool:  ", m_UseIndexPool ? "Yes" : "No",
                     "\n    Tex atlas dim:   ", m_TextureAtlasDim,
                     "\n    Shader Cache:    ", m_EnableShaderCache ? "Yes" : "No",
                     "\n    Tex compression: ", m_TextureCompressMode,
                     "\n    Async tex load:  ", m_AsyncTextureLoading ? "Yes" : "No");

    std::string ModelsDir;
    ArgsParser.Parse("usd_dir", 'd', ModelsDir);
    UpdateModelsList(ModelsDir);

#ifdef DILIGENT_DEVELOPMENT
    m_EnableHotShaderReload = true;
#endif
    ArgsParser.Parse("shader_reload", 'r', m_EnableHotShaderReload);

    return CommandLineStatus::OK;
}

void USDViewer::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    if (m_EnableShaderCache)
    {
        // Create render state cache
        RenderStateCacheCreateInfo StateCacheCI;
        StateCacheCI.LogLevel = RENDER_STATE_CACHE_LOG_LEVEL_NORMAL;

        RefCntAutoPtr<IShaderSourceInputStreamFactory> pReloadFactory;
        if (m_EnableHotShaderReload)
        {
            const auto  FXShaderPaths = FileSystem::SearchRecursive(DILIGENT_FX_SHADERS_DIR, "*");
            std::string ShaderPaths;
            for (const auto& Shader : FXShaderPaths)
            {
                if (!Shader.IsDirectory)
                    continue;

                if (!ShaderPaths.empty())
                    ShaderPaths += ";";

                ShaderPaths += DILIGENT_FX_SHADERS_DIR;
                ShaderPaths += '/';
                ShaderPaths += Shader.Name;
            }
            ShaderPaths += ";";
            ShaderPaths += HYDROGENT_SHADERS_DIR;
            m_pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory(ShaderPaths.c_str(), &pReloadFactory);
            StateCacheCI.pReloadSource   = pReloadFactory;
            StateCacheCI.EnableHotReload = true;
        }

        m_DeviceWithCache = RenderDeviceWithCache<false>{m_pDevice, StateCacheCI};

        const std::string CachePath      = GetRenderStateCacheFilePath(RenderStateCacheLocationAppData, "USDViewer", m_pDevice->GetDeviceInfo().Type);
        const bool        SaveOnExit     = true;
        constexpr Uint32  ContentVersion = 1;
        m_DeviceWithCache.LoadCacheFromFile(CachePath.c_str(), SaveOnExit, ContentVersion);
    }
    else
    {
        m_DeviceWithCache = RenderDeviceWithCache<false>{m_pDevice};
    }

    ImGuizmo::SetGizmoSizeClipSpace(0.15f);

    if (m_UsdFileName.empty())
        m_UsdFileName = "usd/AppleVisionPro.usdz";
    LoadStage();
}

static pxr::GfRange3d ComputeStageAABB(const pxr::UsdStage& Stage)
{
    pxr::TfTokenVector Purposes{{pxr::UsdGeomTokens->default_}};

    // Extent hints are sometimes authored as an optimization to avoid
    // computing bounds, they are particularly useful for some tests where
    // there is no bound on the first frame.
    constexpr bool        UseExtentHints = true;
    pxr::UsdGeomBBoxCache BBoxCache(pxr::UsdTimeCode::Default(), Purposes, UseExtentHints);

    const pxr::GfBBox3d BBox = BBoxCache.ComputeWorldBound(Stage.GetPseudoRoot());
    return BBox.ComputeAlignedRange();
}

static float4x4 GetUpAxisTransform(const pxr::TfToken UpAxis)
{
    // NOTE: transform must not contain reflection as otherwise
    //       rotation in TRS widget will work incorrectly.
    if (UpAxis == pxr::UsdGeomTokens->x)
    {
        return float4x4{
            // clang-format off
            0, -1,  0,  0,
           -1,  0,  0,  0,
            0,  0, -1,  0,
            0,  0,  0,  1
            // clang-format on
        };
    }
    else if (UpAxis == pxr::UsdGeomTokens->y)
    {
        return float4x4{
            // clang-format off
            1,  0,  0,  0,
            0, -1,  0,  0,
            0,  0, -1,  0,
            0,  0,  0,  1
            // clang-format on
        };
    }
    else if (UpAxis == pxr::UsdGeomTokens->z)
    {
        return float4x4{
            // clang-format off
           -1,  0,  0,  0,
            0,  0, -1,  0,
            0, -1,  0,  0,
            0,  0,  0,  1
            // clang-format on
        };
    }
    else
    {
        LOG_WARNING_MESSAGE("Unknown up axis '", UpAxis, "'. Using identity transform");
        return float4x4::Identity();
    }
}

static bool HasDomeLight(pxr::UsdStage& Stage)
{
    for (const auto& Prim : Stage.Traverse())
    {
        if (Prim.IsA<pxr::UsdLuxDomeLight>())
            return true;
    }
    return false;
}

void USDViewer::LoadStage()
{
    {
        // Note: stage components must be destroyed in the reverse order
        //       of creation, so we use std::move() to let the destructor
        //       do the job.
        //       m_Stage = {}; would not work.
        auto Stage = std::move(m_Stage);
    }
    // It is important to clear raw pointers (FinalColorTarget, SelectedPrimId, etc.)
    m_Stage = {};

    std::string FilePath = m_UsdFileName;
#if PLATFORM_APPLE
    if (!FileSystem::IsPathAbsolute(FilePath.c_str()))
        FilePath = FileSystem::FindResource(m_UsdFileName);
#endif

    m_Stage.Stage = pxr::UsdStage::Open(FilePath);
    if (!m_Stage.Stage)
    {
        LOG_ERROR_MESSAGE("Failed to open USD stage '", m_UsdFileName, "'");
        return;
    }

    USD::HnRenderDelegate::CreateInfo DelegateCI;
    DelegateCI.pDevice           = m_DeviceWithCache;
    DelegateCI.pContext          = m_pImmediateContext;
    DelegateCI.pRenderStateCache = m_DeviceWithCache;

    RefCntAutoPtr<IThreadPool> pThreadPool{m_DeviceWithCache.GetShaderCompilationThreadPool()};
    if (!pThreadPool)
    {
        ThreadPoolCreateInfo ThreadPoolCI;
        ThreadPoolCI.NumThreads = std::max(std::thread::hardware_concurrency(), 2u) - 1u;
        pThreadPool             = CreateThreadPool(ThreadPoolCI);
    }
    DelegateCI.pThreadPool = pThreadPool;

    DelegateCI.UseVertexPool       = m_UseVertexPool;
    DelegateCI.UseIndexPool        = m_UseIndexPool;
    DelegateCI.EnableShadows       = true;
    DelegateCI.TextureCompressMode = static_cast<TEXTURE_LOAD_COMPRESS_MODE>(m_TextureCompressMode);

    DelegateCI.AllowHotShaderReload   = m_EnableHotShaderReload;
    DelegateCI.AsyncShaderCompilation = true;
    DelegateCI.AsyncTextureLoading    = m_AsyncTextureLoading;
    DelegateCI.TextureLoadBudget      = Uint64{512} << Uint64{20};

    if (m_DeviceWithCache.GetDeviceInfo().Features.BindlessResources)
    {
        m_BindingMode = USD::HN_MATERIAL_TEXTURES_BINDING_MODE_DYNAMIC;

#if PLATFORM_APPLE
        DelegateCI.TexturesArraySize = 96;
#else
        DelegateCI.TexturesArraySize = 256;
#endif
    }
    else
    {
        m_BindingMode = m_TextureAtlasDim != 0 ?
            USD::HN_MATERIAL_TEXTURES_BINDING_MODE_ATLAS :
            USD::HN_MATERIAL_TEXTURES_BINDING_MODE_LEGACY;

        DelegateCI.TextureAtlasDim = m_TextureAtlasDim;
    }
    DelegateCI.TextureBindingMode = m_BindingMode;

    const pxr::GfRange3d SceneAABB = ComputeStageAABB(*m_Stage.Stage);

    m_Stage.MetersPerUnit    = pxr::UsdGeomGetStageMetersPerUnit(m_Stage.Stage);
    DelegateCI.MetersPerUnit = m_Stage.MetersPerUnit;

    const pxr::SdfPath SceneDelegateId = pxr::SdfPath::AbsoluteRootPath();
    m_Stage.CameraId                   = SceneDelegateId.AppendChild(pxr::TfToken{"_HnCamera_"});
    m_Stage.Camera                     = pxr::UsdGeomCamera{pxr::UsdGeomCamera::Define(m_Stage.Stage, m_Stage.CameraId)};
    VERIFY_EXPR(m_Stage.Camera);

    auto AddDirectionalLight = [&SceneDelegateId, this](const char* Name, float Intensity, const pxr::GfRotation& Rotation, int ShadowMapRes) {
        const pxr::SdfPath      LightId     = SceneDelegateId.AppendChild(pxr::TfToken{Name});
        pxr::UsdLuxDistantLight DirectLight = pxr::UsdLuxDistantLight::Define(m_Stage.Stage, LightId);
        DirectLight.CreateIntensityAttr().Set(Intensity);
        DirectLight.CreateAngleAttr().Set(1.f);
        DirectLight.MakeMatrixXform().Set(pxr::GfMatrix4d{Rotation, pxr::GfVec3d{0, 0, 0}});

        if (ShadowMapRes > 0)
        {
            // Enable shadows
            pxr::UsdLuxShadowAPI ShadowAPI = pxr::UsdLuxShadowAPI::Apply(DirectLight.GetPrim());
            ShadowAPI.CreateShadowEnableAttr().Set(true);

            // Create the shadow resolution attribute
            pxr::UsdAttribute ShadowResolutionAttr = ShadowAPI.GetPrim().CreateAttribute(
                pxr::TfToken("inputs:shadow:resolution"), // Attribute name
                pxr::SdfValueTypeNames->Int,              // Attribute type
                false                                     // Not custom
            );
            // Set the shadow resolution value
            ShadowResolutionAttr.Set(ShadowMapRes);
        }
    };
    AddDirectionalLight("_HnDirectionalLight1_", 10000.f, pxr::GfRotation{pxr::GfVec3d{1, 0.5, 0.0}, -60}, 2048);
    AddDirectionalLight("_HnDirectionalLight2_", 5000.f, pxr::GfRotation{pxr::GfVec3d{1, -0.5, 0.0}, -50}, 1024);
    AddDirectionalLight("_HnDirectionalLight3_", 5000.f, pxr::GfRotation{pxr::GfVec3d{1, 0.0, 0.5}, -40}, 1024);

    // Environment map
    if (!HasDomeLight(*m_Stage.Stage))
    {
        m_Stage.DomeLightId            = SceneDelegateId.AppendChild(pxr::TfToken{"_HnDomeLight_"});
        pxr::UsdLuxDomeLight DomeLight = pxr::UsdLuxDomeLight::Define(m_Stage.Stage, m_Stage.DomeLightId);
        DomeLight.CreateTextureFileAttr().Set(pxr::SdfAssetPath{"textures/papermill.ktx"});
    }

#if 0
    // Point light
    {
        const pxr::SdfPath     LightId    = SceneDelegateId.AppendChild(pxr::TfToken{"_HnPointLight_"});
        pxr::UsdLuxSphereLight PointLight = pxr::UsdLuxSphereLight::Define(m_Stage.Stage, LightId);
        PointLight.CreateIntensityAttr().Set(0.1f * static_cast<float>(SceneAABB.GetSize().GetLengthSq()));
        PointLight.CreateColorAttr().Set(pxr::GfVec3f{1.0f, 0.6f, 0.4f});
        PointLight.CreateEnableColorTemperatureAttr().Set(true);
        PointLight.CreateColorTemperatureAttr().Set(6200.f);
        PointLight.CreateRadiusAttr().Set(0.01f / m_Stage.MetersPerUnit);
        PointLight.MakeMatrixXform().Set(pxr::GfMatrix4d{pxr::GfRotation{pxr::GfVec3d{1, 0, 0}, 0}, SceneAABB.GetMidpoint()});
    }
#endif

    m_Stage.RenderDelegate = USD::HnRenderDelegate::Create(DelegateCI);
    m_Stage.RenderIndex.reset(pxr::HdRenderIndex::New(m_Stage.RenderDelegate.get(), pxr::HdDriverVector{}));

    m_Stage.ImagingDelegate = std::make_unique<pxr::UsdImagingDelegate>(m_Stage.RenderIndex.get(), SceneDelegateId);
    m_Stage.ImagingDelegate->Populate(m_Stage.Stage->GetPseudoRoot());

    const pxr::SdfPath TaskManagerId = SceneDelegateId.AppendChild(pxr::TfToken{"_HnTaskManager_"});
    m_Stage.TaskManager              = std::make_unique<USD::HnTaskManager>(*m_Stage.RenderIndex, TaskManagerId);

    const pxr::SdfPath FinalColorTargetId = SceneDelegateId.AppendChild(pxr::TfToken{"_HnFinalColorTarget_"});
    m_Stage.RenderIndex->InsertBprim(pxr::HdPrimTypeTokens->renderBuffer, m_Stage.ImagingDelegate.get(), FinalColorTargetId);
    m_Stage.FinalColorTarget = static_cast<USD::HnRenderBuffer*>(m_Stage.RenderIndex->GetBprim(pxr::HdPrimTypeTokens->renderBuffer, FinalColorTargetId));
    VERIFY_EXPR(m_Stage.FinalColorTarget != nullptr);


    const pxr::TfToken UpAxis = pxr::UsdGeomGetStageUpAxis(m_Stage.Stage);
    m_Stage.RootTransform     = Diligent::float4x4::Scale(m_Stage.MetersPerUnit) * GetUpAxisTransform(UpAxis);
    m_Stage.ImagingDelegate->SetRootTransform(USD::ToGfMatrix4d(m_Stage.RootTransform));

    float SceneExtent = 1;
    if (!SceneAABB.IsEmpty())
    {
        SceneExtent = 0;
        for (size_t i = 0; i < 8; ++i)
        {
            float3 BBCorner = {
                static_cast<float>((i & 0x1) ? SceneAABB.GetMax()[0] : SceneAABB.GetMin()[0]),
                static_cast<float>((i & 0x2) ? SceneAABB.GetMax()[1] : SceneAABB.GetMin()[1]),
                static_cast<float>((i & 0x4) ? SceneAABB.GetMax()[2] : SceneAABB.GetMin()[2]),
            };
            SceneExtent = std::max(SceneExtent, length(BBCorner));
        }

        SceneExtent *= m_Stage.MetersPerUnit;
        m_Camera.SetDistRange(SceneExtent * 0.01f, SceneExtent * 10.f);
    }
    else
    {
        SceneExtent = 1;
        m_Camera.SetDistRange(0.01f, 100.f);
    }
    m_Camera.SetDist(SceneExtent * 2.f);
    m_CameraSettings.FocusDistance = SceneExtent * 1.5f;

    UpdateCamera();

    m_FrameParams                                     = {};
    m_FrameParams.State.FrontFaceCCW                  = true;
    m_FrameParams.FinalColorTargetId                  = FinalColorTargetId;
    m_FrameParams.CameraId                            = m_Stage.CameraId;
    m_FrameParams.Renderer.LoadingAnimationWorldScale = 1.f / SceneExtent;
    m_Stage.TaskManager->SetFrameParams(m_FrameParams);

    m_Stage.TaskManager->SetRenderRprimParams(m_RenderParams);

    m_PostProcessParams                              = {};
    m_PostProcessParams.ToneMapping.iToneMappingMode = TONE_MAPPING_MODE_UNCHARTED2;
    m_PostProcessParams.ConvertOutputToSRGB          = m_ConvertPSOutputToGamma;
    m_PostProcessParams.EnableTAA                    = true;
    m_PostProcessParams.EnableBloom                  = true;
    m_PostProcessParams.SSAO.EffectRadius            = std::min(SceneExtent * 0.1f, 5.f);

    const float GridScale                = 1.f / std::pow(10.f, std::floor(std::log10(std::max(SceneExtent, 0.01f))));
    m_PostProcessParams.Grid.GridScale   = float4{GridScale};
    m_PostProcessParams.GridFeatureFlags = (CoordinateGridRenderer::FEATURE_FLAG_RENDER_PLANE_XZ |
                                            CoordinateGridRenderer::FEATURE_FLAG_RENDER_AXIS_X |
                                            CoordinateGridRenderer::FEATURE_FLAG_RENDER_AXIS_Z);
    if (UpAxis == pxr::UsdGeomTokens->x)
    {
        m_Camera.SetRotation(PI_F / 4.f, PI_F / 6.f);
    }
    else if (UpAxis == pxr::UsdGeomTokens->y)
    {
        m_Camera.SetRotation(-PI_F / 4.f, PI_F / 6.f);
    }
    else if (UpAxis == pxr::UsdGeomTokens->z)
    {
        m_Camera.SetRotation(PI_F * 3.f / 4.f, PI_F / 6.f);
    }

    m_Stage.TaskManager->SetPostProcessParams(m_PostProcessParams);

    USD::HnRenderBoundBoxTaskParams RenderBoundBoxParams;
    RenderBoundBoxParams.Color       = float4{1.0, 1.0, 1.0, 1.0};
    RenderBoundBoxParams.PatternMask = 0x0000FFFFu;
    m_Stage.TaskManager->SetRenderBoundBoxParams(RenderBoundBoxParams);

    m_Stage.Animation.TimeCodesPerSecond = m_Stage.Stage->GetTimeCodesPerSecond();
    m_Stage.Animation.StartTime          = static_cast<float>(m_Stage.Stage->GetStartTimeCode() / m_Stage.Animation.TimeCodesPerSecond);
    m_Stage.Animation.EndTime            = static_cast<float>(m_Stage.Stage->GetEndTimeCode() / m_Stage.Animation.TimeCodesPerSecond);
    m_Stage.Animation.Time               = m_Stage.Animation.StartTime;
}

void USDViewer::LoadEnvironmentMap(const char* Path)
{
    if (m_Stage.DomeLightId.IsEmpty())
        return;

    pxr::UsdPrim Prim = m_Stage.Stage->GetPrimAtPath(m_Stage.DomeLightId);
    if (!Prim)
        return;

    pxr::UsdLuxDomeLight DomeLight{Prim};
    if (!DomeLight)
        return;

    DomeLight.GetTextureFileAttr().Set(pxr::SdfAssetPath{Path});
}

// Render a frame
void USDViewer::Render()
{
    if (!m_Stage)
        return;

    Timer Stowatch;

    m_Stage.FinalColorTarget->SetTarget(m_pSwapChain->GetCurrentBackBufferRTV());

    {
        ScopedDebugGroup DebugGroup{m_pImmediateContext, "Hydrogent"};

        pxr::HdTaskSharedPtrVector tasks = m_Stage.TaskManager->GetTasks();
        m_Engine.Execute(m_Stage.RenderIndex.get(), &tasks);
    }

    m_Stage.FinalColorTarget->ReleaseTarget();

    const auto& CtxStats         = m_pImmediateContext->GetStats();
    m_Stats.NumDrawCommands      = CtxStats.CommandCounters.Draw + CtxStats.CommandCounters.DrawIndexed;
    m_Stats.NumMultiDrawCommands = CtxStats.CommandCounters.MultiDraw + CtxStats.CommandCounters.MultiDrawIndexed;
    m_Stats.NumPSOChanges        = CtxStats.CommandCounters.SetPipelineState;
    m_Stats.NumSRBChanges        = CtxStats.CommandCounters.CommitShaderResources;
    m_Stats.NumVBChanges         = CtxStats.CommandCounters.SetVertexBuffers;
    m_Stats.NumIBChanges         = CtxStats.CommandCounters.SetIndexBuffer;
    m_Stats.NumBufferMaps        = CtxStats.CommandCounters.MapBuffer;
    m_Stats.NumBufferUpdates     = CtxStats.CommandCounters.UpdateBuffer;
    m_Stats.NumTriangles         = CtxStats.GetTotalTriangleCount();
    m_Stats.NumLines             = CtxStats.GetTotalLineCount();
    m_Stats.NumPoints            = CtxStats.GetTotalPointCount();

    m_Stats.TaskRunTime = static_cast<float>(Stowatch.GetElapsedTime()) * 0.05f + m_Stats.TaskRunTime * 0.95f;
}

void USDViewer::PopulateSceneTree(const pxr::UsdPrim& Prim)
{
    ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick | ImGuiTreeNodeFlags_SpanAvailWidth;
    if (m_Stage.SelectedPrimId.HasPrefix(Prim.GetPath()))
        Flags |= ImGuiTreeNodeFlags_Selected;

    const bool NodeOpen = ImGui::TreeNodeEx(Prim.GetName().GetText(), Flags);
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
    {
        SetSelectedPrim(Prim.GetPath());
    }

    if (ImGui::BeginPopupContextItem())
    {
        bool IsVisible = m_Stage.ImagingDelegate->GetVisible(Prim.GetPath());
        if (ImGui::Selectable(IsVisible ? "Hide" : "Show"))
        {
            if (pxr::UsdGeomImageable Imageable{Prim})
            {
                if (IsVisible)
                    Imageable.MakeInvisible();
                else
                    Imageable.MakeVisible();
            }
        }
        ImGui::EndPopup();
    }

    if (NodeOpen)
    {
        for (const auto& Prop : Prim.GetProperties())
        {
            ImGui::TextDisabled("%s", Prop.GetName().GetText());
        }

        // Check for and display variant sets
        pxr::UsdVariantSets      VariantSets     = Prim.GetVariantSets();
        std::vector<std::string> VariantSetNames = VariantSets.GetNames();
        for (const std::string& VariantSetName : VariantSetNames)
        {
            pxr::UsdVariantSet       VariantSet       = VariantSets.GetVariantSet(VariantSetName);
            const std::string        VariantSelection = VariantSet.GetVariantSelection();
            std::vector<std::string> VariantNames     = VariantSet.GetVariantNames();
            std::vector<const char*> VariantNamePtrs(VariantNames.size());

            int SelectedVariant = -1;
            for (size_t i = 0; i < VariantNames.size(); ++i)
            {
                VariantNamePtrs[i] = VariantNames[i].c_str();
                if (VariantSelection == VariantNames[i])
                    SelectedVariant = static_cast<int>(i);
            }
            ImGui::SetNextItemWidth(180);
            if (ImGui::Combo((VariantSetName + " variant").c_str(), &SelectedVariant, VariantNamePtrs.data(), static_cast<int>(VariantNames.size())))
            {
                if (SelectedVariant >= 0)
                {
                    VariantSet.SetVariantSelection(VariantNames[SelectedVariant]);
                }
            }
        }

        for (auto Child : Prim.GetAllChildren())
            PopulateSceneTree(Child);

        ImGui::TreePop();
    }
    else
    {
        if (m_ScrolllToSelectedTreeItem && (Flags & ImGuiTreeNodeFlags_Selected))
        {
            ImGui::SetScrollHereY();
            m_ScrolllToSelectedTreeItem = false;
        }
    }
}

static pxr::GfMatrix4d GetPrimGlobalTransform(pxr::UsdPrim Prim)
{
    pxr::GfMatrix4d GlobalXForm{1};
    while (Prim)
    {
        if (pxr::UsdGeomXformable XFormable{Prim})
        {
            pxr::GfMatrix4d LocalXform{1.0};
            bool            ResetsXformStack = false;
            if (XFormable.GetLocalTransformation(&LocalXform, &ResetsXformStack))
            {
                GlobalXForm = GlobalXForm * LocalXform;
            }
        }

        Prim = Prim.GetParent();
    }

    return GlobalXForm;
}

void USDViewer::EditSelectedPrimTransform()
{
    pxr::UsdPrim Prim = m_Stage.Stage->GetPrimAtPath(m_Stage.SelectedPrimId);
    if (!Prim)
        return;

    pxr::UsdGeomXformable XFormable{Prim};
    if (!XFormable)
        return;

    // Check if the selected prim has pivot.
    // The xformOpOrder of an xformable that has all of the supported basic ops is as follows (see xformCommonAPI.h):
    // ["xformOp:translate", "xformOp:translate:pivot", "xformOp:rotateXYZ", "xformOp:scale", "!invert!xformOp:translate:pivot"].
    pxr::GfVec3f PivotVal{};
    if (pxr::UsdGeomXformOp PivotOp = XFormable.GetXformOp(pxr::UsdGeomXformOp::TypeTranslate, pxr::UsdGeomTokens->pivot))
    {
        // There must also be an inverse pivot op to negate the translation defined by the pivot (xformCommonAPI.cpp/_GetOrAddCommonXformOps)
        if (XFormable.GetXformOp(pxr::UsdGeomXformOp::TypeTranslate, pxr::UsdGeomTokens->pivot, /*isInverseOp = true*/ true))
        {
            PivotOp.Get(&PivotVal, 0);
        }
    }

    pxr::GfMatrix4d ParentGlobalXForm  = GetPrimGlobalTransform(Prim.GetParent());
    float4x4        ParentGlobalMatrix = USD::ToFloat4x4(ParentGlobalXForm);

    ParentGlobalMatrix = ParentGlobalMatrix * m_Stage.RootTransform;

    pxr::GfMatrix4d LocalXform       = pxr::GfMatrix4d{1.0};
    bool            ResetsXformStack = false;
    XFormable.GetLocalTransformation(&LocalXform, &ResetsXformStack);

    float4x4 NewGlobalMatrix = float4x4::Translation(PivotVal[0], PivotVal[1], PivotVal[2]) * float4x4::MakeMatrix(LocalXform.data()) * ParentGlobalMatrix;

    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::OPERATION GizmoOperation = static_cast<ImGuizmo::OPERATION>(static_cast<int>(ImGuizmo::UNIVERSAL) & ~static_cast<int>(ImGuizmo::ROTATE_SCREEN));
    ImGuizmo::MODE      GizmoMode      = ImGuizmo::LOCAL;
    // NOTE: it is important that ImGuizmo operates on a matrix without reflections, otherwise
    //       rotation will be flipped.
    if (ImGuizmo::Manipulate(m_CameraView.Data(), m_CameraProj.Data(), GizmoOperation, GizmoMode, NewGlobalMatrix.Data()))
    {
        // New local matrix is the delta between new global matrix and parent global matrix
        float4x4 NewLocalMatrix = float4x4::Translation(-PivotVal[0], -PivotVal[1], -PivotVal[2]) * NewGlobalMatrix * ParentGlobalMatrix.Inverse();
        XFormable.MakeMatrixXform().Set(USD::ToGfMatrix4d(NewLocalMatrix));
        // Restore pivot as MakeMatrixXform clears all ops
        if (PivotVal != pxr::GfVec3f{})
        {
            XFormable.AddXformOp(pxr::UsdGeomXformOp::TypeTranslate, pxr::UsdGeomXformOp::PrecisionFloat, pxr::UsdGeomTokens->pivot).Set(PivotVal, 0);
            // Add inverse pivot to negate the transformation
            XFormable.AddXformOp(pxr::UsdGeomXformOp::TypeTranslate, pxr::UsdGeomXformOp::PrecisionFloat, pxr::UsdGeomTokens->pivot, /*isInverseOp = */ true);
        }
    }
}

void USDViewer::UpdateUI()
{
    bool UpdateRenderParams      = false;
    bool UpdateFrameParams       = false;
    bool UpdatePostProcessParams = false;

    ImGuizmo::BeginFrame();

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 600), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr))
    {
        if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("Stage"))
            {
                {
                    m_ModelNames.resize(m_Models.size());
                    for (size_t i = 0; i < m_Models.size(); ++i)
                        m_ModelNames[i] = m_Models[i].Name.c_str();

                    if (ImGui::Combo("Model", &m_SelectedModel, m_ModelNames.data(), static_cast<int>(m_ModelNames.size()), 20))
                    {
                        m_UsdFileName = m_Models[m_SelectedModel].Path;
                        LoadStage();
                    }
                }

#if FILE_DIALOG_SUPPORTED
                if (ImGui::Button("Load model"))
                {
                    FileDialogAttribs OpenDialogAttribs{FILE_DIALOG_TYPE_OPEN};
                    OpenDialogAttribs.Title  = "Select USD file";
                    OpenDialogAttribs.Filter = "USD files (*.usd;*.usdc;*.usdz;*.usda)\0*.usd;*.usdc;*.usdz;*.usda\0All files\0*.*\0\0";
                    auto FileName            = FileSystem::FileDialog(OpenDialogAttribs);
                    if (!FileName.empty())
                    {
                        m_UsdFileName = std::move(FileName);
                        LoadStage();
                    }
                }

                if (!m_Stage.DomeLightId.IsEmpty())
                {
                    if (ImGui::Button("Load Environment Map"))
                    {
                        FileDialogAttribs OpenDialogAttribs{FILE_DIALOG_TYPE_OPEN};
                        OpenDialogAttribs.Title  = "Select HDR file";
                        OpenDialogAttribs.Filter = "HDR files (*.hdr)\0*.hdr;\0All files\0*.*\0\0";
                        auto FileName            = FileSystem::FileDialog(OpenDialogAttribs);
                        if (!FileName.empty())
                            LoadEnvironmentMap(FileName.data());
                    }
                }

                if (ImGui::Button("Open directory"))
                {
                    auto DirName = FileSystem::OpenFolderDialog("Select folder with USD assets");
                    if (!DirName.empty())
                    {
                        UpdateModelsList(DirName.c_str());
                    }
                }
#endif

                {
                    std::array<const char*, static_cast<size_t>(2)> SelectModes;
                    SelectModes[static_cast<size_t>(SelectionMode::OnClick)] = "On click";
                    SelectModes[static_cast<size_t>(SelectionMode::OnHover)] = "On Hover";
                    static_assert(static_cast<size_t>(SelectionMode::Count) == 2, "Did you add a new select mode? You may want to handle it here");

                    int SelectMode = static_cast<int>(m_SelectMode);
                    if (ImGui::Combo("Select mode", &SelectMode, SelectModes.data(), static_cast<int>(SelectModes.size())))
                    {
                        m_SelectMode = static_cast<SelectionMode>(SelectMode);
                    }
                }

                if (m_Stage.Animation.EndTime > m_Stage.Animation.StartTime)
                {
                    ImGui::Spacing();

                    ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
                    if (ImGui::TreeNode("Animation"))
                    {
                        ImGui::Checkbox("Play", &m_Stage.Animation.Play);
                        ImGui::SliderFloat("Time", &m_Stage.Animation.Time, m_Stage.Animation.StartTime, m_Stage.Animation.EndTime);
                        ImGui::TreePop();
                    }
                }

                ImGui::Spacing();

                ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
                if (ImGui::TreeNode("Scene"))
                {
                    if (m_Stage.Stage)
                    {
                        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
                        for (auto Prim : m_Stage.Stage->GetPseudoRoot().GetAllChildren())
                            PopulateSceneTree(Prim);
                    }

                    ImGui::TreePop();
                }

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Renderer"))
            {
                ImGui::PushItemWidth(130);

                if (ImGui::TreeNode("Camera Settings"))
                {
                    const auto         CameraDist = m_Camera.GetDist();
                    const pxr::GfVec2f ClippingRange{CameraDist / 100.f, CameraDist * 3.f};

                    ImGui::SliderFloat("Focal Length (mm)", &m_CameraSettings.FocalLength_mm, 24.0, 300.0);
                    ImGui::SliderFloat("Aperture (f-stop)", &m_CameraSettings.FStop, 1.0, 64.0, "%.3f", ImGuiSliderFlags_Logarithmic);
                    ImGui::SliderFloat("Exposure", &m_CameraSettings.Exposure, -8.0, +8.0, "%.3f");
                    ImGui::SliderFloat("Focus Distance", &m_CameraSettings.FocusDistance, ClippingRange[0], ClippingRange[1], "%.3f", ImGuiSliderFlags_AlwaysClamp);
                    ImGui::SliderFloat("Sensor Width", &m_CameraSettings.SensorWidth_mm, 1, 100);

                    ImGui::ScopedDisabler Disabler{true, 0.5f};
                    ImGui::SliderFloat("Sensor Height", &m_CameraSettings.SensorHeight_mm, 1, 100);

                    ImGui::TreePop();
                }

                ImGui::Spacing();

                ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
                if (ImGui::TreeNode("Lighting"))
                {
                    if (ImGui::SliderFloat("Occlusion strength", &m_FrameParams.Renderer.OcclusionStrength, 0.f, 1.f))
                        UpdateFrameParams = true;
                    if (ImGui::SliderFloat("Emission scale", &m_FrameParams.Renderer.EmissionScale, 0.f, 1.f))
                        UpdateFrameParams = true;
                    if (ImGui::SliderFloat("IBL scale", &m_FrameParams.Renderer.IBLScale, 0.f, 1.f))
                        UpdateFrameParams = true;

                    {
                        std::array<const char*, static_cast<size_t>(PBR_Renderer::DebugViewType::NumDebugViews)> DebugViews;

                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::None)]                 = "None";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Texcoord0)]            = "Tex coords 0";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Texcoord1)]            = "Tex coords 1";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::BaseColor)]            = "Base Color";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Transparency)]         = "Transparency";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Occlusion)]            = "Occlusion";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Emissive)]             = "Emissive";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Metallic)]             = "Metallic";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Roughness)]            = "Roughness";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::DiffuseColor)]         = "Diffuse color";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::SpecularColor)]        = "Specular color (R0)";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Reflectance90)]        = "Reflectance90";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::MeshNormal)]           = "Mesh normal";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::ShadingNormal)]        = "Shading normal";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::MotionVectors)]        = "Motion vectors";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::NdotV)]                = "n*v";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::PunctualLighting)]     = "Punctual Lighting";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::DiffuseIBL)]           = "Diffuse IBL";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::WhiteBaseColor)]       = "White Base Color";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::SpecularIBL)]          = "Specular IBL";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::ClearCoat)]            = "Clear Coat";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::ClearCoatFactor)]      = "Clear Coat Factor";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::ClearCoatRoughness)]   = "Clear Coat Roughness";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::ClearCoatNormal)]      = "Clear Coat Normal";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Sheen)]                = "Sheen";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::SheenColor)]           = "Sheen Color";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::SheenRoughness)]       = "Sheen Roughness";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::AnisotropyStrength)]   = "Anisotropy Strength";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::AnisotropyDirection)]  = "Anisotropy Direction";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Iridescence)]          = "Iridescence";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::IridescenceFactor)]    = "Iridescence Factor";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::IridescenceThickness)] = "Iridescence Thickness";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Transmission)]         = "Transmission";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Thickness)]            = "Volume Thickness";
                        static_assert(static_cast<size_t>(PBR_Renderer::DebugViewType::NumDebugViews) == 34, "Did you add a new debug view mode? You may want to handle it here");

                        if (ImGui::Combo("Debug view", &m_Stage.DebugViewMode, DebugViews.data(), static_cast<int>(DebugViews.size()), 15))
                        {
                            m_Stage.RenderDelegate->SetDebugView(static_cast<PBR_Renderer::DebugViewType>(m_Stage.DebugViewMode));
                        }
                    }

                    {
                        std::array<const char*, static_cast<size_t>(USD::HN_RENDER_MODE_COUNT)> RenderModes;
                        RenderModes[USD::HN_RENDER_MODE_SOLID]      = "Solid";
                        RenderModes[USD::HN_RENDER_MODE_MESH_EDGES] = "Edges";
                        RenderModes[USD::HN_RENDER_MODE_POINTS]     = "Points";
                        static_assert(USD::HN_RENDER_MODE_COUNT == 3, "Did you add a new render mode? You may want to handle it here");

                        if (ImGui::Combo("Render mode", &m_Stage.RenderMode, RenderModes.data(), static_cast<int>(RenderModes.size())))
                        {
                            m_Stage.RenderDelegate->SetRenderMode(static_cast<USD::HN_RENDER_MODE>(m_Stage.RenderMode));
                        }
                    }

                    if (ImGui::SliderFloat("Selection outline width", &m_PostProcessParams.SelectionOutlineWidth, 1.f, 16.f))
                        UpdatePostProcessParams = true;

                    {
                        bool EnableSSR = m_PostProcessParams.SSRScale != 0;
                        if (ImGui::Checkbox("Reflections", &EnableSSR))
                        {
                            m_PostProcessParams.SSRScale = EnableSSR ? 1.f : 0.f;
                            UpdatePostProcessParams      = true;
                        }
                    }

                    {
                        bool EnableSSAO = m_PostProcessParams.SSAOScale != 0;
                        if (ImGui::Checkbox("Ambient Occlusion", &EnableSSAO))
                        {
                            m_PostProcessParams.SSAOScale = EnableSSAO ? 1.f : 0.f;
                            UpdatePostProcessParams       = true;
                        }
                    }

                    if (ImGui::Checkbox("TAA", &m_PostProcessParams.EnableTAA))
                    {
                        UpdatePostProcessParams = true;
                    }

                    if (ImGui::Checkbox("Depth of Field", &m_PostProcessParams.EnableDOF))
                    {
                        UpdatePostProcessParams = true;
                    }

                    if (ImGui::Checkbox("Bloom", &m_PostProcessParams.EnableBloom))
                    {
                        UpdatePostProcessParams = true;
                    }

                    if (ImGui::Checkbox("Shadows", &m_Stage.UseShadows))
                        m_Stage.RenderDelegate->SetUseShadows(m_Stage.UseShadows);

                    ImGui::TreePop();
                }

                ImGui::Spacing();

                {
                    ImGui::ScopedDisabler Disabler{m_PostProcessParams.SSRScale == 0};
                    if (ImGui::TreeNode("Screen Space Reflections"))
                    {
                        if (ScreenSpaceReflection::UpdateUI(m_PostProcessParams.SSR, m_PostProcessParams.SSRFeatureFlags, m_SSRSettingsDisplayMode))
                            UpdatePostProcessParams = true;

                        ImGui::Spacing();
                        if (ImGui::Button("Reset"))
                        {
                            m_PostProcessParams.SSR = USD::HnPostProcessTaskParams{}.SSR;
                            UpdatePostProcessParams = true;
                        }

                        ImGui::TreePop();
                    }
                }

                {
                    ImGui::ScopedDisabler Disabler{m_PostProcessParams.SSAOScale == 0};
                    if (ImGui::TreeNode("Screen Space Ambient Occlusion"))
                    {
                        if (ScreenSpaceAmbientOcclusion::UpdateUI(m_PostProcessParams.SSAO, m_PostProcessParams.SSAOFeatureFlags))
                            UpdatePostProcessParams = true;

                        ImGui::Spacing();
                        if (ImGui::Button("Reset"))
                        {
                            m_PostProcessParams.SSAO = USD::HnPostProcessTaskParams{}.SSAO;
                            UpdatePostProcessParams  = true;
                        }

                        ImGui::TreePop();
                    }
                }

                {
                    ImGui::ScopedDisabler Disabler{!m_PostProcessParams.EnableTAA};
                    if (ImGui::TreeNode("Temporal Anti Aliasing"))
                    {
                        if (TemporalAntiAliasing::UpdateUI(m_PostProcessParams.TAA, m_PostProcessParams.TAAFeatureFlags))
                            UpdatePostProcessParams = true;

                        ImGui::Spacing();
                        if (ImGui::Button("Reset"))
                        {
                            m_PostProcessParams.TAA = USD::HnPostProcessTaskParams{}.TAA;
                            UpdatePostProcessParams = true;
                        }

                        ImGui::TreePop();
                    }
                }

                {
                    ImGui::ScopedDisabler Disabler{!m_PostProcessParams.EnableDOF};
                    if (ImGui::TreeNode("Depth of Field"))
                    {
                        if (DepthOfField::UpdateUI(m_PostProcessParams.DOF, m_PostProcessParams.DOFFeatureFlags))
                            UpdatePostProcessParams = true;

                        ImGui::Spacing();
                        if (ImGui::Button("Reset"))
                        {
                            m_PostProcessParams.DOF = USD::HnPostProcessTaskParams{}.DOF;
                            UpdatePostProcessParams = true;
                        }

                        ImGui::TreePop();
                    }
                }

                {
                    ImGui::ScopedDisabler Disabler{!m_PostProcessParams.EnableBloom};
                    if (ImGui::TreeNode("Bloom"))
                    {
                        if (Bloom::UpdateUI(m_PostProcessParams.Bloom, m_PostProcessParams.BloomFeatureFlags))
                            UpdatePostProcessParams = true;

                        ImGui::Spacing();
                        if (ImGui::Button("Reset"))
                        {
                            m_PostProcessParams.Bloom = USD::HnPostProcessTaskParams{}.Bloom;
                            UpdatePostProcessParams   = true;
                        }

                        ImGui::TreePop();
                    }
                }

                if (ImGui::TreeNode("Tone mapping"))
                {
                    if (ToneMappingUpdateUI(m_PostProcessParams.ToneMapping, &m_PostProcessParams.AverageLogLum))
                        UpdatePostProcessParams = true;

                    ImGui::Spacing();
                    if (ImGui::Button("Reset"))
                    {
                        static constexpr USD::HnPostProcessTaskParams DefaultParams;

                        m_PostProcessParams.ToneMapping   = DefaultParams.ToneMapping;
                        m_PostProcessParams.AverageLogLum = DefaultParams.AverageLogLum;
                        UpdatePostProcessParams           = true;
                    }

                    ImGui::TreePop();
                }

                if (ImGui::TreeNode("Grid"))
                {
                    if (CoordinateGridRenderer::UpdateUI(m_PostProcessParams.Grid, m_PostProcessParams.GridFeatureFlags))
                        UpdatePostProcessParams = true;

                    ImGui::Spacing();
                    if (ImGui::Button("Reset"))
                    {
                        m_PostProcessParams.Grid = USD::HnPostProcessTaskParams{}.Grid;
                        UpdatePostProcessParams  = true;
                    }

                    ImGui::TreePop();
                }

                ImGui::Spacing();

                ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
                if (ImGui::TreeNode("Elements"))
                {
                    auto MaterialCheckbox = [this](const char* Name, const pxr::TfToken& MaterialTag) {
                        bool Enabled = m_Stage.TaskManager->IsMaterialEnabled(MaterialTag);
                        if (ImGui::Checkbox(Name, &Enabled))
                            m_Stage.TaskManager->EnableMaterial(MaterialTag, Enabled);
                    };
                    MaterialCheckbox("Default Material", USD::HnMaterialTagTokens->defaultTag);
                    MaterialCheckbox("Masked Material", USD::HnMaterialTagTokens->masked);
                    MaterialCheckbox("Additive Material", USD::HnMaterialTagTokens->additive);
                    MaterialCheckbox("Translucent Material", USD::HnMaterialTagTokens->translucent);
                    {
                        bool Enabled = m_Stage.TaskManager->IsEnvironmentMapEnabled();
                        if (ImGui::Checkbox("Env map", &Enabled))
                            m_Stage.TaskManager->EnableEnvironmentMap(Enabled);
                    }
                    {
                        bool Enabled = m_Stage.TaskManager->IsSelectedPrimBoundBoxEnabled();
                        if (ImGui::Checkbox("Selected prim bound box", &Enabled))
                            m_Stage.TaskManager->EnableSelectedPrimBoundBox(Enabled);
                    }

                    ImGui::TreePop();
                }

                ImGui::PopItemWidth();

                if (m_EnableHotShaderReload)
                {
                    ImGui::Spacing();
                    if (ImGui::Button("Reload shaders") && m_DeviceWithCache.GetCache())
                        m_DeviceWithCache.GetCache()->Reload();
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }
    ImGui::End();


    if (ImGui::Begin("Stats", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoBackground))
    {
        const USD::HnRenderDelegateMemoryStats MemoryStats = m_Stage.RenderDelegate->GetMemoryStats();
        ImGui::Text("Task time\n"
                    "Binding\n"
                    "Draws + MDraws\n"
                    "Tris\n"
                    "Lines\n"
                    "Points\n"
                    "State Changes\n"
                    "  PSO\n"
                    "  SRB\n"
                    "  VB\n"
                    "  IB\n"
                    "Buffer M + U\n"
                    "Memory Usage\n"
                    "  Vertex Pool\n"
                    "  Index Pool\n"
                    "  Atlas\n"
                    "  Sep Tex");
        ImGui::SameLine();

        const std::string VertPoolCommittedSizeStr = GetMemorySizeString(MemoryStats.VertexPool.CommittedSize);
        const std::string VertPoolUsedSizeStr      = GetMemorySizeString(MemoryStats.VertexPool.UsedSize);
        const std::string IndPoolCommittedSizeStr  = GetMemorySizeString(MemoryStats.IndexPool.CommittedSize).c_str();
        const std::string IndPoolUsedSizeStr       = GetMemorySizeString(MemoryStats.IndexPool.UsedSize).c_str();
        const std::string AtlasCommittedSizeStr    = GetMemorySizeString<Uint64>(MemoryStats.Atlas.CommittedSize, 0, 1 << 20).c_str();
        const std::string SepTexturesSizeStr       = GetMemorySizeString<Uint64>(MemoryStats.TextureRegistry.SeparateTexDataSize, 0, 1 << 20).c_str();

        const char* TextureBindingModeStr = "";
        switch (m_BindingMode)
        {
            case USD::HN_MATERIAL_TEXTURES_BINDING_MODE_LEGACY: TextureBindingModeStr = "Legacy"; break;
            case USD::HN_MATERIAL_TEXTURES_BINDING_MODE_ATLAS: TextureBindingModeStr = "Atlas"; break;
            case USD::HN_MATERIAL_TEXTURES_BINDING_MODE_DYNAMIC: TextureBindingModeStr = "Dynamic"; break;
        }

        ImGui::Text("%.1f ms\n"
                    "%s\n"
                    "%d + %d\n"
                    "%d\n"
                    "%d\n"
                    "%d\n"
                    "\n"
                    "%d\n"
                    "%d\n"
                    "%d\n"
                    "%d\n"
                    "%d + %d\n"
                    "\n"
                    "%s / %s (%d allocs, %dK verts)\n"
                    "%s / %s (%d allocs)\n"
                    "%s (%.1lf%%, %d allocs)\n"
                    "%s",
                    m_Stats.TaskRunTime * 1000.f,
                    TextureBindingModeStr,
                    m_Stats.NumDrawCommands, m_Stats.NumMultiDrawCommands,
                    m_Stats.NumTriangles,
                    m_Stats.NumLines,
                    m_Stats.NumPoints,
                    m_Stats.NumPSOChanges,
                    m_Stats.NumSRBChanges,
                    m_Stats.NumVBChanges,
                    m_Stats.NumIBChanges,
                    m_Stats.NumBufferMaps, m_Stats.NumBufferUpdates,
                    VertPoolUsedSizeStr.c_str(), VertPoolCommittedSizeStr.c_str(),
                    MemoryStats.VertexPool.AllocationCount,
                    static_cast<Uint32>(MemoryStats.VertexPool.AllocatedVertexCount / 1000ull),
                    IndPoolUsedSizeStr.c_str(), IndPoolCommittedSizeStr.c_str(), MemoryStats.IndexPool.AllocationCount,
                    AtlasCommittedSizeStr.c_str(), static_cast<double>(MemoryStats.Atlas.AllocatedTexels) / static_cast<double>(std::max(MemoryStats.Atlas.TotalTexels, Uint64{1})) * 100.0, MemoryStats.Atlas.AllocationCount,
                    SepTexturesSizeStr.c_str());

        ImVec2 WndSize     = ImGui::GetWindowSize();
        ImVec2 DisplaySize = ImGui::GetIO().DisplaySize;
        ImGui::SetWindowPos(ImVec2(DisplaySize.x - WndSize.x - 10, DisplaySize.y - WndSize.y - 10));
    }
    ImGui::End();

    if (!m_Stage.SelectedPrimId.IsEmpty())
    {
        EditSelectedPrimTransform();
    }

    if (UpdateRenderParams)
        m_Stage.TaskManager->SetRenderRprimParams(m_RenderParams);
    if (UpdatePostProcessParams)
        m_Stage.TaskManager->SetPostProcessParams(m_PostProcessParams);
    if (UpdateFrameParams)
        m_Stage.TaskManager->SetFrameParams(m_FrameParams);
}

void USDViewer::UpdateCamera()
{
    auto CameraDist = m_Camera.GetDist();
    // Flip Y axis
    m_CameraView = float4x4::Scale(1, -1, 1) * m_Camera.GetRotation().ToMatrix() * float4x4::Translation(0, 0, CameraDist);
    // Apply pretransform matrix that rotates the scene according the surface orientation
    m_CameraView *= GetSurfacePretransformMatrix(float3{0, 0, 1});
    // USD camera looks along -Z axis
    m_CameraView *= float4x4::Scale(1, 1, -1);

    const SwapChainDesc& SCDesc      = m_pSwapChain->GetDesc();
    m_CameraSettings.SensorHeight_mm = m_CameraSettings.SensorWidth_mm / static_cast<float>(SCDesc.Width) * static_cast<float>(SCDesc.Height);
    const float        FOV           = 2.0f * atanf(m_CameraSettings.SensorHeight_mm / (2.0 * m_CameraSettings.FocalLength_mm));
    const pxr::GfVec2f ClippingRange{CameraDist / 100.f, CameraDist * 3.f};
    // Get projection matrix adjusted to the current screen orientation
    m_CameraProj = GetAdjustedProjectionMatrix(FOV, ClippingRange[0], ClippingRange[1]);

    m_CameraSettings.FocusDistance = clamp(m_CameraSettings.FocusDistance, ClippingRange[0] + m_CameraSettings.FocalLength_mm * 0.001f, ClippingRange[1]);

    m_Stage.Camera.MakeMatrixXform().Set(USD::ToGfMatrix4d((m_Stage.RootTransform * m_CameraView).Inverse()));
    m_Stage.Camera.GetFStopAttr().Set(m_CameraSettings.FStop);
    m_Stage.Camera.GetExposureAttr().Set(m_CameraSettings.Exposure);

    // USD camera properties are measured in scene units
    m_Stage.Camera.GetClippingRangeAttr().Set(ClippingRange / m_Stage.MetersPerUnit);
    m_Stage.Camera.GetFocusDistanceAttr().Set(m_CameraSettings.FocusDistance / m_Stage.MetersPerUnit);

    // By an odd convention, lens and filmback properties are measured in tenths of a scene unit rather than "raw" scene units.
    // https://openusd.org/dev/api/class_usd_geom_camera.html#UsdGeom_CameraUnits
    // So, for example after
    //      UsdCamera.GetFocalLengthAttr().Set(30.f)
    // Reading the attribute will return same value:
    //      float focalLength;
    //      UsdCamera.GetFocalLengthAttr().Get(&focalLength); // focalLength == 30
    // However
    //      focalLength = SceneDelegate->GetCameraParamValue(id, HdCameraTokens->focalLength).Get<float>(); //  focalLength == 3

    constexpr float UsdCamLensUnitScale = 10;
    const float     MillimetersPerUnit  = m_Stage.MetersPerUnit * 1000.f;
    m_Stage.Camera.GetFocalLengthAttr().Set(m_CameraSettings.FocalLength_mm * UsdCamLensUnitScale / MillimetersPerUnit);
    m_Stage.Camera.GetHorizontalApertureAttr().Set(m_CameraSettings.SensorWidth_mm * UsdCamLensUnitScale / MillimetersPerUnit);
    m_Stage.Camera.GetVerticalApertureAttr().Set(m_CameraSettings.SensorHeight_mm * UsdCamLensUnitScale / MillimetersPerUnit);
}

void USDViewer::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    m_Camera.SetZoomSpeed(m_Camera.GetDist() * 0.1f);
    m_Camera.Update(m_InputController);
    UpdateCamera();

    const float LastAnimationTime = m_Stage.Animation.Time;
    if (m_Stage.Animation.Play)
    {
        m_Stage.Animation.Time += static_cast<float>(ElapsedTime);
        if (m_Stage.Animation.Time > m_Stage.Animation.EndTime)
            m_Stage.Animation.Time = m_Stage.Animation.StartTime;
    }

    // Update camera first as TRS widget needs camera view/proj matrices.
    UpdateUI();

    if (LastAnimationTime != m_Stage.Animation.Time)
    {
        m_Stage.ImagingDelegate->SetTime(m_Stage.Animation.Time * m_Stage.Animation.TimeCodesPerSecond);
    }

    if (!m_Stage)
        return;

    const auto&         SCDesc         = m_pSwapChain->GetDesc();
    const auto&         Mouse          = m_InputController.GetMouseState();
    const pxr::SdfPath* SelectedPrimId = nullptr;
    if (Mouse.PosX >= 0 && Mouse.PosX < static_cast<float>(SCDesc.Width) &&
        Mouse.PosY >= 0 && Mouse.PosY < static_cast<float>(SCDesc.Height))
    {
        auto PosX = static_cast<Uint32>(Mouse.PosX);
        auto PosY = static_cast<Uint32>(Mouse.PosY);
        if (m_DeviceWithCache.GetDeviceInfo().IsGLDevice())
            PosY = SCDesc.Height - 1 - PosY;

        USD::HnReadRprimIdTaskParams Params{true, PosX, PosY};
        m_Stage.TaskManager->SetReadRprimIdParams(Params);

        SelectedPrimId = m_Stage.TaskManager->GetSelectedRPrimId();
    }

    bool SelectPrim = false;
    if (m_SelectMode == SelectionMode::OnClick)
    {
        if (!m_IsSelecting)
        {

            if ((m_PrevMouse.ButtonFlags & MouseState::BUTTON_FLAG_LEFT) == 0 &&
                (Mouse.ButtonFlags & MouseState::BUTTON_FLAG_LEFT) != 0)
                m_IsSelecting = true; // LMB was pressed
        }
        else
        {
            if ((Mouse.ButtonFlags & MouseState::BUTTON_FLAG_LEFT) == 0)
            {
                // LMB was released
                SelectPrim = true;
            }
            else if (m_PrevMouse.PosX != Mouse.PosX ||
                     m_PrevMouse.PosY != Mouse.PosY)
            {
                // Mouse was moved while LMB was pressed
                m_IsSelecting = false;
            }
        }
    }
    else if (m_SelectMode == SelectionMode::OnHover)
    {
        SelectPrim    = true;
        m_IsSelecting = false;
    }

    if ((Mouse.ButtonFlags & MouseState::BUTTON_FLAG_LEFT) == 0)
        m_IsSelecting = false;

    if (SelectPrim && SelectedPrimId != nullptr)
    {
        SetSelectedPrim(*SelectedPrimId);
        if (!SelectedPrimId->IsEmpty())
            m_ScrolllToSelectedTreeItem = true;
    }

    m_PrevMouse = Mouse;

    m_Stage.ImagingDelegate->ApplyPendingUpdates();
}

void USDViewer::SetSelectedPrim(const pxr::SdfPath& SelectedPrimId)
{
    if (SelectedPrimId == m_Stage.SelectedPrimId)
        return;

    m_Stage.SelectedPrimId = SelectedPrimId;
    m_Stage.RenderDelegate->SetSelectedRPrimId(m_Stage.SelectedPrimId);
    m_PostProcessParams.NonselectionDesaturationFactor = !m_Stage.SelectedPrimId.IsEmpty() ? 0.5f : 0.f;
    m_Stage.TaskManager->SetPostProcessParams(m_PostProcessParams);
}

} // namespace Diligent
