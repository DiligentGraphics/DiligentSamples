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

#include "Tutorial23_CommandQueues.hpp"

#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "imgui.h"
#include "ImGuiUtils.hpp"
#include "PlatformMisc.hpp"
#include "ShaderMacroHelper.hpp"

namespace Diligent
{
namespace HLSL
{
#include "../assets/Structures.fxh"

static_assert(sizeof(DrawConstants) % 16 == 0, "must be aligned to 16 bytes");
static_assert(sizeof(PostProcessConstants) % 16 == 0, "must be aligned to 16 bytes");
static_assert(sizeof(TerrainConstants) % 16 == 0, "must be aligned to 16 bytes");
} // namespace HLSL

SampleBase* CreateSample()
{
    return new Tutorial23_CommandQueues();
}

Tutorial23_CommandQueues::~Tutorial23_CommandQueues()
{
    if (m_GraphicsCtxFence)
        m_GraphicsCtxFence->Wait(m_GraphicsCtxFenceValue);
    if (m_ComputeCtxFence)
        m_ComputeCtxFence->Wait(m_ComputeCtxFenceValue);
    if (m_TransferCtxFence)
        m_TransferCtxFence->Wait(m_TransferCtxFenceValue);
}

void Tutorial23_CommandQueues::CreatePostProcessPSO(IShaderSourceInputStreamFactory* pShaderSourceFactory)
{
    // Create PSO for post process pass

    ShaderMacroHelper Macros;
    Macros.AddShaderMacro("GLOW", 1);

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name         = "Post process PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    PSOCreateInfo.GraphicsPipeline.NumRenderTargets                  = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                     = m_pSwapChain->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology                 = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable      = false;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;

    const SamplerDesc SamLinearClampDesc //
        {
            FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
            TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP //
        };
    const ImmutableSamplerDesc ImtblSamplers[] = //
        {
            {SHADER_TYPE_PIXEL, "g_GBuffer_Color", SamLinearClampDesc} //
        };
    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType  = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc       = {"Post process VS", SHADER_TYPE_VERTEX, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "PostProcess.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc       = {"Post process PS", SHADER_TYPE_PIXEL, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "PostProcess.psh";
        ShaderCI.Macros     = Macros;
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_PostProcessPSO[0]);


    Macros.UpdateMacro("GLOW", 0);

    RefCntAutoPtr<IShader> pPSnoGlow;
    {
        ShaderCI.Desc       = {"Post process without glow PS", SHADER_TYPE_PIXEL, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "PostProcess.psh";
        ShaderCI.Macros     = Macros;
        m_pDevice->CreateShader(ShaderCI, &pPSnoGlow);
    }
    PSOCreateInfo.pPS          = pPSnoGlow;
    PSOCreateInfo.PSODesc.Name = "Post process without glow PSO";

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_PostProcessPSO[1]);


    RefCntAutoPtr<IShader> pDownSamplePS;
    {
        ShaderCI.Desc       = {"Down sample PS", SHADER_TYPE_PIXEL, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "DownSample.psh";
        m_pDevice->CreateShader(ShaderCI, &pDownSamplePS);
    }
    PSOCreateInfo.pPS = pDownSamplePS;

    PSOCreateInfo.PSODesc.Name                   = "Down sample PSO";
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = m_ColorTargetFormat;

    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers    = nullptr;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = 0;

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_DownSamplePSO);
}

void Tutorial23_CommandQueues::DownSample()
{
    m_pImmediateContext->BeginDebugGroup("Down sample pass");

    m_pImmediateContext->SetPipelineState(m_DownSamplePSO);
    m_pImmediateContext->SetVertexBuffers(0, 0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(nullptr, 0, RESOURCE_STATE_TRANSITION_MODE_NONE);

    StateTransitionDesc Barrier{m_GBuffer.Color, RESOURCE_STATE_RENDER_TARGET, RESOURCE_STATE_SHADER_RESOURCE, 0u, 1u};

    for (Uint32 Mip = 1; Mip < DownSampleFactor; ++Mip)
    {
        Barrier.FirstMipLevel = Mip - 1;
        m_pImmediateContext->TransitionResourceStates(1, &Barrier);

        m_pImmediateContext->SetRenderTargets(1, &m_GBuffer.ColorRTVs[Mip], nullptr, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

        m_pImmediateContext->CommitShaderResources(m_DownSampleSRB[Mip - 1], RESOURCE_STATE_TRANSITION_MODE_NONE);
        m_pImmediateContext->Draw(DrawAttribs{3, DRAW_FLAG_VERIFY_DRAW_ATTRIBS | DRAW_FLAG_VERIFY_STATES});
    }

    // Transit last mipmap level to SRV state.
    // Now all mipmaps in m_GBuffer.Color are in SRV state, so update resource state.
    Barrier.FirstMipLevel = DownSampleFactor - 1;
    Barrier.Flags         = STATE_TRANSITION_FLAG_UPDATE_STATE;
    m_pImmediateContext->TransitionResourceStates(1, &Barrier);

    m_pImmediateContext->EndDebugGroup(); // Down sample pass
}

void Tutorial23_CommandQueues::PostProcess()
{
    m_pImmediateContext->BeginDebugGroup("Post process");

    const auto ViewProj    = m_Camera.GetViewMatrix() * m_Camera.GetProjMatrix();
    const auto ViewProjInv = ViewProj.Inverse();

    HLSL::PostProcessConstants ConstData;
    ConstData.ViewProjInv = ViewProjInv.Transpose();
    ConstData.CameraPos   = m_Camera.GetPos();
    ConstData.FogColor    = m_FogColor;

    m_pImmediateContext->UpdateBuffer(m_PostProcessConstants, 0, sizeof(ConstData), &ConstData, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);


    m_pImmediateContext->SetPipelineState(m_PostProcessPSO[m_Glow ? 0 : 1]);
    m_pImmediateContext->CommitShaderResources(m_PostProcessSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    m_pImmediateContext->SetVertexBuffers(0, 0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(nullptr, 0, RESOURCE_STATE_TRANSITION_MODE_NONE);

    m_pImmediateContext->Draw(DrawAttribs{3, DRAW_FLAG_VERIFY_ALL});

    m_pImmediateContext->EndDebugGroup(); // Post process
}

void Tutorial23_CommandQueues::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    for (Uint32 i = 1; i < InitInfo.NumImmediateCtx; ++i)
    {
        constexpr auto QueueMask = COMMAND_QUEUE_TYPE_PRIMARY_MASK;
        auto*          Ctx       = InitInfo.ppContexts[i];
        const auto&    Desc      = Ctx->GetDesc();

        if (!m_ComputeCtx &&
            ((Desc.QueueType & QueueMask) == COMMAND_QUEUE_TYPE_COMPUTE ||
             (Desc.QueueType & QueueMask) == COMMAND_QUEUE_TYPE_GRAPHICS))
        {
            m_ComputeCtx = Ctx;
        }
        else if (!m_TransferCtx &&
                 (Desc.QueueType & QueueMask) == COMMAND_QUEUE_TYPE_TRANSFER)
        {
            m_TransferCtx = Ctx;
        }
    }

    // Find supported depth target format.
#if PLATFORM_ANDROID
    // For Android try Depth16 first.
    if (m_pDevice->GetTextureFormatInfoExt(TEX_FORMAT_D16_UNORM).BindFlags & BIND_DEPTH_STENCIL)
        m_DepthTargetFormat = TEX_FORMAT_D16_UNORM;
    else
#endif
    {
        if (m_pDevice->GetTextureFormatInfoExt(TEX_FORMAT_D32_FLOAT).BindFlags & BIND_DEPTH_STENCIL)
            m_DepthTargetFormat = TEX_FORMAT_D32_FLOAT;
        else if (m_pDevice->GetTextureFormatInfoExt(TEX_FORMAT_D24_UNORM_S8_UINT).BindFlags & BIND_DEPTH_STENCIL)
            m_DepthTargetFormat = TEX_FORMAT_D24_UNORM_S8_UINT;
    }

    // Use HDR format if supported.
    constexpr auto RTFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    if ((m_pDevice->GetTextureFormatInfoExt(TEX_FORMAT_RGBA16_FLOAT).BindFlags & RTFlags) == RTFlags)
        m_ColorTargetFormat = TEX_FORMAT_RGBA16_FLOAT;

    // Setup camera.
    m_Camera.SetPos(float3{-73.f, 21.f, 47.f});
    m_Camera.SetRotation(17.f, -0.27f);
    m_Camera.SetRotationSpeed(0.005f);
    m_Camera.SetMoveSpeed(5.f);
    m_Camera.SetSpeedUpScales(5.f, 10.f);

    // Create fence for each context
    const auto DevType = m_pDevice->GetDeviceInfo().Type;
    if (DevType == RENDER_DEVICE_TYPE_D3D12 || DevType == RENDER_DEVICE_TYPE_VULKAN || DevType == RENDER_DEVICE_TYPE_METAL)
    {
        FenceDesc FenceCI;
        FenceCI.Type = FENCE_TYPE_GENERAL;

        FenceCI.Name = "Graphics context fence";
        m_pDevice->CreateFence(FenceCI, &m_GraphicsCtxFence);

        FenceCI.Name = "Compute context fence";
        m_pDevice->CreateFence(FenceCI, &m_ComputeCtxFence);

        if (m_TransferCtx)
        {
            FenceCI.Name = "Transfer context fence";
            m_pDevice->CreateFence(FenceCI, &m_TransferCtxFence);
        }
    }

    if (DevType == RENDER_DEVICE_TYPE_D3D11)
        m_Glow = false; // not supported

    ScenePSOCreateAttribs PSOAttribs;
    PSOAttribs.ColorTargetFormat = m_ColorTargetFormat;
    PSOAttribs.DepthTargetFormat = m_DepthTargetFormat;

    // Settings for high-performance discrete GPUs
    if (m_pDevice->GetAdapterInfo().Type == ADAPTER_TYPE_DISCRETE)
    {
        m_SurfaceScaleExp2           = 1;
        m_TransferRateMbExp2         = 5;
        m_Terrain.TerrainSize        = 11;
        PSOAttribs.TurbulenceOctaves = 6;
        PSOAttribs.NoiseOctaves      = 3;
    }

#if PLATFORM_ANDROID
    // Set settings for low-performance mobile devices
    m_SurfaceScaleExp2    = -1;
    m_Terrain.TerrainSize = 7;
    m_Glow                = false;
#endif

    // Create constant buffers
    BufferDesc BuffDesc;
    BuffDesc.BindFlags            = BIND_UNIFORM_BUFFER;
    BuffDesc.Usage                = USAGE_DEFAULT;
    BuffDesc.Size                 = sizeof(HLSL::PostProcessConstants);
    BuffDesc.ImmediateContextMask = (Uint64{1} << m_pImmediateContext->GetDesc().ContextId);
    BuffDesc.Name                 = "Post process constants";
    m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_PostProcessConstants);

    BuffDesc.Usage                = USAGE_DYNAMIC;
    BuffDesc.CPUAccessFlags       = CPU_ACCESS_WRITE;
    BuffDesc.Size                 = sizeof(HLSL::DrawConstants);
    BuffDesc.ImmediateContextMask = (Uint64{1} << m_pImmediateContext->GetDesc().ContextId);
    BuffDesc.Name                 = "Draw constants";
    m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_DrawConstants);

    m_Buildings.Initialize(m_pDevice, m_DrawConstants, (Uint64{1} << m_pImmediateContext->GetDesc().ContextId) | (m_TransferCtx ? Uint64{1} << m_TransferCtx->GetDesc().ContextId : 0));
    m_Terrain.Initialize(m_pDevice, m_DrawConstants, (Uint64{1} << m_pImmediateContext->GetDesc().ContextId) | (m_ComputeCtx ? Uint64{1} << m_ComputeCtx->GetDesc().ContextId : 0));


    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    PSOAttribs.pShaderSourceFactory = pShaderSourceFactory;

    m_Terrain.CreatePSO(PSOAttribs);
    m_Buildings.CreatePSO(PSOAttribs);

    CreatePostProcessPSO(pShaderSourceFactory);

    m_Buildings.CreateResources(m_pImmediateContext);
    m_Terrain.CreateResources(m_pImmediateContext);

    if (m_pDevice->GetDeviceInfo().Features.TimestampQueries)
    {
        m_Profiler.Initialize(m_pDevice);
    }

    // Signal first value to graphics fence.
    // Compute and transfer contexts will wait for this fence.
    if (m_GraphicsCtxFence)
        m_pImmediateContext->EnqueueSignal(m_GraphicsCtxFence, ++m_GraphicsCtxFenceValue);
    m_pImmediateContext->Flush();

    if (m_ComputeCtx)
    {
        m_UseAsyncCompute = true;
        m_ComputeCtx->Flush();
    }
    if (m_TransferCtx)
    {
        m_UseAsyncTransfer = true;
        m_TransferCtx->Flush();
    }
}

void Tutorial23_CommandQueues::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);

    std::vector<GraphicsAdapterInfo> Adapters;

    Uint32 NumAdapters = 0;
    Attribs.pFactory->EnumerateAdapters(Attribs.EngineCI.GraphicsAPIVersion, NumAdapters, 0);
    if (NumAdapters == 0)
        return;

    // Enumerate adapters and find adapter with multiple queues.
    {
        Adapters.resize(NumAdapters);
        Attribs.pFactory->EnumerateAdapters(Attribs.EngineCI.GraphicsAPIVersion, NumAdapters, Adapters.data());

        Attribs.EngineCI.AdapterId = 0;
        Uint32 NumQueues           = 0;
        for (Uint32 AdapterId = 0; AdapterId < NumAdapters; ++AdapterId)
        {
            auto& Adapter = Adapters[AdapterId];
            if (Adapter.NumQueues > NumQueues)
            {
                Attribs.EngineCI.AdapterId = AdapterId;
                NumQueues                  = Adapter.NumQueues;
            }
        }
    }

    auto AddContext = [&](COMMAND_QUEUE_TYPE Type, const char* Name, Uint32 AdapterId) //
    {
        constexpr auto QueueMask = COMMAND_QUEUE_TYPE_PRIMARY_MASK;
        auto*          Queues    = Adapters[AdapterId].Queues;
        for (Uint32 q = 0, Count = Adapters[AdapterId].NumQueues; q < Count; ++q)
        {
            auto& CurQueue = Queues[q];
            if (CurQueue.MaxDeviceContexts == 0)
                continue;

            if ((CurQueue.QueueType & QueueMask) == Type)
            {
                CurQueue.MaxDeviceContexts -= 1;

                ImmediateContextCreateInfo Ctx{};
                Ctx.QueueId  = static_cast<Uint8>(q);
                Ctx.Name     = Name;
                Ctx.Priority = QUEUE_PRIORITY_MEDIUM;
                m_ContextCI.push_back(Ctx);
                return true;
            }
        }
        return false;
    };

    AddContext(COMMAND_QUEUE_TYPE_GRAPHICS, "Graphics", Attribs.EngineCI.AdapterId);
    AddContext(COMMAND_QUEUE_TYPE_TRANSFER, "Transfer", Attribs.EngineCI.AdapterId);

    // On Metal and Vulkan mobile platforms we have only graphics queues.
    if (!AddContext(COMMAND_QUEUE_TYPE_COMPUTE, "Compute", Attribs.EngineCI.AdapterId))
        AddContext(COMMAND_QUEUE_TYPE_GRAPHICS, "Graphics 2", Attribs.EngineCI.AdapterId);

    Attribs.EngineCI.pImmediateContextInfo = m_ContextCI.data();
    Attribs.EngineCI.NumImmediateContexts  = static_cast<Uint32>(m_ContextCI.size());

    // Native fence may be a bit faster on Vulkan.
    Attribs.EngineCI.Features.NativeFence    = DEVICE_FEATURE_STATE_OPTIONAL;
    Attribs.EngineCI.Features.ComputeShaders = DEVICE_FEATURE_STATE_ENABLED;

    // Time queries for profiling.
    Attribs.EngineCI.Features.TimestampQueries              = DEVICE_FEATURE_STATE_OPTIONAL;
    Attribs.EngineCI.Features.TransferQueueTimestampQueries = DEVICE_FEATURE_STATE_OPTIONAL;

    if (Attribs.DeviceType == RENDER_DEVICE_TYPE_VULKAN)
    {
        auto& CreateInfoVk{static_cast<EngineVkCreateInfo&>(Attribs.EngineCI)};
        CreateInfoVk.UploadHeapPageSize = 32 << 20;
        // Increase reserve size to avoid pages being constantly destroyed and created.
        CreateInfoVk.HostVisibleMemoryReserveSize = 1536 << 20;
    }
    else if (Attribs.DeviceType == RENDER_DEVICE_TYPE_D3D12)
    {
        auto& CreateInfoD3D12{static_cast<EngineD3D12CreateInfo&>(Attribs.EngineCI)};
        CreateInfoD3D12.DynamicHeapPageSize = 32 << 20;
    }
}

void Tutorial23_CommandQueues::ComputePass()
{
    auto ComputeCtx = m_UseAsyncCompute ? m_ComputeCtx : m_pImmediateContext;

    const float DebugColor[] = {0.f, 1.f, 0.f, 1.f};
    ComputeCtx->BeginDebugGroup("Compute pass", DebugColor);

    m_Profiler.Begin(ComputeCtx, Profiler::COMPUTE);

    if (m_UseAsyncCompute)
    {
        // Wait until graphics pass finishes working with terrain height and normal maps
        ComputeCtx->DeviceWaitForFence(m_GraphicsCtxFence, m_GraphicsCtxFenceValue);
    }

    m_Terrain.Update(ComputeCtx);

    m_Profiler.End(ComputeCtx, Profiler::COMPUTE);

    ComputeCtx->EndDebugGroup(); // Compute pass

    if (m_UseAsyncCompute)
    {
        ComputeCtx->EnqueueSignal(m_ComputeCtxFence, ++m_ComputeCtxFenceValue);
        ComputeCtx->Flush();

        if (m_Terrain.DoubleBuffering)
        {
            // Wait for compute queue previous pass.
            m_pImmediateContext->DeviceWaitForFence(m_ComputeCtxFence, m_ComputeCtxFenceValue - 1);
        }
        else
        {
            // Wait for compute queue current pass.
            m_pImmediateContext->DeviceWaitForFence(m_ComputeCtxFence, m_ComputeCtxFenceValue);
        }
    }
}

void Tutorial23_CommandQueues::UploadPass()
{
    const Uint32 TransferRate = GetCpuToGpuTransferRateMb();

    if (m_TransferCtx == nullptr || TransferRate == 0)
        return;

    auto TransferCtx = m_UseAsyncTransfer ? m_TransferCtx : m_pImmediateContext;

    const float DebugColor[] = {0.f, 0.f, 1.f, 1.f};
    TransferCtx->BeginDebugGroup("Transfer pass", DebugColor);

    m_Profiler.Begin(TransferCtx, Profiler::TRANSFER);

    if (m_UseAsyncTransfer)
    {
        // Wait until graphics pass finishes with m_BuildingsTexAtlas.
        TransferCtx->DeviceWaitForFence(m_GraphicsCtxFence, m_GraphicsCtxFenceValue);
    }

    Uint32 CpuToGpuTransferRateMb = 0;
    m_Buildings.UpdateAtlas(TransferCtx, TransferRate, CpuToGpuTransferRateMb);
    m_Profiler.SetCpuToGpuTransferRate(CpuToGpuTransferRateMb);

    m_Profiler.End(TransferCtx, Profiler::TRANSFER);

    TransferCtx->EndDebugGroup(); // Transfer pass

    if (m_UseAsyncTransfer)
    {
        TransferCtx->EnqueueSignal(m_TransferCtxFence, ++m_TransferCtxFenceValue);
        TransferCtx->Flush();

        // Wait for transfer queue
        m_pImmediateContext->DeviceWaitForFence(m_TransferCtxFence, m_TransferCtxFenceValue);
    }
}

void Tutorial23_CommandQueues::GraphicsPass1()
{
    SceneDrawAttribs Attribs;
    Attribs.ViewProj     = m_Camera.GetViewMatrix() * m_Camera.GetProjMatrix();
    Attribs.LightDir     = -m_LightDir;
    Attribs.AmbientLight = m_AmbientLight;

    // Make all resource transitions before and after drawing.
    // Transitions and copy operations will break render pass which is slow in tile-based renderer.
    m_Terrain.BeforeDraw(m_pImmediateContext, Attribs);
    m_Buildings.BeforeDraw(m_pImmediateContext, Attribs);

    {
        const float DebugColor[] = {1.f, 0.f, 0.f, 1.f};
        m_pImmediateContext->BeginDebugGroup("Graphics pass 1", DebugColor);

        m_Profiler.Begin(m_pImmediateContext, Profiler::GRAPHICS_1);

        ITextureView* pRTV = m_GBuffer.Color->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
        ITextureView* pDSV = m_GBuffer.Depth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
        m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Clear the back buffer, transitions is not needed
        const float ClearColor[] = {m_SkyColor.x, m_SkyColor.y, m_SkyColor.z, 0.0f};
        m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
        m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_VERIFY);


        m_Terrain.Draw(m_pImmediateContext);
        m_Buildings.Draw(m_pImmediateContext);

        m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);

        m_Profiler.End(m_pImmediateContext, Profiler::GRAPHICS_1);

        m_pImmediateContext->EndDebugGroup(); // Graphics pass 1
    }

    m_Terrain.AfterDraw(m_pImmediateContext);
    m_Buildings.AfterDraw(m_pImmediateContext);

    if (m_UseAsyncCompute || m_UseAsyncTransfer)
    {
        // Notify compute context that graphics context finished working with terrain height and normal maps.
        // Notify transfer context that graphics context finished working with buildings texture atlas.
        m_pImmediateContext->EnqueueSignal(m_GraphicsCtxFence, ++m_GraphicsCtxFenceValue);

        // When used double buffering compute pass may overlap with whole frame.
        if (!m_Terrain.DoubleBuffering || m_UseAsyncTransfer)
            m_pImmediateContext->Flush();
    }
}

void Tutorial23_CommandQueues::GraphicsPass2()
{
    const float DebugColor[] = {1.f, 0.5f, 0.f, 1.f};
    m_pImmediateContext->BeginDebugGroup("Graphics pass 2", DebugColor);

    m_Profiler.Begin(m_pImmediateContext, Profiler::GRAPHICS_2);

    if (m_Glow)
        DownSample();

    // Final pass
    {
        ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        PostProcess();
    }

    m_Profiler.End(m_pImmediateContext, Profiler::GRAPHICS_2);

    m_pImmediateContext->EndDebugGroup(); // Graphics pass 2
}

void Tutorial23_CommandQueues::Render()
{
    m_Profiler.Begin(nullptr, Profiler::FRAME);

    ComputePass();
    UploadPass();

    GraphicsPass1();
    GraphicsPass2();

    if (m_ComputeCtx)
        m_ComputeCtx->FinishFrame();
    if (m_TransferCtx)
        m_TransferCtx->FinishFrame();

    m_Profiler.End(nullptr, Profiler::FRAME);
}

void Tutorial23_CommandQueues::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    m_Profiler.Update(ElapsedTime);
    UpdateUI();

    const float dt = static_cast<float>(ElapsedTime);
    m_Camera.Update(m_InputController, dt);

    m_Terrain.XOffset += dt * 0.5f;
    m_Terrain.Animation += dt * 0.2f;

    m_Buildings.CurrentTime = static_cast<Uint32>(CurrTime + 0.5);
}

void Tutorial23_CommandQueues::WindowResize(Uint32 Width, Uint32 Height)
{
    if (Width == 0 || Height == 0)
        return;

    // Scale surface
    Width  = ScaleSurface(Width);
    Height = ScaleSurface(Height);

    // Set minimal render target size
    Width  = std::max(Width, 1u << DownSampleFactor);
    Height = std::max(Height, 1u << DownSampleFactor);

    // Update projection matrix.
    float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
    m_Camera.SetProjAttribs(1.f, 1000.f, AspectRatio, PI_F / 4.f,
                            m_pSwapChain->GetDesc().PreTransform, m_pDevice->GetDeviceInfo().IsGLDevice());

    // Check if the image needs to be recreated.
    if (m_GBuffer.Color != nullptr &&
        m_GBuffer.Color->GetDesc().Width == Width &&
        m_GBuffer.Color->GetDesc().Height == Height)
        return;

    m_GBuffer = {};

    // Create window-size G-buffer textures.
    TextureDesc RTDesc;
    RTDesc.Name      = "GBuffer Color";
    RTDesc.Type      = RESOURCE_DIM_TEX_2D;
    RTDesc.Width     = Width;
    RTDesc.Height    = Height;
    RTDesc.MipLevels = DownSampleFactor;
    RTDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    RTDesc.Format    = m_ColorTargetFormat;
    m_pDevice->CreateTexture(RTDesc, nullptr, &m_GBuffer.Color);

    // Create texture view
    for (Uint32 Mip = 0; Mip < DownSampleFactor; ++Mip)
    {
        TextureViewDesc ViewDesc;
        ViewDesc.ViewType        = TEXTURE_VIEW_RENDER_TARGET;
        ViewDesc.TextureDim      = RESOURCE_DIM_TEX_2D;
        ViewDesc.MostDetailedMip = Mip;
        ViewDesc.NumMipLevels    = 1;

        m_GBuffer.Color->CreateView(ViewDesc, &m_GBuffer.ColorRTVs[Mip]);

        ViewDesc.ViewType = TEXTURE_VIEW_SHADER_RESOURCE;
        m_GBuffer.Color->CreateView(ViewDesc, &m_GBuffer.ColorSRBs[Mip]);
    }

    RTDesc.Name      = "GBuffer Depth";
    RTDesc.MipLevels = 1;
    RTDesc.BindFlags = BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE;
    RTDesc.Format    = m_DepthTargetFormat;
    m_pDevice->CreateTexture(RTDesc, nullptr, &m_GBuffer.Depth);

    // Create post-processing SRB
    {
        m_PostProcessSRB.Release();
        m_PostProcessPSO[0]->CreateShaderResourceBinding(&m_PostProcessSRB);
        m_PostProcessSRB->GetVariableByName(SHADER_TYPE_PIXEL, "PostProcessConstantsCB")->Set(m_PostProcessConstants);
        m_PostProcessSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_GBuffer_Color")->Set(m_GBuffer.Color->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        m_PostProcessSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_GBuffer_Depth")->Set(m_GBuffer.Depth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    }

    // Create down sample SRB
    for (Uint32 Mip = 0; Mip < DownSampleFactor; ++Mip)
    {
        auto& SRB = m_DownSampleSRB[Mip];
        SRB.Release();
        m_DownSamplePSO->CreateShaderResourceBinding(&SRB);
        SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_GBuffer_Color")->Set(m_GBuffer.ColorSRBs[Mip]);
    }
}

void Tutorial23_CommandQueues::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        // Transfer workload
        const auto PrevUseAsyncTransfer = m_UseAsyncTransfer;
        if (m_TransferCtx)
        {
            const auto TexSize    = m_Buildings.GetOpaqueTexAtlasDataSize();
            int        TexSizePOT = PlatformMisc::GetMSB(TexSize);

            if ((1u << TexSizePOT) < TexSize)
                ++TexSizePOT;

            TexSizePOT = std::max(TexSizePOT - 20, 0); // bytes to Mb

            const String TransferRateStr = std::to_string(GetCpuToGpuTransferRateMb());
            ImGui::TextDisabled("Transfer rate per frame (Mb)");
            ImGui::SliderInt("##TransferRate", &m_TransferRateMbExp2, 0, TexSizePOT, TransferRateStr.c_str());

            ImGui::Checkbox("Use async transfer", &m_UseAsyncTransfer);
            ImGui::Separator();
        }

        // Compute workload
        const auto PrevUseAsyncCompute = m_UseAsyncCompute;
        {
            const auto TerrainSizeStr = std::to_string(1u << m_Terrain.TerrainSize);
            const auto OldTerrainSize = m_Terrain.TerrainSize;
            ImGui::TextDisabled("Terrain dimension");
            ImGui::SliderInt("##TerrainSize", &m_Terrain.TerrainSize, 7, 13, TerrainSizeStr.c_str());
            if (OldTerrainSize != m_Terrain.TerrainSize)
                m_Terrain.Recreate(m_pImmediateContext);

            if (m_ComputeCtx)
                ImGui::Checkbox("Use async compute", &m_UseAsyncCompute);

            ImGui::Checkbox("Double buffering##TerrainDB", &m_Terrain.DoubleBuffering);
            ImGui::Separator();
        }

        // Graphics workload
        {
            const char* SurfaceScaleStr[] = {"1/4", "1/2", "1", "2", "4"};
            const int   OldSurfaceScale   = m_SurfaceScaleExp2;
            ImGui::TextDisabled("Surface scale");
            ImGui::SliderInt("##SurfaceScale", &m_SurfaceScaleExp2, -2, 2, SurfaceScaleStr[m_SurfaceScaleExp2 + 2]);

            // Recreate render targets
            if (OldSurfaceScale != m_SurfaceScaleExp2)
            {
                const auto& SCDesc = m_pSwapChain->GetDesc();
                WindowResize(SCDesc.Width, SCDesc.Height);
            }

            if (m_pDevice->GetDeviceInfo().Type != RENDER_DEVICE_TYPE_D3D11)
                ImGui::Checkbox("Glow", &m_Glow);
        }

        // Idle GPU to avoid validation errors.
        if (PrevUseAsyncCompute != m_UseAsyncCompute || PrevUseAsyncTransfer != m_UseAsyncTransfer)
            m_pDevice->IdleGPU();
    }
    ImGui::End();

    m_Profiler.UpdateUI();
}

} // namespace Diligent
