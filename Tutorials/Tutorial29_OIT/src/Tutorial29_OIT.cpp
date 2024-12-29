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

void Tutorial29_OIT::CreatePipelineStates()
{
    ShaderCreateInfo ShaderCI;

    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    ShaderMacroHelper Macros;
    Macros.Add("CONVERT_PS_OUTPUT_TO_GAMMA", m_ConvertPSOutputToGamma);
    ShaderCI.Macros = Macros;

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.CompileFlags               = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    RenderDeviceX_N Device{m_pDevice};

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc       = {"Geometry VS", SHADER_TYPE_VERTEX, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "geometry.vsh";

        pVS = Device.CreateShader(ShaderCI);
    }

    RefCntAutoPtr<IShader> pGeometryPS;
    {
        ShaderCI.Desc       = {"Geometry PS", SHADER_TYPE_PIXEL, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "alpha_blend.psh";

        pGeometryPS = Device.CreateShader(ShaderCI);
    }

    GraphicsPipelineStateCreateInfoX PsoCI{"Geometry PSO"};

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

    // Disable depth testing
    PsoCI.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    PipelineResourceLayoutDescX ResourceLayout;
    ResourceLayout.AddVariable(SHADER_TYPE_VS_PS, "cbConstants", SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

    const SwapChainDesc& SCDesc = m_pSwapChain->GetDesc();

    PsoCI
        .SetInputLayout(InputLayout)
        .AddShader(pVS)
        .AddShader(pGeometryPS)
        .SetResourceLayout(ResourceLayout)
        .SetBlendDesc(BS_PremultipliedAlphaBlend)
        .AddRenderTarget(SCDesc.ColorBufferFormat)
        .SetDepthFormat(SCDesc.DepthBufferFormat);

    m_AlphaBlendPSO = Device.CreateGraphicsPipelineState(PsoCI);
    m_AlphaBlendPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbConstants")->Set(m_Constants);
    m_AlphaBlendPSO->CreateShaderResourceBinding(&m_AlphaBlendSRB, true);
}

void Tutorial29_OIT::CreateInstanceBuffer()
{
    // Create instance data buffer that will store transformation matrices
    BufferDesc InstBuffDesc;
    InstBuffDesc.Name = "Instance data buffer";
    // Use default usage as this buffer will only be updated when grid size changes
    InstBuffDesc.Usage     = USAGE_DEFAULT;
    InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    InstBuffDesc.Size      = sizeof(HLSL::InstanceData) * MaxInstances;
    m_pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_InstanceBuffer);

    PopulateInstanceBuffer();
}

void Tutorial29_OIT::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::SliderInt("Grid Size", &m_GridSize, 1, 32))
        {
            PopulateInstanceBuffer();
        }
        if (ImGui::SliderFloat("Min Opacity", &m_MinOpacity, 0.f, 1.f))
        {
            m_MaxOpacity = std::max(m_MaxOpacity, m_MinOpacity);
        }
        if (ImGui::SliderFloat("Max Opacity", &m_MaxOpacity, 0.f, 1.f))
        {
            m_MinOpacity = std::min(m_MaxOpacity, m_MinOpacity);
        }
    }
    ImGui::End();
}

void Tutorial29_OIT::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    // Create geometry buffers
    {
        GeometryPrimitiveInfo PrimInfo;
        CreateGeometryPrimitiveBuffers(m_pDevice, SphereGeometryPrimitiveAttributes{1.f, GEOMETRY_PRIMITIVE_VERTEX_FLAG_POS_NORM, 8},
                                       nullptr, &m_VertexBuffer, &m_IndexBuffer, &PrimInfo);
        m_NumIndices = PrimInfo.NumIndices;
    }

    CreateUniformBuffer(m_pDevice, sizeof(HLSL::Constants), "Constants", &m_Constants);
    CreateInstanceBuffer();

    CreatePipelineStates();
}

void Tutorial29_OIT::PopulateInstanceBuffer()
{
    // Populate instance data buffer
    const size_t                    zGridSize = static_cast<size_t>(m_GridSize);
    std::vector<HLSL::InstanceData> Instances(zGridSize * zGridSize * zGridSize);

    float fGridSize = static_cast<float>(m_GridSize);

    std::mt19937 gen; // Standard mersenne_twister_engine. Use default seed
                      // to generate consistent distribution.

    std::uniform_real_distribution<float> scale_distr{0.3f, 1.0f};
    std::uniform_real_distribution<float> offset_distr{-1.f, +1.f};
    std::uniform_real_distribution<float> color_distr{0.3f, 1.0f};
    std::uniform_real_distribution<float> alpha_distr{0.f, 1.f};

    float BaseScale = 1.f / fGridSize;
    int   instId    = 0;
    for (int i = 0; i < m_GridSize * m_GridSize * m_GridSize; ++i)
    {
        float4 TranslationAndScale = float4{offset_distr(gen), offset_distr(gen), offset_distr(gen), BaseScale * scale_distr(gen)};
        float4 Color               = float4{color_distr(gen), color_distr(gen), color_distr(gen), alpha_distr(gen)};
        Instances[instId++]        = {TranslationAndScale, Color};
    }
    // Update instance data buffer
    Uint32 DataSize = static_cast<Uint32>(sizeof(Instances[0]) * Instances.size());
    m_pImmediateContext->UpdateBuffer(m_InstanceBuffer, 0, DataSize, Instances.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}


// Render a frame
void Tutorial29_OIT::Render()
{
    ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    ITextureView* pDSV = m_pSwapChain->GetDepthBufferDSV();
    // Clear the back buffer
    float4 ClearColor = {0.350f, 0.350f, 0.350f, 1.0f};
    if (m_ConvertPSOutputToGamma)
    {
        // If manual gamma correction is required, we need to clear the render target with sRGB color
        ClearColor = LinearToSRGB(ClearColor);
    }
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<HLSL::Constants> CBConstants{m_pImmediateContext, m_Constants, MAP_WRITE, MAP_FLAG_DISCARD};
        CBConstants->ViewProj   = m_ViewProjMatrix;
        CBConstants->LightDir   = normalize(float3{0.57735f, -0.57735f, 0.157735f});
        CBConstants->MinOpacity = m_MinOpacity;
        CBConstants->MaxOpacity = m_MaxOpacity;
    }

    // Bind vertex, instance and index buffers
    const Uint64 offsets[] = {0, 0};
    IBuffer*     pBuffs[]  = {m_VertexBuffer, m_InstanceBuffer};
    m_pImmediateContext->SetVertexBuffers(0, _countof(pBuffs), pBuffs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_IndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set the pipeline state
    m_pImmediateContext->SetPipelineState(m_AlphaBlendPSO);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
    // makes sure that resources are transitioned to required states.
    m_pImmediateContext->CommitShaderResources(m_AlphaBlendSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs DrawAttrs;       // This is an indexed draw call
    DrawAttrs.IndexType    = VT_UINT32; // Index type
    DrawAttrs.NumIndices   = m_NumIndices;
    DrawAttrs.NumInstances = m_GridSize * m_GridSize * m_GridSize; // The number of instances
    // Verify the state of vertex and index buffers
    DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);
}

void Tutorial29_OIT::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    // Set cube view matrix
    float4x4 View = float4x4::RotationX(-0.6f) * float4x4::Translation(0.f, 0.f, 4.0f);

    // Get pretransform matrix that rotates the scene according the surface orientation
    float4x4 SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    // Get projection matrix adjusted to the current screen orientation
    float4x4 Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

    // Compute view-projection matrix
    m_ViewProjMatrix = View * SrfPreTransform * Proj;
}

} // namespace Diligent
