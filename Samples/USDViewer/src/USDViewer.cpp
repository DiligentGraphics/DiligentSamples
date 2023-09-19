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
#include "CommandLineParser.hpp"
#include "GraphicsUtilities.h"
#include "MapHelper.hpp"
#include "TextureUtilities.h"

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
    CI.RTVFormat           = SCDesc.ColorBufferFormat;
    CI.DSVFormat           = SCDesc.DepthBufferFormat;
    CI.pCameraAttribsCB    = m_CameraAttribsCB;
    CI.pLightAttribsCB     = m_LightAttribsCB;
    CI.ConvertOutputToSRGB = m_ConvertPSOutputToGamma;

    USD::CreateHnRenderer(m_pDevice, m_pImmediateContext, CI, &m_Renderer);
    m_Renderer->SetEnvironmentMap(m_pImmediateContext, m_EnvironmentMapSRV);

    if (m_UsdFileName.empty())
        m_UsdFileName = "cube.usd";

    m_Renderer->LoadUSDStage(m_UsdFileName.c_str());

    m_Camera.SetDistRange(1, 1000);
    m_Camera.SetDefaultDistance(100);
    m_Camera.SetZoomSpeed(10);
    m_Camera.ResetDefaults();
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
        MapHelper<HLSL::LightAttribs> LightAttribs(m_pImmediateContext, m_LightAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
        LightAttribs->f4Direction = m_LightDirection;
        LightAttribs->f4Intensity = m_LightColor * m_LightIntensity;
    }

    m_Renderer->Draw(m_pImmediateContext, CameraViewProj);
}

void USDViewer::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    m_Renderer->Update();
    m_Camera.Update(m_InputController);
}

} // namespace Diligent
