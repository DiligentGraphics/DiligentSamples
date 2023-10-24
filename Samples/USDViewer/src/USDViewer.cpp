/*
 *  Copyright 2023 Diligent Graphics LLC
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
#include "PBR_Renderer.hpp"
#include "FileSystem.hpp"
#include "Tasks/HnReadRprimIdTask.hpp"
#include "Tasks/HnSetupRenderingTask.hpp"

#include "imgui.h"
#include "ImGuiUtils.hpp"

#include "pxr/usd/usd/prim.h"
#include "pxr/usd/usd/property.h"

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

SampleBase::CommandLineStatus USDViewer::ProcessCommandLine(int argc, const char* const* argv)
{
    CommandLineParser ArgsParser{argc, argv};
    ArgsParser.Parse("usd_path", 'u', m_UsdFileName);
    ArgsParser.Parse("usd_plugin", 'p', m_UsdPluginRoot);
    return CommandLineStatus::OK;
}

void USDViewer::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    CreateUniformBuffer(m_pDevice, sizeof(HLSL::CameraAttribs), "Camera attribs buffer", &m_CameraAttribsCB);
    CreateUniformBuffer(m_pDevice, sizeof(HLSL::LightAttribs), "Light attribs buffer", &m_LightAttribsCB);

    RefCntAutoPtr<ITexture> EnvironmentMap;
    CreateTextureFromFile("textures/papermill.ktx", TextureLoadInfo{"Environment map"}, m_pDevice, &EnvironmentMap);
    VERIFY_EXPR(EnvironmentMap);
    m_EnvironmentMapSRV = EnvironmentMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    // clang-format off
    StateTransitionDesc Barriers [] =
    {
        {m_CameraAttribsCB, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
        {m_LightAttribsCB,  RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
        {EnvironmentMap,    RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE}
    };
    // clang-format on
    m_pImmediateContext->TransitionResourceStates(_countof(Barriers), Barriers);

    m_Camera.SetDistRange(1, 10000);
    m_Camera.SetDefaultDistance(100);
    m_Camera.SetZoomSpeed(10);
    m_Camera.ResetDefaults();
    m_Camera.SetExtraRotation(QuaternionF::RotationFromAxisAngle(float3{0.75, 0.0, 0.75}, PI_F));

    float4x4 InvYAxis        = float4x4::Identity();
    InvYAxis._22             = -1;
    m_RenderParams.Transform = InvYAxis;

    m_PostProcessParams.ToneMappingMode     = TONE_MAPPING_MODE_UNCHARTED2;
    m_PostProcessParams.ConvertOutputToSRGB = m_ConvertPSOutputToGamma;

    if (m_UsdFileName.empty())
        m_UsdFileName = "cube.usd";
    LoadStage();
}

void USDViewer::LoadStage()
{
    {
        auto Stage = std::move(m_Stage);
    }

    m_Stage.Stage = pxr::UsdStage::Open(m_UsdFileName.c_str());
    if (!m_Stage.Stage)
    {
        LOG_ERROR_MESSAGE("Failed to open USD stage '", m_UsdFileName, "'");
        return;
    }

    m_Stage.RenderDelegate = USD::HnRenderDelegate::Create({m_pDevice, m_pImmediateContext, nullptr, m_CameraAttribsCB, m_LightAttribsCB});
    m_Stage.RenderIndex.reset(pxr::HdRenderIndex::New(m_Stage.RenderDelegate.get(), pxr::HdDriverVector{}));

    const pxr::SdfPath SceneDelegateId = pxr::SdfPath::AbsoluteRootPath();

    m_Stage.ImagingDelegate = std::make_unique<pxr::UsdImagingDelegate>(m_Stage.RenderIndex.get(), SceneDelegateId);
    m_Stage.ImagingDelegate->Populate(m_Stage.Stage->GetPseudoRoot());

    const pxr::SdfPath TaskManagerId = SceneDelegateId.AppendChild(pxr::TfToken{"_HnTaskManager_"});
    m_Stage.TaskManager              = std::make_unique<USD::HnTaskManager>(*m_Stage.RenderIndex, TaskManagerId);

    m_Stage.FinalColorTargetId = SceneDelegateId.AppendChild(pxr::TfToken{"_HnFinalColorTarget_"});
    m_Stage.RenderIndex->InsertBprim(pxr::HdPrimTypeTokens->renderBuffer, m_Stage.ImagingDelegate.get(), m_Stage.FinalColorTargetId);

    m_Stage.RenderDelegate->GetUSDRenderer()->PrecomputeCubemaps(m_pImmediateContext, m_EnvironmentMapSRV);

    USD::HnSetupRenderingTaskParams SetupRenderingParams;
    SetupRenderingParams.ColorFormat        = ColorBufferFormat;
    SetupRenderingParams.MeshIdFormat       = MeshIdFormat;
    SetupRenderingParams.DepthFormat        = DepthFormat;
    SetupRenderingParams.FrontFaceCCW       = true;
    SetupRenderingParams.FinalColorTargetId = m_Stage.FinalColorTargetId;
    m_Stage.TaskManager->SetTaskParams(USD::HnTaskManager::TaskUID_SetupRendering, SetupRenderingParams);

    m_Stage.TaskManager->SetRenderRprimParams(m_RenderParams);
    m_Stage.TaskManager->SetTaskParams(USD::HnTaskManager::TaskUID_PostProcess, m_PostProcessParams);
}

// Render a frame
void USDViewer::Render()
{
    if (!m_Stage)
        return;

    auto CameraDist = m_Camera.GetDist();
    auto CameraView = m_Camera.GetRotation().ToMatrix() * float4x4::Translation(0.f, 0.0f, CameraDist);
    // Apply pretransform matrix that rotates the scene according the surface orientation
    CameraView *= GetSurfacePretransformMatrix(float3{0, 0, 1});

    float4x4 CameraWorld = CameraView.Inverse();

    // Get projection matrix adjusted to the current screen orientation
    const auto CameraProj     = GetAdjustedProjectionMatrix(PI_F / 4.0f, CameraDist / 100.f, CameraDist * 3.f);
    const auto CameraViewProj = CameraView * CameraProj;
    const auto CameraWorldPos = float3::MakeVector(CameraWorld[3]);

    {
        MapHelper<HLSL::CameraAttribs> CamAttribs{m_pImmediateContext, m_CameraAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD};
        CamAttribs->mProjT        = CameraProj.Transpose();
        CamAttribs->mViewProjT    = CameraViewProj.Transpose();
        CamAttribs->mViewProjInvT = CameraViewProj.Inverse().Transpose();
        CamAttribs->f4Position    = float4{CameraWorldPos, 1};
    }

    {
        MapHelper<HLSL::LightAttribs> LightAttribs{m_pImmediateContext, m_LightAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD};
        LightAttribs->f4Direction = m_LightDirection;
        LightAttribs->f4Intensity = m_LightColor * m_LightIntensity;
    }

    auto* FinalColorTarget = static_cast<USD::HnRenderBuffer*>(m_Stage.RenderIndex->GetBprim(pxr::HdPrimTypeTokens->renderBuffer, m_Stage.FinalColorTargetId));
    VERIFY_EXPR(FinalColorTarget != nullptr);
    FinalColorTarget->SetTarget(m_pSwapChain->GetCurrentBackBufferRTV());

    pxr::HdTaskSharedPtrVector tasks = m_Stage.TaskManager->GetTasks();
    m_Engine.Execute(m_Stage.RenderIndex.get(), &tasks);

    FinalColorTarget->ReleaseTarget();
}

static void PopulateSceneTree(pxr::UsdStageRefPtr& Stage, const pxr::UsdPrim& Prim)
{
    if (ImGui::TreeNode(Prim.GetName().GetText()))
    {
        for (const auto& Prop : Prim.GetProperties())
        {
            ImGui::TextDisabled("%s", Prop.GetName().GetText());
        }

        for (auto Child : Prim.GetAllChildren())
            PopulateSceneTree(Stage, Child);

        ImGui::TreePop();
    }
}

void USDViewer::UpdateUI()
{
    bool UpdateRenderParams      = false;
    bool UpdatePostProcessParams = false;

    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 500), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr))
    {
        if (ImGui::BeginTabBar("##tabs", ImGuiTabBarFlags_None))
        {
            if (ImGui::BeginTabItem("Stage"))
            {
#ifdef PLATFORM_WIN32
                if (ImGui::Button("Load"))
                {
                    FileDialogAttribs OpenDialogAttribs{FILE_DIALOG_TYPE_OPEN};
                    OpenDialogAttribs.Title  = "Select USD file";
                    OpenDialogAttribs.Filter = "USD files\0*.usd;*.usdc;*.usdz\0";
                    auto FileName            = FileSystem::FileDialog(OpenDialogAttribs);
                    if (!FileName.empty())
                    {
                        m_UsdFileName = std::move(FileName);
                        LoadStage();
                    }
                }
#endif

                ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
                if (ImGui::TreeNode("Scene"))
                {
                    if (m_Stage.Stage)
                    {
                        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
                        for (auto Prim : m_Stage.Stage->GetPseudoRoot().GetAllChildren())
                            PopulateSceneTree(m_Stage.Stage, Prim);
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
                    if (ImGui::SliderFloat("Occlusion strength", &m_RenderParams.OcclusionStrength, 0.f, 1.f))
                        UpdateRenderParams = true;
                    if (ImGui::SliderFloat("Emission scale", &m_RenderParams.EmissionScale, 0.f, 1.f))
                        UpdateRenderParams = true;
                    if (ImGui::SliderFloat("IBL scale", &m_RenderParams.IBLScale, 0.f, 1.f))
                        UpdateRenderParams = true;

                    {
                        std::array<const char*, static_cast<size_t>(PBR_Renderer::DebugViewType::NumDebugViews)> DebugViews;

                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::None)]            = "None";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Texcoord0)]       = "Tex coords";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::BaseColor)]       = "Base Color";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Transparency)]    = "Transparency";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::NormalMap)]       = "Normal Map";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Occlusion)]       = "Occlusion";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Emissive)]        = "Emissive";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Metallic)]        = "Metallic";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Roughness)]       = "Roughness";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::DiffuseColor)]    = "Diffuse color";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::SpecularColor)]   = "Specular color (R0)";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::Reflectance90)]   = "Reflectance90";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::MeshNormal)]      = "Mesh normal";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::PerturbedNormal)] = "Perturbed normal";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::NdotV)]           = "n*v";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::DirectLighting)]  = "Direct Lighting";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::DiffuseIBL)]      = "Diffuse IBL";
                        DebugViews[static_cast<size_t>(PBR_Renderer::DebugViewType::SpecularIBL)]     = "Specular IBL";
                        static_assert(static_cast<size_t>(PBR_Renderer::DebugViewType::NumDebugViews) == 18, "Did you add a new debug view mode? You may want to handle it here");

                        if (ImGui::Combo("Debug view", &m_RenderParams.DebugView, DebugViews.data(), static_cast<int>(DebugViews.size())))
                            UpdateRenderParams = true;
                    }

                    {
                        std::array<const char*, static_cast<size_t>(USD::HN_RENDER_MODE_COUNT)> RenderModes;
                        RenderModes[USD::HN_RENDER_MODE_SOLID]      = "Solid";
                        RenderModes[USD::HN_RENDER_MODE_MESH_EDGES] = "Edges";
                        static_assert(USD::HN_RENDER_MODE_COUNT == 2, "Did you add a new render mode? You may want to handle it here");

                        int RenderMode = m_RenderParams.RenderMode;
                        if (ImGui::Combo("Render mode", &RenderMode, RenderModes.data(), static_cast<int>(RenderModes.size())))
                        {
                            m_RenderParams.RenderMode = static_cast<USD::HN_RENDER_MODE>(RenderMode);
                            UpdateRenderParams        = true;
                        }
                    }

                    ImGui::TreePop();
                }

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

                ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
                if (ImGui::TreeNode("Elements"))
                {
                    auto ElementCheckbox = [this](const char* Name, USD::HnTaskManager::TaskUID UID) {
                        bool Enabled = m_Stage.TaskManager->IsTaskEnabled(UID);
                        if (ImGui::Checkbox(Name, &Enabled))
                            m_Stage.TaskManager->EnableTask(UID, Enabled);
                    };
                    ElementCheckbox("Default Material", USD::HnTaskManager::TaskUID_RenderRprimsDefault);
                    ElementCheckbox("Masked Material", USD::HnTaskManager::TaskUID_RenderRprimsMasked);
                    ElementCheckbox("Env map", USD::HnTaskManager::TaskUID_RenderEnvMap);
                    ElementCheckbox("Additive Material", USD::HnTaskManager::TaskUID_RenderRprimsAdditive);
                    ElementCheckbox("Translucent Material", USD::HnTaskManager::TaskUID_RenderRprimsTranslucent);

                    ImGui::TreePop();
                }

                ImGui::PopItemWidth();

                ImGui::EndTabItem();
            }
            ImGui::EndTabBar();
        }
    }
    ImGui::End();

    if (UpdateRenderParams)
        m_Stage.TaskManager->SetRenderRprimParams(m_RenderParams);
    if (UpdatePostProcessParams)
        m_Stage.TaskManager->SetTaskParams(USD::HnTaskManager::TaskUID_PostProcess, m_PostProcessParams);
}

void USDViewer::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();
    m_Camera.Update(m_InputController);

    if (m_Stage)
    {
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
            m_Stage.TaskManager->SetTaskParams(USD::HnTaskManager::TaskUID_ReadRprimId, Params);

            SelectedPrimId = m_Stage.TaskManager->GetSelectedRprimId();
        }

        if (SelectedPrimId != m_SelectedPrimId)
        {
            m_SelectedPrimId                                   = SelectedPrimId;
            m_RenderParams.SelectedPrimId                      = m_SelectedPrimId ? *m_SelectedPrimId : pxr::SdfPath{};
            m_PostProcessParams.NonselectionDesaturationFactor = m_SelectedPrimId != nullptr && !m_SelectedPrimId->IsEmpty() ? 0.5f : 0.f;
            m_Stage.TaskManager->SetRenderRprimParams(m_RenderParams);
            m_Stage.TaskManager->SetTaskParams(USD::HnTaskManager::TaskUID_PostProcess, m_PostProcessParams);
        }


        m_Stage.ImagingDelegate->ApplyPendingUpdates();
    }
}

} // namespace Diligent
