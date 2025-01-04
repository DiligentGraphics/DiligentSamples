/*
 *  Copyright 2024-2025 Diligent Graphics LLC
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

#include <random>

#include "Tutorial29_OIT.hpp"

#include "MapHelper.hpp"
#include "ShaderMacroHelper.hpp"
#include "ColorConversion.h"
#include "GraphicsUtilities.h"
#include "imgui.h"
#include "GraphicsTypesX.hpp"
#include "CommonlyUsedStates.h"

namespace Diligent
{

namespace HLSL
{
#include "../assets/common.fxh"

struct InstanceData
{
    float4 TranslationAndScale;
    float4 Color;
};

} // namespace HLSL

SampleBase* CreateSample()
{
    return new Tutorial29_OIT();
}

static constexpr BlendStateDesc BS_UpdateOITTail{
    False,                // AlphaToCoverageEnable
    False,                // IndependentBlendEnable
    RenderTargetBlendDesc // Render Target 0
    {
        True,                   // BlendEnable
        False,                  // LogicOperationEnable
        BLEND_FACTOR_ONE,       // SrcBlend
        BLEND_FACTOR_ONE,       // DestBlend
        BLEND_OPERATION_ADD,    // BlendOp
        BLEND_FACTOR_ZERO,      // SrcBlendAlpha
        BLEND_FACTOR_SRC_ALPHA, // DestBlendAlpha
        BLEND_OPERATION_ADD,    // BlendOpAlpha
    },
};

static constexpr BlendStateDesc BS_AttenuateBackground{
    False,                // AlphaToCoverageEnable
    False,                // IndependentBlendEnable
    RenderTargetBlendDesc // Render Target 0
    {
        True,                   // BlendEnable
        False,                  // LogicOperationEnable
        BLEND_FACTOR_ZERO,      // SrcBlend
        BLEND_FACTOR_SRC_ALPHA, // DestBlend
        BLEND_OPERATION_ADD,    // BlendOp
        BLEND_FACTOR_ZERO,      // SrcBlendAlpha
        BLEND_FACTOR_ONE,       // DestBlendAlpha
        BLEND_OPERATION_ADD,    // BlendOpAlpha
    },
};

void Tutorial29_OIT::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    Attribs.EngineCI.Features.ComputeShaders           = DEVICE_FEATURE_STATE_ENABLED;
    Attribs.EngineCI.Features.PixelUAVWritesAndAtomics = DEVICE_FEATURE_STATE_ENABLED;
    // We will create our own depth buffer
    Attribs.SCDesc.DepthBufferFormat = TEX_FORMAT_UNKNOWN;
}

void Tutorial29_OIT::CreatePipelineStates()
{
    RenderDeviceX_N Device{m_pDevice};
    // WebGPU does not support earlydepthstencil attribute
    m_EarlyDepthStencilSupported = !Device.GetDeviceInfo().IsWebGPUDevice();
    ShaderCreateInfo ShaderCI;

    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    ShaderMacroHelper Macros;
    Macros.Add("CONVERT_PS_OUTPUT_TO_GAMMA", m_ConvertPSOutputToGamma);
    Macros.Add("THREAD_GROUP_SIZE", static_cast<int>(m_ThreadGroupSizeXY));
    Macros.Add("NUM_OIT_LAYERS", static_cast<int>(m_NumOITLayers));
    // Use manual depth testing on WebGPU as it does not support the earlydepthstencil attribute
    Macros.Add("USE_MANUAL_DEPTH_TEST", !m_EarlyDepthStencilSupported);
    ShaderCI.Macros = Macros;

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.CompileFlags               = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    RefCntAutoPtr<IShader> pGeometryVS;
    {
        ShaderCI.Desc       = {"Geometry VS", SHADER_TYPE_VERTEX, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "geometry.vsh";

        pGeometryVS = Device.CreateShader(ShaderCI);
    }

    RefCntAutoPtr<IShader> pScreenTriangleVS;
    {
        ShaderCI.Desc       = {"Screen Triangle VS", SHADER_TYPE_VERTEX, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "screen_triangle.vsh";

        pScreenTriangleVS = Device.CreateShader(ShaderCI);
    }

    RefCntAutoPtr<IShader> pAlphaBlendPS;
    {
        ShaderCI.Desc       = {"Alpha-blend PS", SHADER_TYPE_PIXEL, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "alpha_blend.psh";

        pAlphaBlendPS = Device.CreateShader(ShaderCI);
    }

    RefCntAutoPtr<IShader> pOITBlendPS;
    {
        ShaderCI.Desc       = {"OIT blend PS", SHADER_TYPE_PIXEL, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "oit_blend.psh";

        pOITBlendPS = Device.CreateShader(ShaderCI);
    }

    RefCntAutoPtr<IShader> pUpdateOITLayersPS;
    {
        ShaderCI.Desc       = {"Update OIT Layers PS", SHADER_TYPE_PIXEL, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "update_oit_layers.psh";

        pUpdateOITLayersPS = Device.CreateShader(ShaderCI);
    }

    RefCntAutoPtr<IShader> pClearOITLayersCS;
    {
        ShaderCI.Desc       = {"Clear OIT Layers", SHADER_TYPE_COMPUTE, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "clear_oit_layers.csh";

        pClearOITLayersCS = Device.CreateShader(ShaderCI);
    }

    RefCntAutoPtr<IShader> pAttenuateBackgroundPS;
    {
        ShaderCI.Desc       = {"Attenuate Background PS", SHADER_TYPE_PIXEL, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "attenuate_background.psh";

        pAttenuateBackgroundPS = Device.CreateShader(ShaderCI);
    }

    {
        ComputePipelineStateCreateInfoX PsoCI{"Clear OIT Layers"};

        PipelineResourceLayoutDescX ResourceLayout;
        ResourceLayout.AddVariable(SHADER_TYPE_COMPUTE, "g_rwOITLayers", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE);
        PsoCI
            .AddShader(pClearOITLayersCS)
            .SetResourceLayout(ResourceLayout);

        m_ClearOITLayersPSO = Device.CreateComputePipelineState(PsoCI);
        m_ClearOITLayersPSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "cbConstants")->Set(m_Constants);
    }

    const SwapChainDesc& SCDesc = m_pSwapChain->GetDesc();

    {
        GraphicsPipelineStateCreateInfoX PsoCI;

        // clang-format off
        // Define vertex shader input layout
        // This tutorial uses two types of input: per-vertex data and per-instance data.
        InputLayoutDescX InputLayout
        {
            // Per-vertex data - first buffer slot
            // Attribute 0 - vertex position
            LayoutElement{0, 0, 3, VT_FLOAT32},
            // Attribute 1 - normal
            LayoutElement{1, 0, 3, VT_FLOAT32},
            
            // Per-instance data - second buffer slot
            // Attribute 2 - translation and scale
            LayoutElement{2, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
            // Attribute 3 - color
            LayoutElement{3, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE}
        };
        // clang-format on

        PipelineResourceLayoutDescX ResourceLayout;
        ResourceLayout
            .SetDefaultVariableType(SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
            .AddVariable(SHADER_TYPE_VS_PS, "cbConstants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

        PsoCI
            .SetName("Alpha-blend PSO")
            .SetInputLayout(InputLayout)
            .AddShader(pGeometryVS)
            .AddShader(pAlphaBlendPS)
            .SetResourceLayout(ResourceLayout)
            .SetBlendDesc(BS_PremultipliedAlphaBlend)
            .AddRenderTarget(SCDesc.ColorBufferFormat)
            .SetDepthFormat(m_DepthFormat)
            .SetDepthStencilDesc(DSS_EnableDepthNoWrites);

        m_AlphaBlendPSO = Device.CreateGraphicsPipelineState(PsoCI);
        m_AlphaBlendPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbConstants")->Set(m_Constants);
        m_AlphaBlendSRB.Release();
        m_AlphaBlendPSO->CreateShaderResourceBinding(&m_AlphaBlendSRB, true);

        PsoCI
            .SetName("Opaque PSO")
            .SetBlendDesc(BS_Default)
            .SetDepthStencilDesc(DSS_Default);
        m_OpaquePSO = Device.CreateGraphicsPipelineState(PsoCI);

        PsoCI
            .SetName("OIT blend PSO")
            .SetBlendDesc(BS_AdditiveBlend)
            .SetDepthStencilDesc(DSS_EnableDepthNoWrites)
            .AddShader(pOITBlendPS);
        m_OITBlendPSO = Device.CreateGraphicsPipelineState(PsoCI);
        m_OITBlendPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbConstants")->Set(m_Constants);


        ResourceLayout.AddVariable(SHADER_TYPE_PIXEL, "g_DepthBuffer", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE,
                                   SHADER_VARIABLE_FLAG_UNFILTERABLE_FLOAT_TEXTURE_WEBGPU);
        PsoCI
            .SetName("Update OIT Layers PSO")
            .SetResourceLayout(ResourceLayout)
            .SetBlendDesc(BS_UpdateOITTail)
            .AddShader(pUpdateOITLayersPS)
            .ClearRenderTargets()
            .AddRenderTarget(TailTransmittanceFormat)
            .SetDepthFormat(m_EarlyDepthStencilSupported ? m_DepthFormat : TEX_FORMAT_UNKNOWN)
            .SetDepthStencilDesc(m_EarlyDepthStencilSupported ? DSS_EnableDepthNoWrites : DSS_DisableDepth);
        m_UpdateOITLayersPSO = Device.CreateGraphicsPipelineState(PsoCI);
        m_UpdateOITLayersPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbConstants")->Set(m_Constants);
    }

    {
        GraphicsPipelineStateCreateInfoX PsoCI{"Attenuate background"};

        PipelineResourceLayoutDescX ResourceLayout;
        ResourceLayout
            .SetDefaultVariableType(SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE)
            .AddVariable(SHADER_TYPE_VS_PS, "cbConstants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

        PsoCI
            .AddRenderTarget(SCDesc.ColorBufferFormat)
            .SetDepthFormat(m_DepthFormat)
            .SetBlendDesc(BS_AttenuateBackground)
            .SetDepthStencilDesc(DSS_DisableDepth)
            .AddShader(pScreenTriangleVS)
            .AddShader(pAttenuateBackgroundPS)
            .SetResourceLayout(ResourceLayout);
        m_AttenuateBackgroundPSO = Device.CreateGraphicsPipelineState(PsoCI);
        m_AttenuateBackgroundPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbConstants")->Set(m_Constants);
    }
}

void Tutorial29_OIT::WindowResize(Uint32 Width, Uint32 Height)
{
    TextureDesc TexDesc;
    TexDesc.Name      = "Depth buffer";
    TexDesc.Type      = RESOURCE_DIM_TEX_2D;
    TexDesc.Width     = Width;
    TexDesc.Height    = Height;
    TexDesc.MipLevels = 1;
    TexDesc.Format    = m_DepthFormat;
    TexDesc.BindFlags = BIND_DEPTH_STENCIL;
    if (!m_EarlyDepthStencilSupported)
        TexDesc.BindFlags |= BIND_SHADER_RESOURCE;
    TexDesc.Usage = USAGE_DEFAULT;
    m_DepthBuffer.Release();
    m_pDevice->CreateTexture(TexDesc, nullptr, &m_DepthBuffer);
}

void Tutorial29_OIT::PrepareOITResources()
{
    const SwapChainDesc& SCDesc = m_pSwapChain->GetDesc();
    if (m_OITLayers && m_OITLayers->GetDesc().Size != SCDesc.Width * SCDesc.Height * m_NumOITLayers * sizeof(Uint32))
        m_OITLayers.Release();

    if (m_OITLayers)
        return;

    BufferDesc BuffDesc;
    BuffDesc.Name              = "OIT Layers";
    BuffDesc.Size              = SCDesc.Width * SCDesc.Height * m_NumOITLayers * sizeof(Uint32);
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    BuffDesc.ElementByteStride = sizeof(Uint32);
    BuffDesc.BindFlags         = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
    BuffDesc.Usage             = USAGE_DEFAULT;
    m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_OITLayers);

    TextureDesc TexDesc;
    TexDesc.Name      = "OIT Tail";
    TexDesc.Type      = RESOURCE_DIM_TEX_2D;
    TexDesc.Width     = SCDesc.Width;
    TexDesc.Height    = SCDesc.Height;
    TexDesc.MipLevels = 1;
    TexDesc.Format    = TailTransmittanceFormat;
    TexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
    TexDesc.Usage     = USAGE_DEFAULT;
    m_OITTail.Release();
    m_pDevice->CreateTexture(TexDesc, nullptr, &m_OITTail);

    m_ClearOITLayersSRB.Release();
    m_ClearOITLayersPSO->CreateShaderResourceBinding(&m_ClearOITLayersSRB, true);
    m_ClearOITLayersSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_rwOITLayers")->Set(m_OITLayers->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));

    m_UpdateOITLayersSRB.Release();
    m_UpdateOITLayersPSO->CreateShaderResourceBinding(&m_UpdateOITLayersSRB, true);
    m_UpdateOITLayersSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_rwOITLayers")->Set(m_OITLayers->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
    if (!m_EarlyDepthStencilSupported)
    {
        m_UpdateOITLayersSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_DepthBuffer")->Set(m_DepthBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    }

    m_OITBlendSRB.Release();
    m_OITBlendPSO->CreateShaderResourceBinding(&m_OITBlendSRB, true);
    m_OITBlendSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_OITLayers")->Set(m_OITLayers->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_OITBlendSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_OITTail")->Set(m_OITTail->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

    m_AttenuateBackgroundSRB.Release();
    m_AttenuateBackgroundPSO->CreateShaderResourceBinding(&m_AttenuateBackgroundSRB, true);
    m_AttenuateBackgroundSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_OITLayers")->Set(m_OITLayers->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_AttenuateBackgroundSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_OITTail")->Set(m_OITTail->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
}

void Tutorial29_OIT::CreateGeometryBuffers()
{
    Uint32 NumSubdivision = std::min(4 * 32 / m_GridSize, 8);

    m_VertexBuffer.Release();
    m_IndexBuffer.Release();

    GeometryPrimitiveInfo PrimInfo;
    CreateGeometryPrimitiveBuffers(m_pDevice, SphereGeometryPrimitiveAttributes{1.f, GEOMETRY_PRIMITIVE_VERTEX_FLAG_POS_NORM, NumSubdivision},
                                   nullptr, &m_VertexBuffer, &m_IndexBuffer, &PrimInfo);
    m_NumIndices = PrimInfo.NumIndices;
}

void Tutorial29_OIT::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::SliderInt("Grid Size", &m_GridSize, 1, 32))
        {
            CreateGeometryBuffers();
            CreateInstanceBuffers();
        }

        if (ImGui::SliderFloat("Percent Opaque", &m_PercentOpaque, 0.f, 100.f))
        {
            CreateInstanceBuffers();
        }

        ImGui::Combo("Render Mode", reinterpret_cast<int*>(&m_RenderMode), "Unsorted Alpha Blend\0Layered\0\0");
        if (m_RenderMode == RenderMode::Layered)
        {
            if (ImGui::SliderInt("Num OIT Layers", &m_NumOITLayers, 1, 16))
            {
                CreatePipelineStates();
                PrepareOITResources();
            }
        }

        if (ImGui::SliderFloat("Min Opacity", &m_MinOpacity, 0.f, 1.f))
        {
            m_MaxOpacity = std::max(m_MaxOpacity, m_MinOpacity);
        }
        if (ImGui::SliderFloat("Max Opacity", &m_MaxOpacity, 0.f, 1.f))
        {
            m_MinOpacity = std::min(m_MaxOpacity, m_MinOpacity);
        }
        ImGui::Checkbox("Animate", &m_Animate);
    }
    ImGui::End();
}

void Tutorial29_OIT::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    CreateUniformBuffer(m_pDevice, sizeof(HLSL::Constants), "Constants", &m_Constants);
    CreateInstanceBuffers();
    CreateGeometryBuffers();
    CreatePipelineStates();
}

void Tutorial29_OIT::CreateInstanceBuffers()
{
    m_InstanceBuffer = {};

    std::mt19937 gen; // Standard mersenne_twister_engine. Use default seed
                      // to generate consistent distribution.

    std::uniform_real_distribution<float> scale_distr{0.3f, 1.0f};
    std::uniform_real_distribution<float> offset_distr{-1.f, +1.f};
    std::uniform_real_distribution<float> color_distr{0.3f, 1.0f};
    std::uniform_real_distribution<float> alpha_distr{0.f, 1.f};


    const Uint32 TotalInstances = m_GridSize * m_GridSize * m_GridSize;
    m_NumInstances[0]           = static_cast<Uint32>(static_cast<float>(TotalInstances) * m_PercentOpaque / 100.f);
    m_NumInstances[1]           = TotalInstances - m_NumInstances[0];
    for (Uint32 IsTransparent = 0; IsTransparent < 2; ++IsTransparent)
    {
        std::vector<HLSL::InstanceData> Instances(m_NumInstances[IsTransparent]);
        if (Instances.empty())
            continue;

        float BaseScale = 1.f / static_cast<float>(m_GridSize);
        for (HLSL::InstanceData& Instance : Instances)
        {
            Instance.TranslationAndScale = float4{offset_distr(gen), offset_distr(gen), offset_distr(gen), BaseScale * scale_distr(gen)};
            float4 TransparentColor{color_distr(gen), color_distr(gen), color_distr(gen), alpha_distr(gen)};
            float4 OpaqueColor{0.5, 0.5, 0.5, -1.0};
            Instance.Color = IsTransparent ? TransparentColor : OpaqueColor;
        }
        // Update instance data buffer
        Uint32 DataSize = static_cast<Uint32>(sizeof(Instances[0]) * Instances.size());

        // Create instance data buffer
        BufferDesc InstBuffDesc;
        InstBuffDesc.Name = "Instance data buffer";
        // Use default usage as this buffer will only be updated when grid size changes
        InstBuffDesc.Usage     = USAGE_DEFAULT;
        InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        InstBuffDesc.Size      = DataSize;

        BufferData Data{Instances.data(), DataSize};
        m_pDevice->CreateBuffer(InstBuffDesc, &Data, &m_InstanceBuffer[IsTransparent]);
    }
}


void Tutorial29_OIT::RenderGrid(bool IsTransparent, IPipelineState* pPSO, IShaderResourceBinding* pSRB)
{
    const Uint32 NumInstances = m_NumInstances[IsTransparent ? 1 : 0];
    if (NumInstances == 0)
        return;

    m_pImmediateContext->SetPipelineState(pPSO);
    m_pImmediateContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    IBuffer* pBuffs[] = {m_VertexBuffer, m_InstanceBuffer[IsTransparent ? 1 : 0]};
    m_pImmediateContext->SetVertexBuffers(0, _countof(pBuffs), pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_IndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs DrawAttrs;
    DrawAttrs.IndexType    = VT_UINT32;
    DrawAttrs.NumIndices   = m_NumIndices;
    DrawAttrs.NumInstances = NumInstances;
    DrawAttrs.Flags        = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);
}

// Render a frame
void Tutorial29_OIT::Render()
{
    const SwapChainDesc& SCDesc = m_pSwapChain->GetDesc();
    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<HLSL::Constants> CBConstants{m_pImmediateContext, m_Constants, MAP_WRITE, MAP_FLAG_DISCARD};
        CBConstants->ViewProj   = m_ViewProjMatrix;
        CBConstants->Proj       = m_ProjMatrix;
        CBConstants->LightDir   = normalize(float3{0.57735f, -0.57735f, 0.157735f});
        CBConstants->MinOpacity = m_MinOpacity;
        CBConstants->MaxOpacity = m_MaxOpacity;
        CBConstants->ScreenSize = {SCDesc.Width, SCDesc.Height};
    }

    ITextureView* pDSV          = m_DepthBuffer->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
    ITextureView* pSwapChainRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    {
        m_pImmediateContext->SetRenderTargets(1, &pSwapChainRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Clear the back buffer
        float4 ClearColor = {0.35f, 0.35f, 0.35f, 1.0f};
        if (m_ConvertPSOutputToGamma)
        {
            // If manual gamma correction is required, we need to clear the render target with sRGB color
            ClearColor = LinearToSRGB(ClearColor);
        }
        m_pImmediateContext->ClearRenderTarget(pSwapChainRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    if (m_NumInstances[0] > 0)
    {
        RenderGrid(/*IsTransparent = */ false, m_OpaquePSO, m_AlphaBlendSRB);
    }

    if (m_NumInstances[1] > 0)
    {
        if (m_RenderMode == RenderMode::Layered)
        {
            PrepareOITResources();

            m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // Clear OIT layers
            m_pImmediateContext->SetPipelineState(m_ClearOITLayersPSO);
            m_pImmediateContext->CommitShaderResources(m_ClearOITLayersSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            DispatchComputeAttribs DispatchAttrs{
                (SCDesc.Width + m_ThreadGroupSizeXY - 1) / m_ThreadGroupSizeXY,
                (SCDesc.Height + m_ThreadGroupSizeXY - 1) / m_ThreadGroupSizeXY,
                1,
            };
            m_pImmediateContext->DispatchCompute(DispatchAttrs);

            ITextureView* pTailRTV = m_OITTail->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
            m_pImmediateContext->SetRenderTargets(1, &pTailRTV, m_EarlyDepthStencilSupported ? pDSV : nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            float Tail[4] = {0, 0, 0, 1};
            m_pImmediateContext->ClearRenderTarget(pTailRTV, Tail, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            RenderGrid(/*IsTransparent = */ true, m_UpdateOITLayersPSO, m_UpdateOITLayersSRB);
        }

        m_pImmediateContext->SetRenderTargets(1, &pSwapChainRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        if (m_RenderMode == RenderMode::Layered)
        {
            m_pImmediateContext->SetPipelineState(m_AttenuateBackgroundPSO);
            m_pImmediateContext->CommitShaderResources(m_AttenuateBackgroundSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
        }

        IPipelineState*         pPSO = nullptr;
        IShaderResourceBinding* pSRB = nullptr;
        switch (m_RenderMode)
        {
            case RenderMode::UnsortedAlphaBlend:
                pPSO = m_AlphaBlendPSO;
                pSRB = m_AlphaBlendSRB;
                break;

            case RenderMode::Layered:
                pPSO = m_OITBlendPSO;
                pSRB = m_OITBlendSRB;
                break;

            default:
                UNEXPECTED("Unexpected render mode");
        }
        RenderGrid(/*IsTransparent = */ true, pPSO, pSRB);
    }
}

void Tutorial29_OIT::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    if (m_Animate)
    {
        m_AnimationTime += ElapsedTime;
    }

    float4x4 View =
        float4x4::RotationY(static_cast<float>(m_AnimationTime * 0.25)) *
        float4x4::RotationX(-0.6f) *
        float4x4::Translation(0.f, 0.f, 4.0f);

    // Get pretransform matrix that rotates the scene according the surface orientation
    float4x4 SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    // Get projection matrix adjusted to the current screen orientation
    m_ProjMatrix = GetAdjustedProjectionMatrix(PI_F / 4.0f, 1.f, 5.f);

    // Compute view-projection matrix
    m_ViewProjMatrix = View * SrfPreTransform * m_ProjMatrix;
}

} // namespace Diligent
