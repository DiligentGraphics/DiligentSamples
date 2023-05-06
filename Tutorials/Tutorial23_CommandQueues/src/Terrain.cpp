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

#include "Terrain.hpp"
#include "TextureUtilities.h"
#include "ShaderMacroHelper.hpp"
#include "MapHelper.hpp"
#include "PlatformMisc.hpp"

namespace Diligent
{
namespace HLSL
{
#include "../assets/Structures.fxh"
}

using IndexType = Uint32;

void Terrain::Initialize(IRenderDevice* pDevice, IBuffer* pDrawConstants, Uint64 ImmediateContextMask)
{
    m_FrameId              = 0;
    m_Device               = pDevice;
    m_DrawConstants        = pDrawConstants;
    m_ImmediateContextMask = ImmediateContextMask;
}

void Terrain::CreateResources(IDeviceContext* pContext)
{
    if (TerrainSize > 10)
        m_NoiseScale = 20.0f;
    else if (TerrainSize > 8)
        m_NoiseScale = 10.0f;
    else
        m_NoiseScale = 4.0f;

    std::vector<float2>    Vertices;
    std::vector<IndexType> Indices;

    const auto  GridSize  = std::max(1u, (1u << TerrainSize) / m_ComputeGroupSize) * m_ComputeGroupSize;
    const float GridScale = 1.0f / static_cast<float>(GridSize - 1);

    Vertices.resize(size_t{GridSize} * size_t{GridSize});
    Indices.resize(size_t{GridSize - 1} * size_t{GridSize - 1} * 6u);

    {
        auto*  pVertices = Vertices.data();
        Uint32 v         = 0;
        for (Uint32 y = 0; y < GridSize; ++y)
        {
            for (Uint32 x = 0; x < GridSize; ++x)
            {
                pVertices[v++] = float2{x * GridScale, y * GridScale};
            }
        }
        VERIFY_EXPR(v == Vertices.size());

        auto*  pIndices = Indices.data();
        Uint32 i        = 0;
        for (Uint32 y = 1; y < GridSize; ++y)
        {
            for (Uint32 x = 1; x < GridSize; ++x)
            {
                pIndices[i++] = (x - 1) + y * GridSize;
                pIndices[i++] = x + (y - 1) * GridSize;
                pIndices[i++] = (x - 1) + (y - 1) * GridSize;

                pIndices[i++] = (x - 1) + y * GridSize;
                pIndices[i++] = x + y * GridSize;
                pIndices[i++] = x + (y - 1) * GridSize;
            }
        }
        VERIFY_EXPR(i == Indices.size());
    }

    // Create vertex & index buffers
    {
        BufferDesc BuffDesc;
        BuffDesc.Name      = "Terrain VB";
        BuffDesc.Size      = static_cast<Uint64>(Vertices.size() * sizeof(Vertices[0]));
        BuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        BuffDesc.Usage     = USAGE_IMMUTABLE;
        BufferData BuffData{Vertices.data(), BuffDesc.Size, pContext};
        m_Device->CreateBuffer(BuffDesc, &BuffData, &m_VB);

        BuffDesc.Name      = "Terrain IB";
        BuffDesc.Size      = static_cast<Uint64>(Indices.size() * sizeof(Indices[0]));
        BuffDesc.BindFlags = BIND_INDEX_BUFFER;
        BuffData           = BufferData{Indices.data(), BuffDesc.Size, pContext};
        m_Device->CreateBuffer(BuffDesc, &BuffData, &m_IB);

        // Buffers are used in multiple contexts, but after this transition resources state will never changes.
        const StateTransitionDesc Barriers[] = {
            {m_VB, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_VERTEX_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
            {m_IB, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_INDEX_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE} //
        };
        pContext->TransitionResourceStates(_countof(Barriers), Barriers);
    }

    // Create height & normal maps
    {
        TextureDesc TexDesc;
        TexDesc.Name                 = "Terrain height map";
        TexDesc.Type                 = RESOURCE_DIM_TEX_2D;
        TexDesc.Format               = TEX_FORMAT_R16_FLOAT;
        TexDesc.Width                = GridSize;
        TexDesc.Height               = GridSize;
        TexDesc.BindFlags            = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
        TexDesc.ImmediateContextMask = m_ImmediateContextMask;
        m_Device->CreateTexture(TexDesc, nullptr, &m_HeightMap[0]);
        m_Device->CreateTexture(TexDesc, nullptr, &m_HeightMap[1]);

        TexDesc.Name   = "Terrain normal map";
        TexDesc.Format = TEX_FORMAT_RGBA16_FLOAT; //TEX_FORMAT_RGBA8_UNORM;
        m_Device->CreateTexture(TexDesc, nullptr, &m_NormalMap[0]);
        m_Device->CreateTexture(TexDesc, nullptr, &m_NormalMap[1]);

        const StateTransitionDesc Barriers[] = {
            {m_HeightMap[0], RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_UNORDERED_ACCESS},
            {m_HeightMap[1], RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_UNORDERED_ACCESS},
            {m_NormalMap[0], RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_UNORDERED_ACCESS},
            {m_NormalMap[1], RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_UNORDERED_ACCESS} //
        };
        pContext->TransitionResourceStates(_countof(Barriers), Barriers);

        // Resources are used in multiple contexts, so disable automatic resource transitions.
        m_HeightMap[0]->SetState(RESOURCE_STATE_UNKNOWN);
        m_HeightMap[1]->SetState(RESOURCE_STATE_UNKNOWN);
        m_NormalMap[0]->SetState(RESOURCE_STATE_UNKNOWN);
        m_NormalMap[1]->SetState(RESOURCE_STATE_UNKNOWN);
    }

    if (m_DiffuseMap == nullptr)
    {
        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB       = true;
        loadInfo.GenerateMips = true;
        RefCntAutoPtr<ITexture> Tex;
        CreateTextureFromFile("Sand.jpg", loadInfo, m_Device, &m_DiffuseMap);

        const StateTransitionDesc Barrier = {m_DiffuseMap, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE};
        pContext->TransitionResourceStates(1, &Barrier);
    }

    if (m_TerrainConstants[0] == nullptr || m_TerrainConstants[1] == nullptr)
    {
        BufferDesc BuffDesc;
        BuffDesc.Name      = "Terrain constants";
        BuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
        BuffDesc.Usage     = USAGE_DEFAULT;
        BuffDesc.Size      = sizeof(HLSL::TerrainConstants);

        BuffDesc.ImmediateContextMask = m_ImmediateContextMask; // compute context
        m_Device->CreateBuffer(BuffDesc, nullptr, &m_TerrainConstants[0]);

        BuffDesc.ImmediateContextMask = m_ImmediateContextMask; // graphics context
        m_Device->CreateBuffer(BuffDesc, nullptr, &m_TerrainConstants[1]);
    }

    // Set terrain generator shader resources
    for (Uint32 i = 0; i < _countof(m_GenSRB); ++i)
    {
        auto& SRB = m_GenSRB[i];
        m_GenPSO->CreateShaderResourceBinding(&SRB);
        SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "TerrainConstantsCB")->Set(m_TerrainConstants[0]);
        SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_HeightMapUAV")->Set(m_HeightMap[i]->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
        SRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_NormalMapUAV")->Set(m_NormalMap[i]->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
    }

    // Set draw terrain shader resources
    for (Uint32 i = 0; i < _countof(m_DrawSRB); ++i)
    {
        auto& SRB = m_DrawSRB[i];
        m_DrawPSO->CreateShaderResourceBinding(&SRB);
        SRB->GetVariableByName(SHADER_TYPE_VERTEX, "DrawConstantsCB")->Set(m_DrawConstants);
        SRB->GetVariableByName(SHADER_TYPE_VERTEX, "TerrainConstantsCB")->Set(m_TerrainConstants[1]);
        SRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_TerrainHeightMap")->Set(m_HeightMap[1 - i]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        SRB->GetVariableByName(SHADER_TYPE_PIXEL, "DrawConstantsCB")->Set(m_DrawConstants);
        SRB->GetVariableByName(SHADER_TYPE_PIXEL, "TerrainConstantsCB")->Set(m_TerrainConstants[1]);
        SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_TerrainNormalMap")->Set(m_NormalMap[1 - i]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_TerrainDiffuseMap")->Set(m_DiffuseMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    }
}

void Terrain::CreatePSO(const ScenePSOCreateAttribs& Attr)
{
    // Terrain generation PSO
    {
        const auto& CSInfo    = m_Device->GetAdapterInfo().ComputeShader;
        Uint32      GroupSize = static_cast<Uint32>(sqrt(static_cast<float>(CSInfo.MaxThreadGroupInvocations)));
        GroupSize             = 2u << PlatformMisc::GetMSB(GroupSize);
        GroupSize             = (GroupSize * GroupSize <= CSInfo.MaxThreadGroupInvocations) ? GroupSize : (GroupSize >> 1);

#if PLATFORM_ANDROID
        // 64 threads per threadgroup is much faster
        GroupSize = std::min(GroupSize, 8u);
#else
        GroupSize = std::min(GroupSize, 16u);
#endif

        m_ComputeGroupSize = GroupSize - m_GroupBorderSize;

        VERIFY_EXPR(GroupSize > 0);
        VERIFY_EXPR(GroupSize * GroupSize <= CSInfo.MaxThreadGroupInvocations);

        ShaderMacroHelper Macros;
        Macros.AddShaderMacro("GROUP_SIZE_WITH_BORDER", GroupSize);
        Macros.AddShaderMacro("GROUP_SIZE", m_ComputeGroupSize);
        Macros.AddShaderMacro("TERRAIN_OCTAVES", Attr.TurbulenceOctaves);
        Macros.AddShaderMacro("NOISE_OCTAVES", Attr.NoiseOctaves);

        ShaderCreateInfo ShaderCI;
        ShaderCI.Desc                       = {"Generate terrain height and normal map CS", SHADER_TYPE_COMPUTE, true};
        ShaderCI.pShaderSourceStreamFactory = Attr.pShaderSourceFactory;
        ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.Macros                     = Macros;
        ShaderCI.FilePath                   = "GenerateTerrain.csh";
        ShaderCI.EntryPoint                 = "CSMain";

        RefCntAutoPtr<IShader> pCS;
        m_Device->CreateShader(ShaderCI, &pCS);

        ComputePipelineStateCreateInfo PSOCreateInfo;

        PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;
        PSOCreateInfo.PSODesc.Name         = "Generate terrain height and normal map PSO";

        PSOCreateInfo.PSODesc.ImmediateContextMask               = m_ImmediateContextMask;
        PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

        PSOCreateInfo.pCS = pCS;
        m_Device->CreateComputePipelineState(PSOCreateInfo, &m_GenPSO);
    }

    // Draw terrain PSO
    {
        GraphicsPipelineStateCreateInfo PSOCreateInfo;

        PSOCreateInfo.PSODesc.Name         = "Draw terrain PSO";
        PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

        PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
        PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = Attr.ColorTargetFormat;
        PSOCreateInfo.GraphicsPipeline.DSVFormat                    = Attr.DepthTargetFormat;
        PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
        PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.pShaderSourceStreamFactory = Attr.pShaderSourceFactory;

        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc       = {"Draw terrain VS", SHADER_TYPE_VERTEX, true};
            ShaderCI.EntryPoint = "main";
            ShaderCI.FilePath   = "DrawTerrain.vsh";
            m_Device->CreateShader(ShaderCI, &pVS);
        }

        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc       = {"Draw terrain PS", SHADER_TYPE_PIXEL, true};
            ShaderCI.EntryPoint = "main";
            ShaderCI.FilePath   = "DrawTerrain.psh";
            m_Device->CreateShader(ShaderCI, &pPS);
        }

        PSOCreateInfo.pVS = pVS;
        PSOCreateInfo.pPS = pPS;

        const LayoutElement LayoutElems[] = {LayoutElement{0, 0, 2, VT_FLOAT32, False}};

        PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
        PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

        const SamplerDesc SamLinearClampDesc //
            {
                FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
                TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP //
            };
        const SamplerDesc SamLinearWrapDesc //
            {
                FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
                TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP //
            };
        const ImmutableSamplerDesc ImtblSamplers[] = //
            {
                {SHADER_TYPE_PIXEL, "g_TerrainNormalMap", SamLinearClampDesc},
                {SHADER_TYPE_PIXEL, "g_TerrainDiffuseMap", SamLinearWrapDesc} //
            };
        PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
        PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);
        PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType  = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

        m_Device->CreateGraphicsPipelineState(PSOCreateInfo, &m_DrawPSO);
    }
}

void Terrain::Update(IDeviceContext* pContext)
{
    pContext->BeginDebugGroup("Update terrain");

    const auto& TexDesc = m_HeightMap[0]->GetDesc();

    // Update constants
    {
        HLSL::TerrainConstants ConstData;
        ConstData.Scale      = float3(m_XZScale, m_TerrainHeightScale, m_XZScale);
        ConstData.UVScale    = m_UVScale;
        ConstData.Animation  = Animation;
        ConstData.XOffset    = XOffset;
        ConstData.NoiseScale = m_NoiseScale;

        pContext->UpdateBuffer(m_TerrainConstants[0], 0, sizeof(ConstData), &ConstData, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    pContext->SetPipelineState(m_GenPSO);

    // Terrain height and normal maps can not be transitioned here because has UNKNOWN state.
    const auto Id = DoubleBuffering ? m_FrameId : 0;
    pContext->CommitShaderResources(m_GenSRB[Id], RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DispatchComputeAttribs dispatchAttrs;
    dispatchAttrs.ThreadGroupCountX = TexDesc.Width / m_ComputeGroupSize;
    dispatchAttrs.ThreadGroupCountY = TexDesc.Height / m_ComputeGroupSize;

    VERIFY_EXPR(dispatchAttrs.ThreadGroupCountX * m_ComputeGroupSize == TexDesc.Width);
    VERIFY_EXPR(dispatchAttrs.ThreadGroupCountY * m_ComputeGroupSize == TexDesc.Height);

    pContext->DispatchCompute(dispatchAttrs);

    pContext->EndDebugGroup(); // Update terrain
}

void Terrain::Draw(IDeviceContext* pContext)
{
    pContext->BeginDebugGroup("Draw terrain");

    pContext->SetPipelineState(m_DrawPSO);

    // Terrain height and normal maps can not be transitioned here because has UNKNOWN state.
    // Other resources has constant state and does not require transitions.
    const auto Id = DoubleBuffering ? m_FrameId : 1;
    pContext->CommitShaderResources(m_DrawSRB[Id], RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    // Vertex and index buffers are immutable and does not require transitions.
    IBuffer* VBs[] = {m_VB};
    pContext->SetVertexBuffers(0, _countof(VBs), VBs, nullptr, RESOURCE_STATE_TRANSITION_MODE_VERIFY, SET_VERTEX_BUFFERS_FLAG_RESET);
    pContext->SetIndexBuffer(m_IB, 0, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    DrawIndexedAttribs drawAttribs;
    drawAttribs.NumIndices = static_cast<Uint32>(m_IB->GetDesc().Size / sizeof(IndexType));
    drawAttribs.IndexType  = VT_UINT32;
    drawAttribs.Flags      = DRAW_FLAG_VERIFY_ALL;
    pContext->DrawIndexed(drawAttribs);

    pContext->EndDebugGroup(); // Draw terrain
}

void Terrain::BeforeDraw(IDeviceContext* pContext, const SceneDrawAttribs& Attr)
{
    // Update constants
    {
        HLSL::TerrainConstants ConstData;
        ConstData.Scale      = float3(m_XZScale, m_TerrainHeightScale, m_XZScale);
        ConstData.UVScale    = m_UVScale;
        ConstData.Animation  = Animation;
        ConstData.XOffset    = XOffset;
        ConstData.NoiseScale = m_NoiseScale;

        pContext->UpdateBuffer(m_TerrainConstants[1], 0, sizeof(ConstData), &ConstData, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    {
        const float Center = -m_XZScale * 0.5f;

        MapHelper<HLSL::DrawConstants> ConstData(pContext, m_DrawConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        ConstData->ModelViewProj = (float4x4::Translation(Center, 0.f, Center) * Attr.ViewProj).Transpose();
        ConstData->NormalMat     = float4x4::Identity();
        ConstData->LightDir      = Attr.LightDir;
        ConstData->AmbientLight  = Attr.AmbientLight;
    }

    // Resources must be manually transitioned to required state.
    // Vulkan:     the correct pipeline barrier must contains vertex and pixel shader stages which is not supported in compute context.
    // DirectX 12: height map used as non-pixel shader resource and can be transitioned in compute context,
    //             but normal map used as pixel shader resource and must be transitioned in graphics context.
    const Uint32              Id         = DoubleBuffering ? 1 - m_FrameId : 0;
    const StateTransitionDesc Barriers[] = {
        {m_HeightMap[Id], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE},
        {m_NormalMap[Id], RESOURCE_STATE_UNORDERED_ACCESS, RESOURCE_STATE_SHADER_RESOURCE},
        {m_TerrainConstants[1], RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
        {m_DrawConstants, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE} //
    };
    pContext->TransitionResourceStates(_countof(Barriers), Barriers);
}

void Terrain::AfterDraw(IDeviceContext* pContext)
{
    // Resources must be manually transitioned to required state.
    const Uint32              Id         = DoubleBuffering ? 1 - m_FrameId : 0;
    const StateTransitionDesc Barriers[] = {
        {m_HeightMap[Id], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS},
        {m_NormalMap[Id], RESOURCE_STATE_SHADER_RESOURCE, RESOURCE_STATE_UNORDERED_ACCESS} //
    };
    pContext->TransitionResourceStates(_countof(Barriers), Barriers);

    ++m_FrameId;
}

void Terrain::Recreate(IDeviceContext* pContext)
{
    // Recreate terrain buffers
    m_VB           = nullptr;
    m_IB           = nullptr;
    m_HeightMap[0] = nullptr;
    m_HeightMap[1] = nullptr;
    m_NormalMap[0] = nullptr;
    m_NormalMap[1] = nullptr;
    m_GenSRB[0]    = nullptr;
    m_GenSRB[1]    = nullptr;
    m_DrawSRB[0]   = nullptr;
    m_DrawSRB[1]   = nullptr;

    m_Device->IdleGPU();

    CreateResources(pContext);

    pContext->Flush();
    m_Device->IdleGPU();
}

} // namespace Diligent
