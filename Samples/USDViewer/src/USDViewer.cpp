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
#include "Tasks/HnReadRprimIdTask.hpp"
#include "Tasks/HnRenderAxesTask.hpp"
#include "HnTokens.hpp"
#include "HnCamera.hpp"
#include "HnLight.hpp"
#include "GfTypeConversions.hpp"

#include "imgui.h"
#include "ImGuiUtils.hpp"
#include "ImGuizmo.h"

#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/property.h"
#include "pxr/usd/usdGeom/metrics.h"


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
    ArgsParser.Parse("texture_atlas", m_UseTextureAtlas);
    LOG_INFO_MESSAGE("USD Viewer Arguments:",
                     "\n    USD Path:        ", m_UsdFileName,
                     "\n    Use vertex pool: ", m_UseVertexPool ? "Yes" : "No",
                     "\n    Use index pool:  ", m_UseIndexPool ? "Yes" : "No",
                     "\n    Use tex atlas:   ", m_UseTextureAtlas ? "Yes" : "No");

    std::string ModelsDir;
    ArgsParser.Parse("usd_dir", 'd', ModelsDir);
    UpdateModelsList(ModelsDir);

    return CommandLineStatus::OK;
}

void USDViewer::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    ImGuizmo::SetGizmoSizeClipSpace(0.15f);

    RefCntAutoPtr<ITexture> EnvironmentMap;
    CreateTextureFromFile("textures/papermill.ktx", TextureLoadInfo{"Environment map"}, m_pDevice, &EnvironmentMap);
    VERIFY_EXPR(EnvironmentMap);
    m_EnvironmentMapSRV = EnvironmentMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    StateTransitionDesc Barriers[] =
        {
            {EnvironmentMap, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE},
        };
    m_pImmediateContext->TransitionResourceStates(_countof(Barriers), Barriers);

    m_PostProcessParams.ToneMappingMode     = TONE_MAPPING_MODE_UNCHARTED2;
    m_PostProcessParams.ConvertOutputToSRGB = m_ConvertPSOutputToGamma;

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

    m_Stage.Stage = pxr::UsdStage::Open(m_UsdFileName.c_str());
    if (!m_Stage.Stage)
    {
        LOG_ERROR_MESSAGE("Failed to open USD stage '", m_UsdFileName, "'");
        return;
    }

    USD::HnRenderDelegate::CreateInfo DelegateCI;
    DelegateCI.pDevice           = m_pDevice;
    DelegateCI.pContext          = m_pImmediateContext;
    DelegateCI.pRenderStateCache = nullptr;
    DelegateCI.UseVertexPool     = m_UseVertexPool;
    DelegateCI.UseIndexPool      = m_UseIndexPool;
    DelegateCI.TextureAtlasDim   = m_UseTextureAtlas ? 2048 : 0;
    m_Stage.RenderDelegate       = USD::HnRenderDelegate::Create(DelegateCI);
    m_Stage.RenderIndex.reset(pxr::HdRenderIndex::New(m_Stage.RenderDelegate.get(), pxr::HdDriverVector{}));

    const pxr::SdfPath SceneDelegateId = pxr::SdfPath::AbsoluteRootPath();

    m_Stage.ImagingDelegate = std::make_unique<pxr::UsdImagingDelegate>(m_Stage.RenderIndex.get(), SceneDelegateId);
    m_Stage.ImagingDelegate->Populate(m_Stage.Stage->GetPseudoRoot());

    const pxr::SdfPath TaskManagerId = SceneDelegateId.AppendChild(pxr::TfToken{"_HnTaskManager_"});
    m_Stage.TaskManager              = std::make_unique<USD::HnTaskManager>(*m_Stage.RenderIndex, TaskManagerId);

    const pxr::SdfPath FinalColorTargetId = SceneDelegateId.AppendChild(pxr::TfToken{"_HnFinalColorTarget_"});
    m_Stage.RenderIndex->InsertBprim(pxr::HdPrimTypeTokens->renderBuffer, m_Stage.ImagingDelegate.get(), FinalColorTargetId);
    m_Stage.FinalColorTarget = static_cast<USD::HnRenderBuffer*>(m_Stage.RenderIndex->GetBprim(pxr::HdPrimTypeTokens->renderBuffer, FinalColorTargetId));
    VERIFY_EXPR(m_Stage.FinalColorTarget != nullptr);

    const pxr::SdfPath CameraId = SceneDelegateId.AppendChild(pxr::TfToken{"_HnCamera_"});
    m_Stage.RenderIndex->InsertSprim(pxr::HdPrimTypeTokens->camera, m_Stage.ImagingDelegate.get(), CameraId);
    m_Stage.Camera = static_cast<USD::HnCamera*>(m_Stage.RenderIndex->GetSprim(pxr::HdPrimTypeTokens->camera, CameraId));
    VERIFY_EXPR(m_Stage.Camera != nullptr);

    const pxr::SdfPath LightId = SceneDelegateId.AppendChild(pxr::TfToken{"_HnLight_"});
    m_Stage.RenderIndex->InsertSprim(pxr::HdPrimTypeTokens->light, m_Stage.ImagingDelegate.get(), LightId);
    m_Stage.Light = static_cast<USD::HnLight*>(m_Stage.RenderIndex->GetSprim(pxr::HdPrimTypeTokens->light, LightId));
    VERIFY_EXPR(m_Stage.Light != nullptr);

    m_Stage.RenderDelegate->GetUSDRenderer()->PrecomputeCubemaps(m_pImmediateContext, m_EnvironmentMapSRV);

    m_FrameParams                    = {};
    m_FrameParams.State.FrontFaceCCW = true;
    m_FrameParams.FinalColorTargetId = FinalColorTargetId;
    m_FrameParams.CameraId           = CameraId;
    m_Stage.TaskManager->SetFrameParams(m_FrameParams);

    m_RenderParams.SelectedPrimId = {};
    m_Stage.TaskManager->SetRenderRprimParams(m_RenderParams);

    m_PostProcessParams = {};
    m_Stage.TaskManager->SetPostProcessParams(m_PostProcessParams);

    const pxr::TfToken UpAxis = pxr::UsdGeomGetStageUpAxis(m_Stage.Stage);
    m_Stage.RootTransform     = GetUpAxisTransform(UpAxis);
    m_Stage.ImagingDelegate->SetRootTransform(USD::ToGfMatrix4d(m_Stage.RootTransform));

    float                SceneExtent = 100;
    const pxr::GfRange3d SceneAABB   = ComputeStageAABB(*m_Stage.Stage);
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
        m_Camera.SetDist(SceneExtent * 2.f);
        m_Camera.SetDistRange(SceneExtent * 0.01f, SceneExtent * 10.f);
    }
    else
    {
        m_Camera.SetDist(300.f);
        m_Camera.SetDistRange(0.1f, 10000.f);
    }
    UpdateCamera();

    USD::HnRenderAxesTaskParams RenderAxesParams;
    RenderAxesParams.Transform = float4x4::Scale(SceneExtent * 2.f) * m_Stage.RootTransform;
    m_Stage.TaskManager->SetRenderAxesParams(RenderAxesParams);

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
}

// Render a frame
void USDViewer::Render()
{
    if (!m_Stage)
        return;

    m_Stage.Camera->SetViewMatrix(m_CameraView);
    m_Stage.Camera->SetProjectionMatrix(m_CameraProj);

    m_Stage.Light->SetDirection(m_LightDirection);
    m_Stage.Light->SetIntensity(m_LightColor * m_LightIntensity);

    m_Stage.FinalColorTarget->SetTarget(m_pSwapChain->GetCurrentBackBufferRTV());

    pxr::HdTaskSharedPtrVector tasks = m_Stage.TaskManager->GetTasks();
    m_Engine.Execute(m_Stage.RenderIndex.get(), &tasks);

    m_Stage.FinalColorTarget->ReleaseTarget();

    const auto& CtxStats    = m_pImmediateContext->GetStats();
    m_Stats.NumDrawCommands = CtxStats.CommandCounters.Draw + CtxStats.CommandCounters.DrawIndexed;
    m_Stats.NumPSOChanges   = CtxStats.CommandCounters.SetPipelineState;
    m_Stats.NumSRBChanges   = CtxStats.CommandCounters.CommitShaderResources;
    m_Stats.NumVBChanges    = CtxStats.CommandCounters.SetVertexBuffers;
    m_Stats.NumIBChanges    = CtxStats.CommandCounters.SetIndexBuffer;
    m_Stats.NumBufferMaps   = CtxStats.CommandCounters.MapBuffer;
    m_Stats.NumTriangles    = CtxStats.GetTotalTriangleCount();
    m_Stats.NumLines        = CtxStats.GetTotalLineCount();
    m_Stats.NumPoints       = CtxStats.GetTotalPointCount();
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
    if (NodeOpen)
    {
        for (const auto& Prop : Prim.GetProperties())
        {
            ImGui::TextDisabled("%s", Prop.GetName().GetText());
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

    pxr::GfMatrix4d ParentGlobalXForm  = GetPrimGlobalTransform(Prim.GetParent());
    float4x4        ParentGlobalMatrix = USD::ToFloat4x4(ParentGlobalXForm);

    ParentGlobalMatrix = ParentGlobalMatrix * m_Stage.RootTransform;

    pxr::GfMatrix4d LocalXform       = pxr::GfMatrix4d{1.0};
    bool            ResetsXformStack = false;
    XFormable.GetLocalTransformation(&LocalXform, &ResetsXformStack);

    float4x4 NewGlobalMatrix = float4x4::MakeMatrix(LocalXform.data()) * ParentGlobalMatrix;

    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
    ImGuizmo::OPERATION GizmoOperation = static_cast<ImGuizmo::OPERATION>(static_cast<int>(ImGuizmo::UNIVERSAL) & ~static_cast<int>(ImGuizmo::ROTATE_SCREEN));
    ImGuizmo::MODE      GizmoMode      = ImGuizmo::LOCAL;
    // NOTE: it is important that ImGuizmo operates on a matrix without reflections, otherwise
    //       rotation will be flipped.
    if (ImGuizmo::Manipulate(m_CameraView.Data(), m_CameraProj.Data(), GizmoOperation, GizmoMode, NewGlobalMatrix.Data(), NULL, NULL))
    {
        // New local matrix is the delta between new global matrix and parent global matrix
        float4x4 NewLocalMatrix = NewGlobalMatrix * ParentGlobalMatrix.Inverse();
        XFormable.MakeMatrixXform().Set(USD::ToGfMatrix4d(NewLocalMatrix));
    }
}

void USDViewer::UpdateUI()
{
    bool UpdateRenderParams      = false;
    bool UpdateFrameParams       = false;
    bool UpdatePostProcessParams = false;

    ImGuizmo::BeginFrame();

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 550), ImGuiCond_FirstUseEver);
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

#ifdef PLATFORM_WIN32
                if (ImGui::Button("Load model"))
                {
                    FileDialogAttribs OpenDialogAttribs{FILE_DIALOG_TYPE_OPEN};
                    OpenDialogAttribs.Title  = "Select USD file";
                    OpenDialogAttribs.Filter = "USD files\0*.usd;*.usdc;*.usdz;*.usda\0";
                    auto FileName            = FileSystem::FileDialog(OpenDialogAttribs);
                    if (!FileName.empty())
                    {
                        m_UsdFileName = std::move(FileName);
                        LoadStage();
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

                ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
                if (ImGui::TreeNode("Lighting"))
                {
                    ImGui::ColorEdit3("Light Color", &m_LightColor.r);

                    ImGui::SliderFloat("Light Intensity", &m_LightIntensity, 0.f, 50.f);
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
                        static_assert(static_cast<size_t>(PBR_Renderer::DebugViewType::NumDebugViews) == 33, "Did you add a new debug view mode? You may want to handle it here");

                        if (ImGui::Combo("Debug view", &m_DebugViewMode, DebugViews.data(), static_cast<int>(DebugViews.size())))
                        {
                            m_Stage.RenderDelegate->SetDebugView(static_cast<PBR_Renderer::DebugViewType>(m_DebugViewMode));
                        }
                    }

                    {
                        std::array<const char*, static_cast<size_t>(USD::HN_RENDER_MODE_COUNT)> RenderModes;
                        RenderModes[USD::HN_RENDER_MODE_SOLID]      = "Solid";
                        RenderModes[USD::HN_RENDER_MODE_MESH_EDGES] = "Edges";
                        RenderModes[USD::HN_RENDER_MODE_POINTS]     = "Points";
                        static_assert(USD::HN_RENDER_MODE_COUNT == 3, "Did you add a new render mode? You may want to handle it here");

                        int RenderMode = m_RenderParams.RenderMode;
                        if (ImGui::Combo("Render mode", &RenderMode, RenderModes.data(), static_cast<int>(RenderModes.size())))
                        {
                            m_RenderParams.RenderMode = static_cast<USD::HN_RENDER_MODE>(RenderMode);
                            UpdateRenderParams        = true;
                        }
                    }

                    if (ImGui::SliderFloat("Selection outline width", &m_PostProcessParams.SelectionOutlineWidth, 1.f, 16.f))
                        UpdatePostProcessParams = true;

                    ImGui::TreePop();
                }

                ImGui::Spacing();

                ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
                if (ImGui::TreeNode("Tone mapping"))
                {
                    {
                        std::array<const char*, 8> ToneMappingMode;
                        ToneMappingMode[TONE_MAPPING_MODE_NONE]         = "None";
                        ToneMappingMode[TONE_MAPPING_MODE_EXP]          = "Exp";
                        ToneMappingMode[TONE_MAPPING_MODE_REINHARD]     = "Reinhard";
                        ToneMappingMode[TONE_MAPPING_MODE_REINHARD_MOD] = "Reinhard Mod";
                        ToneMappingMode[TONE_MAPPING_MODE_UNCHARTED2]   = "Uncharted 2";
                        ToneMappingMode[TONE_MAPPING_FILMIC_ALU]        = "Filmic ALU";
                        ToneMappingMode[TONE_MAPPING_LOGARITHMIC]       = "Logarithmic";
                        ToneMappingMode[TONE_MAPPING_ADAPTIVE_LOG]      = "Adaptive log";
                        if (ImGui::Combo("Tone Mapping Mode", &m_PostProcessParams.ToneMappingMode, ToneMappingMode.data(), static_cast<int>(ToneMappingMode.size())))
                            UpdatePostProcessParams = true;
                    }

                    if (ImGui::SliderFloat("Average log lum", &m_PostProcessParams.AverageLogLum, 0.01f, 10.0f))
                        UpdatePostProcessParams = true;
                    if (ImGui::SliderFloat("Middle gray", &m_PostProcessParams.MiddleGray, 0.01f, 1.0f))
                        UpdatePostProcessParams = true;
                    if (ImGui::SliderFloat("White point", &m_PostProcessParams.WhitePoint, 0.1f, 20.0f))
                        UpdatePostProcessParams = true;

                    if (ImGui::Button("Reset"))
                    {
                        static constexpr USD::HnPostProcessTaskParams DefaultParams;
                        m_PostProcessParams.ToneMappingMode = TONE_MAPPING_MODE_UNCHARTED2;
                        m_PostProcessParams.AverageLogLum   = DefaultParams.AverageLogLum;
                        m_PostProcessParams.MiddleGray      = DefaultParams.MiddleGray;
                        m_PostProcessParams.WhitePoint      = DefaultParams.WhitePoint;
                        UpdatePostProcessParams             = true;
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
                        bool Enabled = m_Stage.TaskManager->AreAxesEnabled();
                        if (ImGui::Checkbox("Axes", &Enabled))
                            m_Stage.TaskManager->EnableAxes(Enabled);
                    }

                    ImGui::TreePop();
                }

                ImGui::PopItemWidth();

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Stats"))
            {
                const auto MemoryStats = m_Stage.RenderDelegate->GetMemoryStats();
                ImGui::TextDisabled("Num draws\n"
                                    "Tris\n"
                                    "Lines\n"
                                    "Points\n"
                                    "State Changes\n"
                                    "  PSO\n"
                                    "  SRB\n"
                                    "  VB\n"
                                    "  IB\n"
                                    "Buffer maps\n"
                                    "Memory Usage\n"
                                    "  Vertex Pool\n"
                                    "  Index Pool\n"
                                    "  Atlas");
                ImGui::SameLine();

                const std::string VertPoolCommittedSizeStr = GetMemorySizeString(MemoryStats.VertexPool.CommittedSize);
                const std::string VertPoolUsedSizeStr      = GetMemorySizeString(MemoryStats.VertexPool.UsedSize);
                const std::string IndPoolCommittedSizeStr  = GetMemorySizeString(MemoryStats.IndexPool.CommittedSize).c_str();
                const std::string IndPoolUsedSizeStr       = GetMemorySizeString(MemoryStats.IndexPool.UsedSize).c_str();
                const std::string AtlasCommittedSizeStr    = GetMemorySizeString(MemoryStats.Atlas.CommittedSize).c_str();
                ImGui::TextDisabled("%d\n"
                                    "%d\n"
                                    "%d\n"
                                    "%d\n"
                                    "\n"
                                    "%d\n"
                                    "%d\n"
                                    "%d\n"
                                    "%d\n"
                                    "%d\n"
                                    "\n"
                                    "%s / %s (%d allocs, %dK verts)\n"
                                    "%s / %s (%d allocs)\n"
                                    "%s (%.1lf%%, %d allocs)",
                                    m_Stats.NumDrawCommands,
                                    m_Stats.NumTriangles,
                                    m_Stats.NumLines,
                                    m_Stats.NumPoints,
                                    m_Stats.NumPSOChanges,
                                    m_Stats.NumSRBChanges,
                                    m_Stats.NumVBChanges,
                                    m_Stats.NumIBChanges,
                                    m_Stats.NumBufferMaps,
                                    VertPoolUsedSizeStr.c_str(), VertPoolCommittedSizeStr.c_str(), MemoryStats.VertexPool.AllocationCount, MemoryStats.VertexPool.AllocatedVertexCount / 1000,
                                    IndPoolUsedSizeStr.c_str(), IndPoolCommittedSizeStr.c_str(), MemoryStats.IndexPool.AllocationCount,
                                    AtlasCommittedSizeStr.c_str(), static_cast<double>(MemoryStats.Atlas.AllocatedTexels) / static_cast<double>(std::max(MemoryStats.Atlas.TotalTexels, Uint64{1})) * 100.0, MemoryStats.Atlas.AllocationCount);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
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

    // Get projection matrix adjusted to the current screen orientation
    m_CameraProj = GetAdjustedProjectionMatrix(PI_F / 4.0f, CameraDist / 100.f, CameraDist * 3.f);
}

void USDViewer::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    m_Camera.SetZoomSpeed(m_Camera.GetDist() * 0.1f);
    m_Camera.Update(m_InputController);
    UpdateCamera();
    // Update camera first as TRS widget needs camera view/proj matrices.
    UpdateUI();

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
        if (m_pDevice->GetDeviceInfo().IsGLDevice())
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

    m_Stage.SelectedPrimId                             = SelectedPrimId;
    m_RenderParams.SelectedPrimId                      = m_Stage.SelectedPrimId;
    m_PostProcessParams.NonselectionDesaturationFactor = !m_Stage.SelectedPrimId.IsEmpty() ? 0.5f : 0.f;
    m_Stage.TaskManager->SetRenderRprimParams(m_RenderParams);
    m_Stage.TaskManager->SetPostProcessParams(m_PostProcessParams);
}

} // namespace Diligent
