/*
 *  Copyright 2026 Diligent Graphics LLC
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

#include "Tutorial30_Scene.hpp"

#include "EngineFactory.h"
#include "MapHelper.hpp"
#include "GraphicsTypesX.hpp"
#include "GraphicsUtilities.h"

namespace Diligent
{

namespace HLSL
{

struct Constants
{
    float4x4 WorldViewProj;
    float4x4 NormalTransform;
    float4   Color;
};

} // namespace HLSL

Tutorial30_Scene::Tutorial30_Scene(IRenderDevice* pDevice) :
    m_pDevice{pDevice}
{
    CreateGeometry(pDevice);
    CreateConstantBuffer(pDevice);

    m_FrameTimer.Restart();
}

void Tutorial30_Scene::EnsurePipelineState(TEXTURE_FORMAT ColorFormat, TEXTURE_FORMAT DepthFormat)
{
    if (m_PSO)
    {
        const GraphicsPipelineDesc& Desc = m_PSO->GetGraphicsPipelineDesc();
        if (Desc.RTVFormats[0] == ColorFormat && Desc.DSVFormat == DepthFormat)
            return;
    }

    // Both the preview (orbit camera) and the immersive (CompositorServices)
    // renderers use right-handed view and projection matrices, which flip the
    // NDC winding of the cube geometry relative to the default D3D convention.
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PSOCreateInfo.PSODesc.Name                                          = "Cube PSO";
    PSOCreateInfo.PSODesc.PipelineType                                  = PIPELINE_TYPE_GRAPHICS;
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets                     = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                        = ColorFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat                            = DepthFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology                    = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_BACK;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = True;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable         = True;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthFunc           = COMPARISON_FUNC_GREATER_EQUAL; // Reverse-Z: Render() clears depth to 0 and we use GREATER_EQUAL.
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType            = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    ShaderCI.CompileFlags                    = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube VS";
        ShaderCI.FilePath        = "cube.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube PS";
        ShaderCI.FilePath        = "cube.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    InputLayoutDescX InputLayout;
    InputLayout.Add(0u, 0u, 3u, VT_FLOAT32, False); // Position
    InputLayout.Add(1u, 0u, 3u, VT_FLOAT32, False); // Normal

    PSOCreateInfo.GraphicsPipeline.InputLayout = InputLayout;
    PSOCreateInfo.pVS                          = pVS;
    PSOCreateInfo.pPS                          = pPS;

    m_PSO.Release();
    m_SRB.Release();
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_PSO);
    m_PSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_Constants);
    m_PSO->CreateShaderResourceBinding(&m_SRB, true);
}

void Tutorial30_Scene::CreateGeometry(IRenderDevice* pDevice)
{
    GeometryPrimitiveBuffersCreateInfo BufferCI;
    BufferCI.VertexBufferUsage = USAGE_IMMUTABLE;
    BufferCI.IndexBufferUsage  = USAGE_IMMUTABLE;

    GeometryPrimitiveInfo CubeInfo;
    CreateGeometryPrimitiveBuffers(
        pDevice,
        CubeGeometryPrimitiveAttributes{2.f, GEOMETRY_PRIMITIVE_VERTEX_FLAG_POS_NORM},
        &BufferCI,
        &m_CubeVertexBuffer,
        &m_CubeIndexBuffer,
        &CubeInfo);
    m_CubeIndexCount = CubeInfo.NumIndices;
}

void Tutorial30_Scene::CreateConstantBuffer(IRenderDevice* pDevice)
{
    BufferDesc CBDesc;
    CBDesc.Name           = "VS Constants CB";
    CBDesc.Size           = sizeof(HLSL::Constants);
    CBDesc.Usage          = USAGE_DYNAMIC;
    CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
    CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    pDevice->CreateBuffer(CBDesc, nullptr, &m_Constants);
}

void Tutorial30_Scene::Update()
{
    const double Now     = m_FrameTimer.GetElapsedTime();
    const float  Elapsed = static_cast<float>(Now - m_LastFrameTime);
    m_LastFrameTime      = Now;
    m_RotationAngle += Elapsed * 0.5f;
}

void Tutorial30_Scene::Render(IDeviceContext* pContext,
                              ITextureView*   pRTV,
                              ITextureView*   pDSV,
                              const float4x4& ViewProj)
{
    EnsurePipelineState(pRTV->GetTexture()->GetDesc().Format,
                        pDSV->GetTexture()->GetDesc().Format);

    pContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    constexpr float ClearColor[] = {0.05f, 0.05f, 0.08f, 1.0f};
    pContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    pContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 0.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    IBuffer* pBuffs[] = {m_CubeVertexBuffer};
    pContext->SetVertexBuffers(0, 1, pBuffs, nullptr,
                               RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                               SET_VERTEX_BUFFERS_FLAG_RESET);
    pContext->SetIndexBuffer(m_CubeIndexBuffer, 0,
                             RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    pContext->SetPipelineState(m_PSO);
    pContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Ground plane
    RenderCuboid(pContext, {0, 0, 0, 1}, {0, -1.5f, -2.0f}, {2.0f, 0.1f, 2.0f}, {0.4f, 0.5f, 0.5f}, ViewProj);
    // Table-like surface
    RenderCuboid(pContext, {0, 0, 0, 1}, {0, -0.6f, -2.0f}, {1.0f, 0.05f, 1.0f}, {0.6f, 0.6f, 0.4f}, ViewProj);
    // Spinning cube
    RenderCuboid(pContext,
                 QuaternionF::RotationFromAxisAngle(float3{0, 1, 0}, m_RotationAngle),
                 {0, -0.1f, -2.0f}, {0.3f, 0.3f, 0.3f}, {0.8f, 0.2f, 0.2f}, ViewProj);
}

void Tutorial30_Scene::RenderCuboid(IDeviceContext*    pContext,
                                    const QuaternionF& Rotation,
                                    const float3&      Position,
                                    const float3&      Scale,
                                    const float3&      Color,
                                    const float4x4&    ViewProj)
{
    const float4x4 ModelTransform = float4x4::Scale(Scale * 0.5f) * Rotation.ToMatrix() * float4x4::Translation(Position);
    {
        MapHelper<HLSL::Constants> CBConstants{pContext, m_Constants, MAP_WRITE, MAP_FLAG_DISCARD};
        CBConstants->WorldViewProj   = ModelTransform * ViewProj;
        CBConstants->NormalTransform = Rotation.ToMatrix();
        CBConstants->Color           = float4{Color.x, Color.y, Color.z, 1.0f};
    }

    DrawIndexedAttribs DrawAttrs;
    DrawAttrs.IndexType  = VT_UINT32;
    DrawAttrs.NumIndices = m_CubeIndexCount;
    DrawAttrs.Flags      = DRAW_FLAG_VERIFY_ALL;
    pContext->DrawIndexed(DrawAttrs);
}

} // namespace Diligent
