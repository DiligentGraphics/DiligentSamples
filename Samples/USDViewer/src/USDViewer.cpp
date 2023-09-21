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

#include "CommandLineParser.hpp"
#include "GraphicsUtilities.h"
#include "MapHelper.hpp"
#include "TextureUtilities.h"
#include "PBR_Renderer.hpp"
#include "FileSystem.hpp"

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
}

} // namespace HLSL

SampleBase* CreateSample()
{
    return new USDViewer();
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

    const auto& SCDesc = m_pSwapChain->GetDesc();

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

    USD::HnRendererCreateInfo CI;
    CI.FrontCCW            = true;
    CI.RTVFormat           = SCDesc.ColorBufferFormat;
    CI.DSVFormat           = SCDesc.DepthBufferFormat;
    CI.pCameraAttribsCB    = m_CameraAttribsCB;
    CI.pLightAttribsCB     = m_LightAttribsCB;
    CI.ConvertOutputToSRGB = m_ConvertPSOutputToGamma;

    USD::CreateHnRenderer(m_pDevice, m_pImmediateContext, CI, &m_Renderer);
    m_Renderer->SetEnvironmentMap(m_pImmediateContext, m_EnvironmentMapSRV);

    if (m_UsdFileName.empty())
        m_UsdFileName = "cube.usd";

    m_Camera.SetDistRange(1, 1000);
    m_Camera.SetDefaultDistance(100);
    m_Camera.SetZoomSpeed(10);
    m_Camera.ResetDefaults();
    m_Camera.SetExtraRotation(QuaternionF::RotationFromAxisAngle(float3{0.75, 0.0, 0.75}, PI_F));

    float4x4 InvYAxis       = float4x4::Identity();
    InvYAxis._22            = -1;
    m_DrawAttribs.Transform = InvYAxis;

    LoadStage();
}

void USDViewer::LoadStage()
{
    m_Stage = pxr::UsdStage::Open(m_UsdFileName.c_str());
    if (!m_Stage)
    {
        LOG_ERROR_MESSAGE("Failed to open USD stage '", m_UsdFileName, "'");
        return;
    }

    m_Renderer->LoadUSDStage(m_Stage);
}

// Render a frame
void USDViewer::Render()
{
    // Clear the back buffer
    const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
    // Let the engine perform required state transitions
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    auto CameraView = m_Camera.GetRotation().ToMatrix() * float4x4::Translation(0.f, 0.0f, m_Camera.GetDist());
    // Apply pretransform matrix that rotates the scene according the surface orientation
    CameraView *= GetSurfacePretransformMatrix(float3{0, 0, 1});

    float4x4 CameraWorld = CameraView.Inverse();

    // Get projection matrix adjusted to the current screen orientation
    const auto CameraProj     = GetAdjustedProjectionMatrix(PI_F / 4.0f, 1.f, 1000.f);
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

    m_Renderer->Draw(m_pImmediateContext, m_DrawAttribs);
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
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::TreeNode("Renderer"))
        {
#ifdef PLATFORM_WIN32
            if (ImGui::Button("Load Stage"))
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
            if (ImGui::TreeNode("Lighting"))
            {
                ImGui::ColorEdit3("Light Color", &m_LightColor.r);
                // clang-format off
                ImGui::SliderFloat("Light Intensity",    &m_LightIntensity,                0.f, 50.f);
                ImGui::SliderFloat("Occlusion strength", &m_DrawAttribs.OcclusionStrength, 0.f,  1.f);
                ImGui::SliderFloat("Emission scale",     &m_DrawAttribs.EmissionScale,     0.f,  1.f);
                ImGui::SliderFloat("IBL scale",          &m_DrawAttribs.IBLScale,          0.f,  1.f);
                // clang-format on
                ImGui::TreePop();
            }

            ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
            if (ImGui::TreeNode("Tone mapping"))
            {
                // clang-format off
                ImGui::SliderFloat("Average log lum", &m_DrawAttribs.AverageLogLum,     0.01f, 10.0f);
                ImGui::SliderFloat("Middle gray",     &m_DrawAttribs.MiddleGray,        0.01f,  1.0f);
                ImGui::SliderFloat("White point",     &m_DrawAttribs.WhitePoint,        0.1f,  20.0f);
                // clang-format on
                ImGui::TreePop();
            }

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

                ImGui::Combo("Debug view", &m_DrawAttribs.DebugView, DebugViews.data(), static_cast<int>(DebugViews.size()));
            }

            {
                std::array<const char*, static_cast<size_t>(USD::HN_RENDER_MODE_COUNT)> RenderModes;
                RenderModes[USD::HN_RENDER_MODE_SOLID]      = "Solid";
                RenderModes[USD::HN_RENDER_MODE_MESH_EDGES] = "Edges";
                static_assert(USD::HN_RENDER_MODE_COUNT == 2, "Did you add a new render mode? You may want to handle it here");

                int RenderMode = m_DrawAttribs.RenderMode;
                ImGui::Combo("Render mode", &RenderMode, RenderModes.data(), static_cast<int>(RenderModes.size()));
                m_DrawAttribs.RenderMode = static_cast<USD::HN_RENDER_MODE>(RenderMode);
            }
            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::TreeNode("Scene"))
        {
            if (m_Stage)
            {
                ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
                for (auto Prim : m_Stage->GetPseudoRoot().GetAllChildren())
                    PopulateSceneTree(m_Stage, Prim);
            }
            ImGui::TreePop();
        }
    }

    ImGui::End();
}


void USDViewer::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();
    m_Renderer->Update();
    m_Camera.Update(m_InputController);
}

} // namespace Diligent
