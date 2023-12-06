/*
 *  Copyright 2019-2023 Diligent Graphics LLC
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

#include <cmath>
#include <array>

#include "PostProcessDemo.hpp"
#include "MapHelper.hpp"
#include "BasicMath.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "imgui.h"
#include "imGuIZMO.h"
#include "ImGuiUtils.hpp"
#include "CallbackWrapper.hpp"
#include "CommandLineParser.hpp"
#include "CommonlyUsedStates.h"
#include "EnvMapRenderer.hpp"
#include "ImGuiImplDiligent.hpp"
#include "ScopedDebugGroup.hpp"

namespace Diligent
{

namespace HLSL
{

#include "Shaders/Common/public/BasicStructures.fxh"
#include "Shaders/PBR/public/PBR_Structures.fxh"
#include "Shaders/PBR/private/RenderPBR_Structures.fxh"

} // namespace HLSL

static SamplerDesc DefaultSampler = Sam_Aniso16xWrap;

static RefCntAutoPtr<IShader> CreateShader(IRenderDevice*          pDevice,
                                           IRenderStateCache*      pStateCache,
                                           const Char*             FileName,
                                           const Char*             EntryPoint,
                                           SHADER_TYPE             Type,
                                           const ShaderMacroArray& Macros = {})
{

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders", &pShaderSourceFactory);


    ShaderCreateInfo ShaderCI;
    ShaderCI.EntryPoint                      = EntryPoint;
    ShaderCI.FilePath                        = FileName;
    ShaderCI.Macros                          = Macros;
    ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.ShaderType                 = Type;
    ShaderCI.Desc.Name                       = EntryPoint;
    ShaderCI.pShaderSourceStreamFactory      = pShaderSourceFactory;
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    return RenderDeviceWithCache<false>{pDevice, pStateCache}.CreateShader(ShaderCI);
}

SampleBase* CreateSample()
{
    return new PostProcessDemo();
}

PostProcessDemo::PostProcessDemo()
{
    m_Camera.SetPos(float3(0.0f, 0.0f, 0.0f));
    m_Camera.SetLookAt(float3(1.0f, 0.0f, 0.0f));
}

void PostProcessDemo::LoadModel(const char* Path)
{
    GLTF::ModelCreateInfo ModelCI{};
    ModelCI.FileName             = Path;
    ModelCI.ComputeBoundingBoxes = true;

    m_Model.reset(new GLTF::Model{m_pDevice, m_pImmediateContext, ModelCI});

    m_ModelResourceBindings = m_GLTFRenderer->CreateResourceBindings(*m_Model, m_pFrameAttribsCB);

    m_RenderParams.SceneIndex = m_Model->DefaultSceneId;
    m_RenderParams.Flags      = m_RenderParams.Flags & ~PBR_Renderer::PSO_FLAG_ENABLE_TONE_MAPPING;

    UpdateScene();
}

void PostProcessDemo::UpdateScene()
{
    m_Model->ComputeTransforms(m_RenderParams.SceneIndex, m_Transforms);
    const auto ModelAABB = m_Model->ComputeBoundingBox(m_RenderParams.SceneIndex, m_Transforms);

    float  MaxDim = 0;
    float3 ModelDim{ModelAABB.Max - ModelAABB.Min};
    MaxDim = std::max(MaxDim, ModelDim.x);
    MaxDim = std::max(MaxDim, ModelDim.y);
    MaxDim = std::max(MaxDim, ModelDim.z);

    const auto ModelTransform = float4x4::Translation(-ModelAABB.Min - 0.5f * ModelDim + float3(0.0f, 0.375f * ModelDim.y, 0.0f));
    m_Model->ComputeTransforms(m_RenderParams.SceneIndex, m_Transforms, ModelTransform);
}

PostProcessDemo::CommandLineStatus PostProcessDemo::ProcessCommandLine(int argc, const char* const* argv)
{
    CommandLineParser ArgsParser{argc, argv};
    ArgsParser.Parse("model", m_InitialModelPath);
    return CommandLineStatus::OK;
}

void PostProcessDemo::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);
    m_pScreenSpaceReflection.reset(new ScreenSpaceReflection{m_pDevice});
    WindowResize(512u, 512u);

    RefCntAutoPtr<ITexture> EnvironmentMap;
    CreateTextureFromFile("textures/papermill.ktx", TextureLoadInfo{"Environment map"}, m_pDevice, &EnvironmentMap);
    m_pEnvironmentMapSRV = EnvironmentMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    GLTF_PBR_Renderer::CreateInfo RendererCI;
    RendererCI.ColorMapImmutableSampler    = DefaultSampler;
    RendererCI.PhysDescMapImmutableSampler = DefaultSampler;
    RendererCI.NormalMapImmutableSampler   = DefaultSampler;
    RendererCI.AOMapImmutableSampler       = DefaultSampler;
    RendererCI.EmissiveMapImmutableSampler = DefaultSampler;

    RendererCI.RTVFmt                = m_pColorBuffer->GetDesc().Format;
    RendererCI.DSVFmt                = m_pDepthBuffer->GetDesc().Format;
    RendererCI.FrontCounterClockwise = false;

    m_GLTFRenderer.reset(new GLTF_PBR_Renderer{m_pDevice, nullptr, m_pImmediateContext, RendererCI});

    CreateUniformBuffer(m_pDevice, sizeof(HLSL::PBRFrameAttribs), "PBRFrameAttribs", &m_pFrameAttribsCB);
    CreateUniformBuffer(m_pDevice, sizeof(HLSL::ToneMappingAttribs), "ToneMappingAttribs", &m_pToneMapingAttribsCB);

    // clang-format off
    StateTransitionDesc Barriers [] =
    {
        {m_pFrameAttribsCB, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
        {EnvironmentMap,   RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE}
    };
    // clang-format on
    m_pImmediateContext->TransitionResourceStates(_countof(Barriers), Barriers);

    m_GLTFRenderer->PrecomputeCubemaps(m_pImmediateContext, m_pEnvironmentMapSRV);

    LoadModel(m_InitialModelPath.c_str());

    {
        ShaderResourceVariableDesc VariableDescs[] = {
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "cbCameraAttribs", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "g_InputDepth", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };

        const auto VS = CreateShader(m_pDevice, nullptr, "FullScreenTriangleVS.fx", "FullScreenTriangleVS", SHADER_TYPE_VERTEX);
        const auto PS = CreateShader(m_pDevice, nullptr, "GenerateRoughnessNormal.fx", "GenerateRoughnessNormalPS", SHADER_TYPE_PIXEL);

        PipelineResourceLayoutDesc ResourceLayout;
        ResourceLayout.Variables    = VariableDescs;
        ResourceLayout.NumVariables = _countof(VariableDescs);

        GraphicsPipelineStateCreateInfo PSOCreateInfo;
        PipelineStateDesc&              PSODesc = PSOCreateInfo.PSODesc;

        PSODesc.Name           = "PostProcessDemo::GenerateRoughnessNormal";
        PSODesc.ResourceLayout = ResourceLayout;

        auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

        GraphicsPipeline.RasterizerDesc.FillMode              = FILL_MODE_SOLID;
        GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_NONE;
        GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = true;
        GraphicsPipeline.DepthStencilDesc                     = DSS_DisableDepth;
        GraphicsPipeline.BlendDesc                            = BS_Default;
        PSOCreateInfo.pVS                                     = VS;
        PSOCreateInfo.pPS                                     = PS;
        GraphicsPipeline.PrimitiveTopology                    = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        GraphicsPipeline.NumRenderTargets                     = 2;
        GraphicsPipeline.DSVFormat                            = TEX_FORMAT_UNKNOWN;
        GraphicsPipeline.RTVFormats[0]                        = m_pMaterialBuffer->GetDesc().Format;
        GraphicsPipeline.RTVFormats[1]                        = m_pNormalBuffer->GetDesc().Format;

        m_pGenerateRoughnessNormalPSO = RenderDeviceWithCache<false>{m_pDevice, nullptr}.CreateGraphicsPipelineState(PSOCreateInfo);
        m_pGenerateRoughnessNormalPSO->CreateShaderResourceBinding(&m_pGenerateRoughnessNormalSRB, true);
    }

    {
        ShaderResourceVariableDesc VariableDescs[] = {
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "cbCameraAttribs", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "g_InputDepth", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };

        const auto VS = CreateShader(m_pDevice, nullptr, "FullScreenTriangleVS.fx", "FullScreenTriangleVS", SHADER_TYPE_VERTEX);
        const auto PS = CreateShader(m_pDevice, nullptr, "ComputeMotionVectors.fx", "ComputeMotionVectorsPS", SHADER_TYPE_PIXEL);

        PipelineResourceLayoutDesc ResourceLayout;
        ResourceLayout.Variables    = VariableDescs;
        ResourceLayout.NumVariables = _countof(VariableDescs);

        GraphicsPipelineStateCreateInfo PSOCreateInfo;
        PipelineStateDesc&              PSODesc = PSOCreateInfo.PSODesc;

        PSODesc.Name           = "PostProcessDemo::ComputeMotionVectors";
        PSODesc.ResourceLayout = ResourceLayout;
        PSOCreateInfo.pVS      = VS;
        PSOCreateInfo.pPS      = PS;

        auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

        GraphicsPipeline.RasterizerDesc.FillMode              = FILL_MODE_SOLID;
        GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_NONE;
        GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = true;
        GraphicsPipeline.DepthStencilDesc                     = DSS_DisableDepth;
        GraphicsPipeline.BlendDesc                            = BS_Default;
        GraphicsPipeline.PrimitiveTopology                    = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        GraphicsPipeline.NumRenderTargets                     = 1;
        GraphicsPipeline.DSVFormat                            = TEX_FORMAT_UNKNOWN;
        GraphicsPipeline.RTVFormats[0]                        = m_pMotionVectors->GetDesc().Format;

        m_pComputeMotionVectorsPSO = RenderDeviceWithCache<false>{m_pDevice, nullptr}.CreateGraphicsPipelineState(PSOCreateInfo);
        m_pComputeMotionVectorsPSO->CreateShaderResourceBinding(&m_pComputeMotionVectorsSRB, true);
    }

    {
        ShaderResourceVariableDesc VariableDescs[] = {
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "cbToneMappingAttribs", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "cbCameraAttribs", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "g_TextureRadiance", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "g_TextureRadianceSSR", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "g_TextureNormalBuffer", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "g_TextureMaterialBuffer", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
            ShaderResourceVariableDesc{SHADER_TYPE_PIXEL, "g_BRDF_LUT", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        };

        ShaderMacroHelper PSMacros;
        PSMacros.Add("TONE_MAPPING_MODE", TONE_MAPPING_MODE_UNCHARTED2);

        const auto VS = CreateShader(m_pDevice, nullptr, "FullScreenTriangleVS.fx", "FullScreenTriangleVS", SHADER_TYPE_VERTEX);
        const auto PS = CreateShader(m_pDevice, nullptr, "ApplyPostEffects.fx", "ApplyPostEffectsPS", SHADER_TYPE_PIXEL, PSMacros);

        ImmutableSamplerDesc ImtblSamplers[] = {
            {SHADER_TYPE_PIXEL, "g_BRDF_LUT", Sam_Aniso16xWrap},
        };

        PipelineResourceLayoutDesc ResourceLayout;
        ResourceLayout.Variables            = VariableDescs;
        ResourceLayout.NumVariables         = _countof(VariableDescs);
        ResourceLayout.ImmutableSamplers    = ImtblSamplers;
        ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

        GraphicsPipelineStateCreateInfo PSOCreateInfo;
        PipelineStateDesc&              PSODesc = PSOCreateInfo.PSODesc;

        PSODesc.Name           = "PostProcessDemo::ApplyPostEffects";
        PSODesc.ResourceLayout = ResourceLayout;
        PSOCreateInfo.pVS      = VS;
        PSOCreateInfo.pPS      = PS;

        auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

        GraphicsPipeline.RasterizerDesc.FillMode              = FILL_MODE_SOLID;
        GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_NONE;
        GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = true;
        GraphicsPipeline.DepthStencilDesc                     = DSS_DisableDepth;
        GraphicsPipeline.BlendDesc                            = BS_Default;
        GraphicsPipeline.PrimitiveTopology                    = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        GraphicsPipeline.NumRenderTargets                     = 1;
        GraphicsPipeline.DSVFormat                            = TEX_FORMAT_UNKNOWN;
        GraphicsPipeline.RTVFormats[0]                        = m_pSwapChain->GetDesc().ColorBufferFormat;

        m_pApplyPostEffectsPSO = RenderDeviceWithCache<false>{m_pDevice, nullptr}.CreateGraphicsPipelineState(PSOCreateInfo);
        m_pApplyPostEffectsPSO->CreateShaderResourceBinding(&m_pApplyPostEffectsSRB, true);
    }

    const auto& ImGuiIO = ImGui::GetIO();
    m_pImGuiFont        = ImGuiIO.Fonts->AddFontFromFileTTF("fonts/Roboto-Medium.ttf", 14.0f);
    ImGui::StyleColorsDark();
    m_pImGui->UpdateFontsTexture();
}

void PostProcessDemo::WindowResize(Uint32 Width, Uint32 Height)
{
    SampleBase::WindowResize(Width, Height);
    m_pScreenSpaceReflection->OnWindowResize(m_pDevice, m_pImmediateContext, Width, Height);

    {
        if (m_pColorBuffer)
            m_pColorBuffer.Release();

        TextureDesc Desc;
        Desc.Name      = "PostProcessDemo::ColorBuffer";
        Desc.Type      = RESOURCE_DIM_TEX_2D;
        Desc.Format    = TEX_FORMAT_R11G11B10_FLOAT;
        Desc.Width     = Width;
        Desc.Height    = Height;
        Desc.MipLevels = 1;
        Desc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
        m_pDevice->CreateTexture(Desc, nullptr, &m_pColorBuffer);
    }

    {
        if (m_pDepthBuffer)
            m_pDepthBuffer.Release();

        TextureDesc Desc;
        Desc.Name      = "PostProcessDemo::DepthBuffer";
        Desc.Type      = RESOURCE_DIM_TEX_2D;
        Desc.Format    = TEX_FORMAT_D32_FLOAT;
        Desc.Width     = Width;
        Desc.Height    = Height;
        Desc.MipLevels = 1;
        Desc.BindFlags = BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE;
        m_pDevice->CreateTexture(Desc, nullptr, &m_pDepthBuffer);
    }

    {
        if (m_pNormalBuffer)
            m_pNormalBuffer.Release();

        TextureDesc Desc;
        Desc.Name      = "PostProcessDemo::NormalBuffer";
        Desc.Type      = RESOURCE_DIM_TEX_2D;
        Desc.Format    = TEX_FORMAT_RGBA16_FLOAT;
        Desc.Width     = Width;
        Desc.Height    = Height;
        Desc.MipLevels = 1;
        Desc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
        m_pDevice->CreateTexture(Desc, nullptr, &m_pNormalBuffer);
    }

    {
        if (m_pMaterialBuffer)
            m_pMaterialBuffer.Release();

        TextureDesc Desc;
        Desc.Name      = "SSRDemo::MaterialBuffer";
        Desc.Type      = RESOURCE_DIM_TEX_2D;
        Desc.Format    = TEX_FORMAT_RGBA8_UNORM;
        Desc.Width     = Width;
        Desc.Height    = Height;
        Desc.MipLevels = 1;
        Desc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
        m_pDevice->CreateTexture(Desc, nullptr, &m_pMaterialBuffer);
    }

    {
        if (m_pMotionVectors)
            m_pMotionVectors.Release();

        TextureDesc Desc;
        Desc.Name      = "SSRDemo::MotionVectors";
        Desc.Type      = RESOURCE_DIM_TEX_2D;
        Desc.Format    = TEX_FORMAT_RG16_FLOAT;
        Desc.Width     = Width;
        Desc.Height    = Height;
        Desc.MipLevels = 1;
        Desc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
        m_pDevice->CreateTexture(Desc, nullptr, &m_pMotionVectors);
    }
}

void PostProcessDemo::UpdateUI()
{
    ImGui::PushFont(static_cast<ImFont*>(m_pImGuiFont));
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (m_Model->Scenes.size() > 1)
        {
            std::vector<std::pair<Uint32, std::string>> SceneList;
            SceneList.reserve(m_Model->Scenes.size());
            for (Uint32 i = 0; i < m_Model->Scenes.size(); ++i)
                SceneList.emplace_back(i, !m_Model->Scenes[i].Name.empty() ? m_Model->Scenes[i].Name : std::to_string(i));
            if (ImGui::Combo("Scene", &m_RenderParams.SceneIndex, SceneList.data(), static_cast<int>(SceneList.size())))
                UpdateScene();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::TreeNode("Lighting"))
        {
            ImGui::SliderFloat("Occlusion strength", &m_ShaderAttribs.OcclusionStrength, 0.f, 1.f);
            ImGui::SliderFloat("Emission scale", &m_ShaderAttribs.EmissionScale, 0.f, 1.f);
            ImGui::SliderFloat("IBL scale", &m_ShaderAttribs.IBLScale, 0.f, 1.f);
            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::TreeNode("Tone mapping"))
        {
            // clang-format off
            ImGui::SliderFloat("Average log lum", &m_ShaderAttribs.AverageLogLum, 0.01f, 10.0f);
            ImGui::SliderFloat("Middle gray",     &m_ShaderAttribs.MiddleGray,    0.01f,  1.0f);
            ImGui::SliderFloat("White point",     &m_ShaderAttribs.WhitePoint,    0.1f,  20.0f);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Screen Space Reflection"))
        {
            // clang-format off
            ImGui::SliderFloat("Scene Roughness", &m_ShaderAttribs.SSRParams.SceneRoughness, 0.0f, 1.0f);

            if (ImGui::TreeNode("Intersection"))
            {
                 ImGui::SliderFloat("Depth Buffer Thickness", &m_ShaderAttribs.SSRParams.DepthBufferThickness, 0.0f, 10.0f);
                 ImGui::SliderFloat("Roughness Threshold", &m_ShaderAttribs.SSRParams.RoughnessThreshold, 0.0f, 1.0f);
                 ImGui::SliderInt("Max Traversal Iterations",  &m_ShaderAttribs.SSRParams.MaxTraversalIntersections, 0, 256);
                 ImGui::SliderInt("Most Detailed Mip", &m_ShaderAttribs.SSRParams.MostDetailedMip, 0, SSR_DEPTH_HIERARCHY_MAX_MIP);
                 ImGui::SliderFloat("GGX Importance Sample Bias", &m_ShaderAttribs.SSRParams.GGXImportanceSampleBias, 0.0f, 1.0f);
                 ImGui::TreePop();
            }

            if (ImGui::TreeNode("Spatial Reconstruction"))  {
                ImGui::SliderFloat("Spatial Reconstruction Radius", &m_ShaderAttribs.SSRParams.SpatialReconstructionRadius, 2.0f, 8.0f);
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Temporal Accumulation"))
            {
                ImGui::SliderFloat("Temporal Stability Radiance Factor", &m_ShaderAttribs.SSRParams.TemporalRadianceStabilityFactor, 0.0f, 1.0f);
                ImGui::SliderFloat("Temporal Stability Variance Factor", &m_ShaderAttribs.SSRParams.TemporalVarianceStabilityFactor, 0.0f, 1.0f);
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Bilateral Cleanup"))
            {
                ImGui::SliderFloat("Bilateral Cleanup Spatial Sigma Factor", &m_ShaderAttribs.SSRParams.BilateralCleanupSpatialSigmaFactor, 0.0f, 4.0f);
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }

        {
            std::pair<GLTF_PBR_Renderer::DebugViewType, const char*> DebugViews[] = {
                {GLTF_PBR_Renderer::DebugViewType::None, "None"},
                {GLTF_PBR_Renderer::DebugViewType::Texcoord0, "Tex coords 0"},
                {GLTF_PBR_Renderer::DebugViewType::Texcoord1, "Tex coords 1"},
                {GLTF_PBR_Renderer::DebugViewType::BaseColor, "Base Color"},
                {GLTF_PBR_Renderer::DebugViewType::Transparency, "Transparency"},
                {GLTF_PBR_Renderer::DebugViewType::Occlusion, "Occlusion"},
                {GLTF_PBR_Renderer::DebugViewType::Emissive, "Emissive"},
                {GLTF_PBR_Renderer::DebugViewType::Metallic, "Metallic"},
                {GLTF_PBR_Renderer::DebugViewType::Roughness, "Roughness"},
                {GLTF_PBR_Renderer::DebugViewType::DiffuseColor, "Diffuse color"},
                {GLTF_PBR_Renderer::DebugViewType::SpecularColor, "Specular color (R0)"},
                {GLTF_PBR_Renderer::DebugViewType::Reflectance90, "Reflectance90"},
                {GLTF_PBR_Renderer::DebugViewType::MeshNormal, "Mesh normal"},
                {GLTF_PBR_Renderer::DebugViewType::ShadingNormal, "Shadign normal"},
                {GLTF_PBR_Renderer::DebugViewType::NdotV, "n*v"},
                {PBR_Renderer::DebugViewType::PunctualLighting, "Punctual Lighting"},
                {GLTF_PBR_Renderer::DebugViewType::DiffuseIBL, "Diffuse IBL"},
                {GLTF_PBR_Renderer::DebugViewType::SpecularIBL, "Specular IBL"},
            };

            ImGui::Combo("Debug view", &m_RenderParams.DebugView, DebugViews, _countof(DebugViews));
        }

        if (ImGui::TreeNode("Renderer Features"))
        {
            auto FeatureCheckbox = [&](const char* Name, GLTF_PBR_Renderer::PSO_FLAGS Flag) {
                bool Enabled = (m_RenderParams.Flags & Flag) != 0;
                if (ImGui::Checkbox(Name, &Enabled))
                {
                    if (Enabled)
                        m_RenderParams.Flags |= Flag;
                    else
                        m_RenderParams.Flags &= ~Flag;
                }
            };
            FeatureCheckbox("Vertex Colors", GLTF_PBR_Renderer::PSO_FLAG_USE_VERTEX_COLORS);
            FeatureCheckbox("Vertex Normals", GLTF_PBR_Renderer::PSO_FLAG_USE_VERTEX_NORMALS);
            FeatureCheckbox("Texcoords", GLTF_PBR_Renderer::PSO_FLAG_USE_TEXCOORD0 | GLTF_PBR_Renderer::PSO_FLAG_USE_TEXCOORD1);
            FeatureCheckbox("Joints", GLTF_PBR_Renderer::PSO_FLAG_USE_JOINTS);
            FeatureCheckbox("Color map", GLTF_PBR_Renderer::PSO_FLAG_USE_COLOR_MAP);
            FeatureCheckbox("Normal map", GLTF_PBR_Renderer::PSO_FLAG_USE_NORMAL_MAP);
            FeatureCheckbox("Phys desc map", GLTF_PBR_Renderer::PSO_FLAG_USE_PHYS_DESC_MAP);
            FeatureCheckbox("Occlusion", GLTF_PBR_Renderer::PSO_FLAG_USE_AO_MAP);
            FeatureCheckbox("Emissive", GLTF_PBR_Renderer::PSO_FLAG_USE_EMISSIVE_MAP);
            FeatureCheckbox("IBL", GLTF_PBR_Renderer::PSO_FLAG_USE_IBL);
            FeatureCheckbox("Tone Mapping", GLTF_PBR_Renderer::PSO_FLAG_ENABLE_TONE_MAPPING);
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Alpha Modes"))
        {
            auto AlphaModeCheckbox = [&](const char* Name, GLTF_PBR_Renderer::RenderInfo::ALPHA_MODE_FLAGS Flag) {
                bool Enabled = (m_RenderParams.AlphaModes & Flag) != 0;
                if (ImGui::Checkbox(Name, &Enabled))
                {
                    if (Enabled)
                        m_RenderParams.AlphaModes |= Flag;
                    else
                        m_RenderParams.AlphaModes &= ~Flag;
                }
            };
            AlphaModeCheckbox("Opaque", GLTF_PBR_Renderer::RenderInfo::ALPHA_MODE_FLAG_OPAQUE);
            AlphaModeCheckbox("Mask", GLTF_PBR_Renderer::RenderInfo::ALPHA_MODE_FLAG_MASK);
            AlphaModeCheckbox("Blend", GLTF_PBR_Renderer::RenderInfo::ALPHA_MODE_FLAG_BLEND);
            ImGui::TreePop();
        }
    }
    ImGui::End();
    ImGui::PopFont();
}

PostProcessDemo::~PostProcessDemo() = default;

// Render a frame
void PostProcessDemo::Render()
{
    float YFov  = PI_F / 4.0f;
    float ZNear = 0.1f;
    float ZFar  = 100.f;

    const auto CameraViewCurr = m_Camera.GetViewMatrix();
    const auto CameraViewPrev = m_CameraViewLast;
    const auto CameraProj     = GetAdjustedProjectionMatrix(YFov, ZNear, ZFar);
    const auto CameraViewProjCurr = CameraViewCurr * CameraProj;
    const auto CameraViewProjPrev = CameraViewPrev * CameraProj;

    {
        MapHelper<HLSL::PBRFrameAttribs> FrameAttribs(m_pImmediateContext, m_pFrameAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
        {
            auto& Camera            = FrameAttribs->Camera;
            Camera.f4ViewportSize.x = static_cast<float>(m_pSwapChain->GetDesc().Width);
            Camera.f4ViewportSize.y = static_cast<float>(m_pSwapChain->GetDesc().Height);
            Camera.f4ViewportSize.z = 1.0f / Camera.f4ViewportSize.x;
            Camera.f4ViewportSize.w = 1.0f / Camera.f4ViewportSize.y;
            Camera.mViewT           = CameraViewCurr.Transpose();
            Camera.mProjT           = CameraProj.Transpose();
            Camera.mViewProjT       = CameraViewProjCurr.Transpose();
            Camera.mProjInvT        = CameraProj.Inverse().Transpose();
            Camera.mViewInvT        = CameraViewCurr.Inverse().Transpose();
            Camera.mViewProjInvT    = CameraViewProjCurr.Inverse().Transpose();
            Camera.mPrevViewT       = CameraViewPrev.Transpose();
            Camera.mPrevProjT       = CameraProj.Transpose();
            Camera.mPrevViewProjT   = CameraViewProjPrev.Transpose();
            Camera.mPrevViewInvT     = CameraViewPrev.Inverse().Transpose();
            Camera.mPrevProjInvT     = CameraProj.Inverse().Transpose();
            Camera.mPrevViewProjInvT = CameraViewProjPrev.Inverse().Transpose();
            Camera.f4Position       = float4(m_Camera.GetPos(), 1);
            Camera.f4ExtraData[0]   = float4(m_ShaderAttribs.SSRParams.SceneRoughness, 0.0f, 0.0f, 0.0f);
        }
        {
            auto& Light     = FrameAttribs->Light;
            Light.Direction = float4(0.0f);
            Light.Intensity = float4(0.0f);
        }
        {
            auto& Renderer = FrameAttribs->Renderer;
            m_GLTFRenderer->SetInternalShaderParameters(Renderer);

            Renderer.OcclusionStrength = m_ShaderAttribs.OcclusionStrength;
            Renderer.EmissionScale     = m_ShaderAttribs.EmissionScale;
            Renderer.AverageLogLum     = m_ShaderAttribs.AverageLogLum;
            Renderer.MiddleGray        = m_ShaderAttribs.MiddleGray;
            Renderer.WhitePoint        = m_ShaderAttribs.WhitePoint;
            Renderer.IBLScale          = m_ShaderAttribs.IBLScale;
            Renderer.HighlightColor    = m_ShaderAttribs.HighlightColor;
            Renderer.UnshadedColor     = m_ShaderAttribs.WireframeColor;
            Renderer.PointSize         = 1;
        }

        MapHelper<HLSL::ToneMappingAttribs> ToneMappingAttribs(m_pImmediateContext, m_pToneMapingAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
        {
            ToneMappingAttribs->iToneMappingMode     = TONE_MAPPING_MODE_UNCHARTED2;
            ToneMappingAttribs->bAutoExposure        = 0;
            ToneMappingAttribs->fMiddleGray          = m_ShaderAttribs.MiddleGray;
            ToneMappingAttribs->bLightAdaptation     = 0;
            ToneMappingAttribs->fWhitePoint          = m_ShaderAttribs.WhitePoint;
            ToneMappingAttribs->fLuminanceSaturation = 1.0;
            ToneMappingAttribs->Padding0 = *reinterpret_cast<uint*>(&m_ShaderAttribs.AverageLogLum);
        }
    }

    {
        ScopedDebugGroup DebugGroup{m_pImmediateContext, "ComputeLighting"};


        const float ClearColor[] = {0.032f, 0.032f, 0.032f, 1.0f};

        auto* pRTV =  m_pColorBuffer->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
        auto* pDSV =  m_pDepthBuffer->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);

        m_pImmediateContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_GLTFRenderer->Begin(m_pImmediateContext);
        m_GLTFRenderer->Render(m_pImmediateContext, *m_Model, m_Transforms, m_RenderParams, &m_ModelResourceBindings);
        m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
    }

    {
        ScopedDebugGroup DebugGroup{m_pImmediateContext, "ComputeMotionVectors"};

        m_pComputeMotionVectorsSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbCameraAttribs")->Set(m_pFrameAttribsCB);
        m_pComputeMotionVectorsSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_InputDepth")->Set(m_pDepthBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

        ITextureView* pRTVs[] = {
            m_pMotionVectors->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),
        };

        m_pImmediateContext->CommitShaderResources(m_pComputeMotionVectorsSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->SetPipelineState(m_pComputeMotionVectorsPSO);
        m_pImmediateContext->SetRenderTargets(_countof(pRTVs), pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
        m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
    }

    {
        ScopedDebugGroup DebugGroup{m_pImmediateContext, "GenerateRoughnessNormal"};

        m_pGenerateRoughnessNormalSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbCameraAttribs")->Set(m_pFrameAttribsCB);
        m_pGenerateRoughnessNormalSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_InputDepth")->Set(m_pDepthBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

        ITextureView* pRTVs[] = {
            m_pMaterialBuffer->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),
            m_pNormalBuffer->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),
        };

        m_pImmediateContext->CommitShaderResources(m_pGenerateRoughnessNormalSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->SetPipelineState(m_pGenerateRoughnessNormalPSO);
        m_pImmediateContext->SetRenderTargets(_countof(pRTVs), pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
        m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
    }

    {
        ScreenSpaceReflection::RenderAttributes SSRRenderAttribs{};
        SSRRenderAttribs.pDevice = m_pDevice;
        SSRRenderAttribs.pDeviceContext = m_pImmediateContext;
        SSRRenderAttribs.pColorBufferSRV = m_pColorBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        SSRRenderAttribs.pDepthBufferSRV = m_pDepthBuffer->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
        SSRRenderAttribs.pNormalBufferSRV = m_pNormalBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        SSRRenderAttribs.pMaterialBufferSRV = m_pMaterialBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        SSRRenderAttribs.pMotionVectorsSRV = m_pMotionVectors->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

        SSRRenderAttribs.SSRAttribs.ProjMatrix = CameraProj.Transpose();
        SSRRenderAttribs.SSRAttribs.ViewMatrix = CameraViewCurr.Transpose();
        SSRRenderAttribs.SSRAttribs.ViewProjMatrix = CameraViewProjCurr.Transpose();
        SSRRenderAttribs.SSRAttribs.InvProjMatrix = CameraProj.Inverse().Transpose();
        SSRRenderAttribs.SSRAttribs.InvViewMatrix = CameraViewCurr.Inverse().Transpose();
        SSRRenderAttribs.SSRAttribs.InvViewProjMatrix = CameraViewProjCurr.Inverse().Transpose();
        SSRRenderAttribs.SSRAttribs.PrevViewProjMatrix = CameraViewProjPrev.Transpose();
        SSRRenderAttribs.SSRAttribs.InvPrevViewProjMatrix = CameraViewPrev.Inverse().Transpose();
        SSRRenderAttribs.SSRAttribs.CameraPosition =  float4(m_Camera.GetPos(), 1);
        
        SSRRenderAttribs.SSRAttribs.RenderSize.x = m_pSwapChain->GetDesc().Width;
        SSRRenderAttribs.SSRAttribs.RenderSize.y = m_pSwapChain->GetDesc().Height;
        SSRRenderAttribs.SSRAttribs.InverseRenderSize.x = 1.0f / static_cast<float>(SSRRenderAttribs.SSRAttribs.RenderSize.x);
        SSRRenderAttribs.SSRAttribs.InverseRenderSize.y = 1.0f / static_cast<float>(SSRRenderAttribs.SSRAttribs.RenderSize.y);
        SSRRenderAttribs.SSRAttribs.FrameIndex = m_CurrentFrameNumber;
        SSRRenderAttribs.SSRAttribs.IBLFactor = m_ShaderAttribs.IBLScale;
        SSRRenderAttribs.SSRAttribs.DepthBufferThickness = m_ShaderAttribs.SSRParams.DepthBufferThickness;
        SSRRenderAttribs.SSRAttribs.RoughnessThreshold = m_ShaderAttribs.SSRParams.RoughnessThreshold;
        SSRRenderAttribs.SSRAttribs.MaxTraversalIntersections = m_ShaderAttribs.SSRParams.MaxTraversalIntersections;
        SSRRenderAttribs.SSRAttribs.MostDetailedMip = m_ShaderAttribs.SSRParams.MostDetailedMip;
        SSRRenderAttribs.SSRAttribs.GGXImportanceSampleBias = m_ShaderAttribs.SSRParams.GGXImportanceSampleBias;
        SSRRenderAttribs.SSRAttribs.SpatialReconstructionRadius = m_ShaderAttribs.SSRParams.SpatialReconstructionRadius;
        SSRRenderAttribs.SSRAttribs.TemporalRadianceStabilityFactor = m_ShaderAttribs.SSRParams.TemporalRadianceStabilityFactor;
        SSRRenderAttribs.SSRAttribs.TemporalVarianceStabilityFactor = m_ShaderAttribs.SSRParams.TemporalVarianceStabilityFactor;
        SSRRenderAttribs.SSRAttribs.BilateralCleanupSpatialSigmaFactor = m_ShaderAttribs.SSRParams.BilateralCleanupSpatialSigmaFactor;
        SSRRenderAttribs.SSRAttribs.RoughnessChannel = 0;
        SSRRenderAttribs.SSRAttribs.IsRoughnessPerceptual = true;
        m_pScreenSpaceReflection->Execute(SSRRenderAttribs);
    }

    {
        ScopedDebugGroup DebugGroup{m_pImmediateContext, "ApplyPostEffects"};

        m_pApplyPostEffectsSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbToneMappingAttribs")->Set(m_pToneMapingAttribsCB);
        m_pApplyPostEffectsSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbCameraAttribs")->Set(m_pFrameAttribsCB);
        m_pApplyPostEffectsSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_TextureRadiance")->Set(m_pColorBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        m_pApplyPostEffectsSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_TextureRadianceSSR")->Set(m_pScreenSpaceReflection->GetSSRRadianceSRV());
        m_pApplyPostEffectsSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_TextureNormalBuffer")->Set(m_pNormalBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        m_pApplyPostEffectsSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_TextureMaterialBuffer")->Set(m_pMaterialBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        m_pApplyPostEffectsSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_BRDF_LUT")->Set(m_GLTFRenderer->GetPreintegratedGGX_SRV());

        ITextureView* pRTVs[] = {
            m_pSwapChain->GetCurrentBackBufferRTV()
        };

        m_pImmediateContext->CommitShaderResources(m_pApplyPostEffectsSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->SetPipelineState(m_pApplyPostEffectsPSO);
        m_pImmediateContext->SetRenderTargets(_countof(pRTVs), pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
        m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
    }

}

void PostProcessDemo::Update(double CurrTime, double ElapsedTime)
{
    m_CameraViewLast = m_Camera.GetViewMatrix();

    m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();
}

} // namespace Diligent
