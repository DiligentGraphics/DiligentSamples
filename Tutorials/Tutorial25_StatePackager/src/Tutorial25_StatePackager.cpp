/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#include "Tutorial25_StatePackager.hpp"

#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "Dearchiver.h"
#include "imgui.h"
#include "FileWrapper.hpp"
#include "DataBlobImpl.hpp"
#include "CallbackWrapper.hpp"

namespace Diligent
{

namespace
{

namespace HLSL
{

#include "../assets/structures.fxh"

}

} // namespace

SampleBase* CreateSample()
{
    return new Tutorial25_StatePackager();
}

void Tutorial25_StatePackager::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);

    Attribs.EngineCI.Features.ComputeShaders = DEVICE_FEATURE_STATE_ENABLED;

    // We do not need the depth buffer from the swap chain in this sample
    Attribs.SCDesc.DepthBufferFormat = TEX_FORMAT_UNKNOWN;
}


void Tutorial25_StatePackager::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::Checkbox("Next event estimation", &m_bUseNextEventEstimation))
            m_SampleCount = 0;

        if (ImGui::SliderInt("Num bounces", &m_NumBounces, 1, 8))
            m_SampleCount = 0;

        if (ImGui::SliderInt("Samples per frame", &m_NumSamplesPerFrame, 1, 32))
            m_SampleCount = 0;
    }
    ImGui::End();
}

void Tutorial25_StatePackager::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    CreateUniformBuffer(m_pDevice, sizeof(HLSL::ShaderConstants), "Shader constants CB", &m_pShaderConstantsCB);

    RefCntAutoPtr<IDearchiver> pDearchiver;
    DearchiverCreateInfo       DearchiverCI{};
    m_pEngineFactory->CreateDearchiver(DearchiverCI, &pDearchiver);

    FileWrapper pArchive{"StateArchive.bin"};
    VERIFY_EXPR(pArchive);
    auto pArchiveData = DataBlobImpl::Create();
    pArchive->Read(pArchiveData);
    VERIFY_EXPR(pArchiveData);
    pDearchiver->LoadArchive(pArchiveData);

    {
        PipelineStateUnpackInfo UnpackInfo;
        UnpackInfo.pDevice      = m_pDevice;
        UnpackInfo.PipelineType = PIPELINE_TYPE_GRAPHICS;
        UnpackInfo.Name         = "G-Buffer PSO";

        auto ModifyGBufferPSODesc = MakeCallback(
            [](PipelineStateCreateInfo& PSODesc) {
                auto& GraphicsPSOCI    = static_cast<GraphicsPipelineStateCreateInfo&>(PSODesc);
                auto& GraphicsPipeline = GraphicsPSOCI.GraphicsPipeline;

                GraphicsPipeline.NumRenderTargets = 3;
                GraphicsPipeline.RTVFormats[0]    = GBuffer::AlbedoFormat;
                GraphicsPipeline.RTVFormats[1]    = GBuffer::NormalFormat;
                GraphicsPipeline.RTVFormats[2]    = GBuffer::DepthFormat;
                GraphicsPipeline.DSVFormat        = TEX_FORMAT_UNKNOWN;
            });

        UnpackInfo.ModifyPipelineStateCreateInfo = ModifyGBufferPSODesc;
        UnpackInfo.pUserData                     = ModifyGBufferPSODesc;
        pDearchiver->UnpackPipelineState(UnpackInfo, &m_pGBufferPSO);
        VERIFY_EXPR(m_pGBufferPSO);

        m_pGBufferPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "cbConstants")->Set(m_pShaderConstantsCB);
        m_pGBufferPSO->CreateShaderResourceBinding(&m_pGBufferSRB, true);
        VERIFY_EXPR(m_pGBufferSRB);
    }

    {
        PipelineStateUnpackInfo UnpackInfo;
        UnpackInfo.pDevice      = m_pDevice;
        UnpackInfo.PipelineType = PIPELINE_TYPE_COMPUTE;
        UnpackInfo.Name         = "Path Trace PSO";
        pDearchiver->UnpackPipelineState(UnpackInfo, &m_pPathTracePSO);
        VERIFY_EXPR(m_pPathTracePSO);

        m_pPathTracePSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "cbConstants")->Set(m_pShaderConstantsCB);
    }

    {
        PipelineStateUnpackInfo UnpackInfo;
        UnpackInfo.pDevice      = m_pDevice;
        UnpackInfo.PipelineType = PIPELINE_TYPE_GRAPHICS;
        UnpackInfo.Name         = "Resolve PSO";

        auto ModifyResolvePSODesc = MakeCallback(
            [this](PipelineStateCreateInfo& PSODesc) {
                auto& GraphicsPSOCI    = static_cast<GraphicsPipelineStateCreateInfo&>(PSODesc);
                auto& GraphicsPipeline = GraphicsPSOCI.GraphicsPipeline;

                GraphicsPipeline.NumRenderTargets = 1;
                GraphicsPipeline.RTVFormats[0]    = m_pSwapChain->GetDesc().ColorBufferFormat;
                GraphicsPipeline.DSVFormat        = m_pSwapChain->GetDesc().DepthBufferFormat;
            });

        UnpackInfo.ModifyPipelineStateCreateInfo = ModifyResolvePSODesc;
        UnpackInfo.pUserData                     = ModifyResolvePSODesc;
        pDearchiver->UnpackPipelineState(UnpackInfo, &m_pResolvePSO);
        VERIFY_EXPR(m_pResolvePSO);

        m_pResolvePSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "cbConstants")->Set(m_pShaderConstantsCB);
    }

    m_Camera.SetPos(float3(0.0f, 1.0f, -20.0f));
    m_Camera.SetRotationSpeed(0.002f);
    m_Camera.SetMoveSpeed(5.f);
    m_Camera.SetSpeedUpScales(5.f, 10.f);
}

void Tutorial25_StatePackager::WindowResize(Uint32 Width, Uint32 Height)
{
    m_GBuffer = {};
    m_pPathTraceSRB.Release();
    m_pResolveSRB.Release();
    m_pRadianceAccumulationBuffer.Release();

    float NearPlane   = 0.1f;
    float FarPlane    = 50.f;
    float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
    m_Camera.SetProjAttribs(NearPlane, FarPlane, AspectRatio, PI_F / 4.f,
                            m_pSwapChain->GetDesc().PreTransform, m_pDevice->GetDeviceInfo().IsGLDevice());
}

void Tutorial25_StatePackager::CreateGBuffer()
{
    const auto& SCDesc = m_pSwapChain->GetDesc();

    // Create window-size offscreen render target
    TextureDesc TexDesc;
    TexDesc.Name      = "G-buffer Albedo";
    TexDesc.Type      = RESOURCE_DIM_TEX_2D;
    TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    TexDesc.Format    = GBuffer::AlbedoFormat;
    TexDesc.Width     = SCDesc.Width;
    TexDesc.Height    = SCDesc.Height;
    TexDesc.MipLevels = 1;

    m_pDevice->CreateTexture(TexDesc, nullptr, &m_GBuffer.pAlbedo);
    VERIFY_EXPR(m_GBuffer.pAlbedo);

    TexDesc.Name   = "G-buffer normal";
    TexDesc.Format = GBuffer::NormalFormat;
    m_pDevice->CreateTexture(TexDesc, nullptr, &m_GBuffer.pNormal);
    VERIFY_EXPR(m_GBuffer.pNormal);

    TexDesc.Name      = "G-buffer depth";
    TexDesc.Format    = GBuffer::DepthFormat;
    TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    m_pDevice->CreateTexture(TexDesc, nullptr, &m_GBuffer.pDepth);
    VERIFY_EXPR(m_GBuffer.pDepth);

    // Create the radiance accumulation buffer
    TexDesc.Name      = "Radiance accumulation buffer";
    TexDesc.Format    = RadianceAccumulationFormat;
    TexDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    m_pDevice->CreateTexture(TexDesc, nullptr, &m_pRadianceAccumulationBuffer);
    VERIFY_EXPR(m_pRadianceAccumulationBuffer);

    m_pPathTraceSRB.Release();
    m_pPathTracePSO->CreateShaderResourceBinding(&m_pPathTraceSRB, true);
    m_pPathTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Albedo")->Set(m_GBuffer.pAlbedo->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    m_pPathTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Normal")->Set(m_GBuffer.pNormal->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    m_pPathTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Depth")->Set(m_GBuffer.pDepth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    m_pPathTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Radiance")->Set(m_pRadianceAccumulationBuffer->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));

    m_pResolveSRB.Release();
    m_pResolvePSO->CreateShaderResourceBinding(&m_pResolveSRB, true);
    m_pResolveSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Radiance")->Set(m_pRadianceAccumulationBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

    m_SampleCount       = 0;
    m_LastFrameViewProj = {};
}

// Render a frame
void Tutorial25_StatePackager::Render()
{
    if (!m_GBuffer)
        CreateGBuffer();

    const auto& SCDesc = m_pSwapChain->GetDesc();

    bool UpdateGBuffer = false;
    {
        MapHelper<HLSL::ShaderConstants> ShaderData{m_pImmediateContext, m_pShaderConstantsCB, MAP_WRITE, MAP_FLAG_DISCARD};
        ShaderData->uScreenWidth  = SCDesc.Width;
        ShaderData->uScreenHeight = SCDesc.Height;
        ShaderData->fScreenWidth  = static_cast<float>(SCDesc.Width);
        ShaderData->fScreenHeight = static_cast<float>(SCDesc.Height);

        const auto& View     = m_Camera.GetViewMatrix();
        const auto& Proj     = m_Camera.GetProjMatrix();
        const auto  ViewProj = View * Proj;

        if (m_LastFrameViewProj != ViewProj)
        {
            m_SampleCount       = 0;
            m_LastFrameViewProj = ViewProj;
            UpdateGBuffer       = true;
        }

        ShaderData->fLastSampleCount = static_cast<float>(m_SampleCount);
        m_SampleCount += m_NumSamplesPerFrame;
        ShaderData->fNewSampleCount = static_cast<float>(m_SampleCount);

        ShaderData->iNumBounces         = m_NumBounces;
        ShaderData->iNumSamplesPerFrame = m_NumSamplesPerFrame;

        ShaderData->ViewProjMat    = ViewProj.Transpose();
        ShaderData->ViewProjInvMat = ViewProj.Inverse().Transpose();
    }

    // Draw the scene into G-buffer
    if (UpdateGBuffer)
    {
        ITextureView* ppRTVs[] = {
            m_GBuffer.pAlbedo->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),
            m_GBuffer.pNormal->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),
            m_GBuffer.pDepth->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET) //
        };
        m_pImmediateContext->SetRenderTargets(_countof(ppRTVs), ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_pImmediateContext->CommitShaderResources(m_pGBufferSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->SetPipelineState(m_pGBufferPSO);
        m_pImmediateContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
    }

    // Path trace
    {
        static constexpr Uint32 ThreadGroupSize = 8;

        m_pImmediateContext->SetPipelineState(m_pPathTracePSO);
        m_pImmediateContext->CommitShaderResources(m_pPathTraceSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DispatchComputeAttribs DispatchArgs{(SCDesc.Width + ThreadGroupSize - 1) / ThreadGroupSize, (SCDesc.Height + ThreadGroupSize - 1) / ThreadGroupSize};
        m_pImmediateContext->DispatchCompute(DispatchArgs);
    }

    // Resolve
    {
        ITextureView* ppRTVs[] = {m_pSwapChain->GetCurrentBackBufferRTV()};
        m_pImmediateContext->SetRenderTargets(_countof(ppRTVs), ppRTVs, m_pSwapChain->GetDepthBufferDSV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_pImmediateContext->SetPipelineState(m_pResolvePSO);
        m_pImmediateContext->CommitShaderResources(m_pResolveSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_pImmediateContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
    }
}

void Tutorial25_StatePackager::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));
}

} // namespace Diligent
