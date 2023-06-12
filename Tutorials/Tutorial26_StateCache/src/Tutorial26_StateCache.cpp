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

#include "Tutorial26_StateCache.hpp"

#include <random>

#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "FileWrapper.hpp"
#include "CallbackWrapper.hpp"
#include "GraphicsAccessories.hpp"
#include "DataBlobImpl.hpp"
#include "ShaderMacroHelper.hpp"
#include "imgui.h"

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
    return new Tutorial26_StateCache{};
}

Tutorial26_StateCache::Tutorial26_StateCache() :
    m_Scene{std::make_unique<HLSL::SceneAttribs>()}
{
    {
        auto& Light{m_Scene->Light};
        Light.f4Normal    = {0, -1, 0};
        Light.f4Intensity = {1, 1, 1, 15};
        Light.f2PosXZ     = {0, 0};
        Light.f2SizeXZ    = {1.5f, 1.5f};
    }

    {
        auto& MirrorBall{m_Scene->Balls[0]};
        MirrorBall.Center = float3{+2.5f, -3.415f, 1.5f};
        MirrorBall.Radius = 1.5;

        MirrorBall.Mat.BaseColor = float3{1.0, 1.0, 1.0};
        MirrorBall.Mat.Emittance = float3{0.0, 0.0, 0.0};
        MirrorBall.Mat.Type      = MAT_TYPE_MIRROR;
        MirrorBall.Mat.Metallic  = 1.0;
        MirrorBall.Mat.Roughness = 0.0;
        MirrorBall.Mat.IOR       = 1.5;
    }

    {
        auto& GlassBall{m_Scene->Balls[1]};
        GlassBall.Center = float3{-1.5f, -3.415f, 0.5f};
        GlassBall.Radius = 1.5;

        GlassBall.Mat.BaseColor = float3{1.0, 1.0, 1.0};
        GlassBall.Mat.Emittance = float3{0.0, 0.0, 0.0};
        GlassBall.Mat.Type      = MAT_TYPE_GLASS;
        GlassBall.Mat.Metallic  = 0.0;
        GlassBall.Mat.Roughness = 0.0;
        GlassBall.Mat.IOR       = 1.5;
    }

    {
        HLSL::SphereInfo Sphere;
        Sphere.Mat.Type = MAT_TYPE_SMITH_GGX;
        Sphere.Mat.IOR  = 1.5;

        Sphere.Center        = float3{+3.0f, -4.165f, -3.2f};
        Sphere.Radius        = 0.75;
        Sphere.Mat.BaseColor = float3{0.9f, 0.7f, 0.1f};
        Sphere.Mat.Emittance = float3{0.0f, 0.0f, 0.0f};
        Sphere.Mat.Metallic  = 0.9f;
        Sphere.Mat.Roughness = 0.1f;
        m_Scene->Balls[2]    = Sphere;

        Sphere.Center        = float3{+0.5f, -4.165f, -2.5f};
        Sphere.Mat.BaseColor = float3{0.9f, 0.7f, 0.1f};
        Sphere.Mat.Metallic  = 0.9f;
        Sphere.Mat.Roughness = 0.5f;
        m_Scene->Balls[3]    = Sphere;

        Sphere.Center        = float3{-3.3f, -4.165f, -3.5f};
        Sphere.Mat.BaseColor = float3{0.9f, 0.8f, 0.9f};
        Sphere.Mat.Metallic  = 0.2f;
        Sphere.Mat.Roughness = 0.1f;
        m_Scene->Balls[4]    = Sphere;

        Sphere.Center        = float3{-3.7f, -4.165f, +3.5f};
        Sphere.Mat.BaseColor = float3{0.9f, 0.8f, 0.9f};
        Sphere.Mat.Metallic  = 0.2f;
        Sphere.Mat.Roughness = 0.8f;
        m_Scene->Balls[5]    = Sphere;
    }
}

void Tutorial26_StateCache::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);

    Attribs.EngineCI.Features.ComputeShaders = DEVICE_FEATURE_STATE_ENABLED;

    // We do not need the depth buffer from the swap chain in this sample
    Attribs.SCDesc.DepthBufferFormat = TEX_FORMAT_UNKNOWN;
}


void Tutorial26_StateCache::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Controls\n"
                    "  Camera: LMB + WASDQE\n"
                    "  Light:  RMB");

        if (ImGui::SliderInt("Num bounces", &m_NumBounces, 1, 8))
            m_SampleCount = 0;

        if (ImGui::Checkbox("Show only last bounce", &m_ShowOnlyLastBounce))
            m_SampleCount = 0;

        if (ImGui::Checkbox("Next Event Estimation", &m_UseNEE))
            m_SampleCount = 0;

        if (ImGui::Combo("BRDF Sampling mode", &m_BRDFSamplingMode, "Cosine-weighted\0"
                                                                    "Importance Sampling\0"))
        {
            CreatePathTracePSO();
            m_SampleCount = 0;
        }

        if (m_UseNEE)
        {
            if (ImGui::Combo("NEE mode", &m_NEEMode, "Sample Light\0"
                                                     "Sample BRDF\0"
                                                     "MIS\0"
                                                     "MIS - Light part\0"
                                                     "MIS - BRDF part\0"))
            {
                CreatePathTracePSO();
                m_SampleCount = 0;
            }

            if (m_NEEMode >= NEE_MODE_MIS)
            {
                if (ImGui::DragFloat("Balance Heuristics Power", &m_BalanceHeuristicsPower, 0.01f, 1, 4))
                {
                    m_BalanceHeuristicsPower = clamp(m_BalanceHeuristicsPower, 0.01f, 4.f);
                    m_SampleCount            = 0;
                }
            }
        }

        if (ImGui::Checkbox("Full BRDF reflectance term (debugging)", &m_FullBRDFReflectance))
        {
            CreatePathTracePSO();
            m_SampleCount = 0;
        }

        if (ImGui::SliderInt("Samples per frame", &m_NumSamplesPerFrame, 1, 32))
            m_SampleCount = 0;

        ImGui::Text("Samples count: %d", m_SampleCount);
        if (ImGui::Checkbox("Limit Sample Count", &m_LimitSampleCount))
        {
            if (m_LimitSampleCount)
                m_SampleCount = 0;
        }

        if (m_LimitSampleCount)
        {
            if (ImGui::InputInt("Max Samples", &m_MaxSamples, 8, 128))
            {
                m_MaxSamples = std::max(m_MaxSamples, 1);
                if (m_SampleCount > m_MaxSamples)
                {
                    m_SampleCount = 0;
                }
            }
        }

        ImGui::Separator();

        if (ImGui::TreeNodeEx("Scene", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::TreePop();

            bool ResetTracer = false;
            if (ImGui::TreeNode("Light"))
            {
                if (ImGui::SliderFloat("Width", &m_Scene->Light.f2SizeXZ.x, 0.5, 3))
                    ResetTracer = true;

                if (ImGui::SliderFloat("Height", &m_Scene->Light.f2SizeXZ.y, 0.5, 3))
                    ResetTracer = true;

                if (ImGui::SliderFloat("Intensity", &m_Scene->Light.f4Intensity.a, 1, 50))
                    ResetTracer = true;

                if (ImGui::ColorEdit3("Color", &m_Scene->Light.f4Intensity.x))
                    ResetTracer = true;

                ImGui::TreePop();
            }

            for (int i = 0; i < NUM_BALLS; ++i)
            {
                std::string NodeID = "SphereNode";
                NodeID += std::to_string(i);
                if (ImGui::TreeNode(NodeID.c_str(), "Ball %d", i))
                {
                    auto& Mat     = m_Scene->Balls[i].Mat;
                    auto  MatType = Mat.Type - 1;
                    if (ImGui::Combo("Material", &MatType,
                                     "Smith GGX\0"
                                     "Glass\0"
                                     "Mirror\0"))
                    {
                        Mat.Type    = MatType + 1;
                        ResetTracer = true;
                    }

                    if (ImGui::ColorEdit3("Base color", &Mat.BaseColor.x))
                        ResetTracer = true;

                    if (Mat.Type == MAT_TYPE_GLASS)
                    {
                        if (ImGui::SliderFloat("Index of Refraction", &Mat.IOR, 1.0, 2.5))
                            ResetTracer = true;
                    }

                    if (Mat.Type == MAT_TYPE_SMITH_GGX)
                    {
                        if (ImGui::SliderFloat("Metallic", &Mat.Metallic, 0.0, 1.0))
                            ResetTracer = true;
                        if (ImGui::SliderFloat("Roughness", &Mat.Roughness, 0.0, 1.0))
                            ResetTracer = true;
                    }

                    ImGui::TreePop();
                }
            }
            if (ResetTracer)
            {
                m_SampleCount       = 0;
                m_LastFrameViewProj = {}; // Need to update G-buffer
            }
        }

        ImGui::Separator();

        if (m_pStateCache)
        {
            if (ImGui::Button("Reload states"))
            {
                m_pStateCache->Reload();
                m_SampleCount       = 0;
                m_LastFrameViewProj = {}; // Need to update G-buffer
            }
        }

        if (!m_StateCachePath.empty())
        {
            if (ImGui::Button("Delete cache file"))
            {
                FileSystem::DeleteFile(m_StateCachePath.c_str());
                m_pStateCache->Reset();
            }
        }
    }
    ImGui::End();
}

void Tutorial26_StateCache::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    // Create render state cache
    {
        RenderStateCacheCreateInfo CacheCI;
        CacheCI.pDevice  = m_pDevice;
        CacheCI.LogLevel = RENDER_STATE_CACHE_LOG_LEVEL_VERBOSE;
        // Enable hot state reload
        CacheCI.EnableHotReload = true;
        CreateRenderStateCache(CacheCI, &m_pStateCache);
        VERIFY(m_pStateCache, "Failed to create render state cache");
    }

    // Try to load the state cache data
    {
        // Note: there is GetRenderStateCacheFilePath() function that can be used to get the path to the cache file.

        // Get local application data directory.
        m_StateCachePath = FileSystem::GetLocalAppDataDirectory("DiligentEngine-Tutorial26");
        if (!FileSystem::PathExists(m_StateCachePath.c_str()))
        {
            // Create the directory if it does not exist
            FileSystem::CreateDirectory(m_StateCachePath.c_str());
        }

        if (!FileSystem::IsSlash(m_StateCachePath.back()))
            m_StateCachePath.push_back(FileSystem::SlashSymbol);

        m_StateCachePath += "state_cache_";
        // Use different cache files for each device type. This is not required, but is more convenient.
        m_StateCachePath += GetRenderDeviceTypeShortString(m_pDevice->GetDeviceInfo().Type);

        // Use different cache files for debug and release. This is not required, but is more convenient.
#ifdef DILIGENT_DEBUG
        m_StateCachePath += "_d";
#else
        m_StateCachePath += "_r";
#endif
        m_StateCachePath += ".bin";

        if (FileSystem::FileExists(m_StateCachePath.c_str()))
        {
            FileWrapper CacheDataFile{m_StateCachePath.c_str()};
            auto        pCacheData = DataBlobImpl::Create();
            if (CacheDataFile->Read(pCacheData))
            {
                if (m_pStateCache->Load(pCacheData))
                    LOG_INFO_MESSAGE("Successfully loaded state cache file ", m_StateCachePath);
                else
                    LOG_ERROR_MESSAGE("Failed to load state cache file ", m_StateCachePath);
            }
            else
            {
                LOG_ERROR_MESSAGE("Failed to read state cache file ", m_StateCachePath);
            }
        }
        else
        {
            LOG_INFO_MESSAGE("State cache file ", m_StateCachePath, " does not exist");
        }
    }

    CreateUniformBuffer(m_pDevice, sizeof(HLSL::ShaderConstants), "Shader constants CB", &m_pShaderConstantsCB);

    // Create a shader source stream factory to load shaders and DRSN files
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);

    // Create render state notation parser
    {
        RenderStateNotationParserCreateInfo ParserCI;
        // Enable state reloading in the parser
        ParserCI.EnableReload = true;
        CreateRenderStateNotationParser(ParserCI, &m_pRSNParser);
        VERIFY(m_pRSNParser != nullptr, "Failed to create RSN parser");
        // Parse the render state notation file
        auto res = m_pRSNParser->ParseFile("RenderStates.json", pShaderSourceFactory);
        VERIFY(res, "Failed to parse render states file");
    }

    // Create render state notation loader
    {
        RenderStateNotationLoaderCreateInfo LoaderCI;
        LoaderCI.pDevice        = m_pDevice;
        LoaderCI.pParser        = m_pRSNParser;
        LoaderCI.pStateCache    = m_pStateCache;
        LoaderCI.pStreamFactory = pShaderSourceFactory;
        CreateRenderStateNotationLoader(LoaderCI, &m_pRSNLoader);
        VERIFY(m_pRSNLoader, "Failed to create render state loader");
    }

    // Load G-buffer PSO
    {
        LoadPipelineStateInfo LoadInfo;
        LoadInfo.PipelineType = PIPELINE_TYPE_GRAPHICS;
        LoadInfo.Name         = "G-Buffer PSO";

        // Define the callback that is called by the state loader before creating
        // the pipeline to let the application modify some parameters. We will use
        // it to set the render target formats.
        auto ModifyGBufferPSODesc = MakeCallback(
            [](PipelineStateCreateInfo& PSODesc) {
                auto& GraphicsPSOCI    = static_cast<GraphicsPipelineStateCreateInfo&>(PSODesc);
                auto& GraphicsPipeline = GraphicsPSOCI.GraphicsPipeline;

                GraphicsPipeline.NumRenderTargets = 5;

                GraphicsPipeline.RTVFormats[0] = GBuffer::BaseColorFormat;
                GraphicsPipeline.RTVFormats[1] = GBuffer::NormalFormat;
                GraphicsPipeline.RTVFormats[2] = GBuffer::EmittanceFormat;
                GraphicsPipeline.RTVFormats[3] = GBuffer::PhysDescFormat;
                GraphicsPipeline.RTVFormats[4] = GBuffer::DepthFormat;
                GraphicsPipeline.DSVFormat     = TEX_FORMAT_UNKNOWN;
            });

        LoadInfo.ModifyPipeline      = ModifyGBufferPSODesc;
        LoadInfo.pModifyPipelineData = ModifyGBufferPSODesc;
        m_pRSNLoader->LoadPipelineState(LoadInfo, &m_pGBufferPSO);
        VERIFY_EXPR(m_pGBufferPSO);

        m_pGBufferPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "cbConstants")->Set(m_pShaderConstantsCB);
        m_pGBufferPSO->CreateShaderResourceBinding(&m_pGBufferSRB, true);
        VERIFY_EXPR(m_pGBufferSRB);
    }

    // Load the path trace PSO
    CreatePathTracePSO();

    // Load the resolve PSO
    {
        LoadPipelineStateInfo LoadInfo;
        LoadInfo.PipelineType = PIPELINE_TYPE_GRAPHICS;
        LoadInfo.Name         = "Resolve PSO";

        // Define the callback to set the render target and depth stencil formats.
        // These formats are only known at run time, so we can't define them in the
        // render state notation file.
        auto ModifyResolvePSODesc = MakeCallback(
            [this](PipelineStateCreateInfo& PSODesc) {
                auto& GraphicsPSOCI    = static_cast<GraphicsPipelineStateCreateInfo&>(PSODesc);
                auto& GraphicsPipeline = GraphicsPSOCI.GraphicsPipeline;

                GraphicsPipeline.NumRenderTargets = 1;
                GraphicsPipeline.RTVFormats[0]    = m_pSwapChain->GetDesc().ColorBufferFormat;
                GraphicsPipeline.DSVFormat        = m_pSwapChain->GetDesc().DepthBufferFormat;
            });

        LoadInfo.ModifyPipeline      = ModifyResolvePSODesc;
        LoadInfo.pModifyPipelineData = ModifyResolvePSODesc;
        m_pRSNLoader->LoadPipelineState(LoadInfo, &m_pResolvePSO);
        VERIFY_EXPR(m_pResolvePSO);

        m_pResolvePSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "cbConstants")->Set(m_pShaderConstantsCB);
    }

    m_Camera.SetPos(float3{0.0f, 1.0f, -20.0f});
    m_Camera.SetRotationSpeed(0.002f);
    m_Camera.SetMoveSpeed(5.f);
    m_Camera.SetSpeedUpScales(5.f, 10.f);
}

void Tutorial26_StateCache::CreatePathTracePSO()
{
    ShaderMacroHelper Macros;
    Macros.AddShaderMacro("BRDF_SAMPLING_MODE_COS_WEIGHTED", BRDF_SAMPLING_MODE_COS_WEIGHTED);
    Macros.AddShaderMacro("BRDF_SAMPLING_MODE_IMPORTANCE_SAMPLING", BRDF_SAMPLING_MODE_IMPORTANCE_SAMPLING);
    Macros.AddShaderMacro("BRDF_SAMPLING_MODE", m_BRDFSamplingMode);

    Macros.AddShaderMacro("NEE_MODE_LIGHT", NEE_MODE_LIGHT);
    Macros.AddShaderMacro("NEE_MODE_BRDF", NEE_MODE_BRDF);
    Macros.AddShaderMacro("NEE_MODE_MIS", NEE_MODE_MIS);
    Macros.AddShaderMacro("NEE_MODE_MIS_LIGHT", NEE_MODE_MIS_LIGHT);
    Macros.AddShaderMacro("NEE_MODE_MIS_BRDF", NEE_MODE_MIS_BRDF);
    Macros.AddShaderMacro("NEE_MODE", m_NEEMode);

    Macros.AddShaderMacro("OPTIMIZED_BRDF_REFLECTANCE", !m_FullBRDFReflectance);

    auto ModifyShaderCI = MakeCallback(
        [&](ShaderCreateInfo& ShaderCI, SHADER_TYPE Type, bool& AddToLoaderCache) {
            VERIFY_EXPR(Type == SHADER_TYPE_COMPUTE);
            ShaderCI.Macros = Macros;
            // Do not add the shader to the loader's cache as
            // we may be recreating the shader at run-time.
            AddToLoaderCache = false;
        });

    LoadPipelineStateInfo LoadInfo;
    LoadInfo.ModifyShader      = ModifyShaderCI;
    LoadInfo.pModifyShaderData = ModifyShaderCI;
    LoadInfo.PipelineType      = PIPELINE_TYPE_COMPUTE;
    LoadInfo.Name              = "Path Trace PSO";
    // The loader has its own cache that holds objects previously created by the application and
    // uses the object name as the key. In this example we recompile the path tracing
    // pipeline at run time when some of the settings change. The pipeline uses the same name, and
    // we don't want to get old pipeline from the cache, so we set `LoadInfo.AddToCache = false`. Note
    // that the pipeline is always added to the render state cache.
    LoadInfo.AddToCache = false;
    m_pPathTracePSO.Release();
    m_pRSNLoader->LoadPipelineState(LoadInfo, &m_pPathTracePSO);
    VERIFY_EXPR(m_pPathTracePSO);

    m_pPathTracePSO->GetStaticVariableByName(SHADER_TYPE_COMPUTE, "cbConstants")->Set(m_pShaderConstantsCB);
}

void Tutorial26_StateCache::WindowResize(Uint32 Width, Uint32 Height)
{
    m_GBuffer = {};
    m_pPathTraceSRB.Release();
    m_pResolveSRB.Release();
    m_pRadianceAccumulationBuffer.Release();

    float NearPlane   = 0.1f;
    float FarPlane    = 50.f;
    float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
    // Note: don't use GL matrix in GL mode since we are writing depth directly to the G-buffer
    static constexpr bool UseGLProjection = false;
    m_Camera.SetProjAttribs(NearPlane, FarPlane, AspectRatio, PI_F / 4.f, m_pSwapChain->GetDesc().PreTransform, UseGLProjection);
}

void Tutorial26_StateCache::CreateGBuffer()
{
    const auto& SCDesc = m_pSwapChain->GetDesc();

    TextureDesc TexDesc;
    TexDesc.Name      = "G-buffer albedo";
    TexDesc.Type      = RESOURCE_DIM_TEX_2D;
    TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    TexDesc.Format    = GBuffer::BaseColorFormat;
    TexDesc.Width     = SCDesc.Width;
    TexDesc.Height    = SCDesc.Height;
    TexDesc.MipLevels = 1;

    m_pDevice->CreateTexture(TexDesc, nullptr, &m_GBuffer.pBaseColor);
    VERIFY_EXPR(m_GBuffer.pBaseColor);

    TexDesc.Name   = "G-buffer normal";
    TexDesc.Format = GBuffer::NormalFormat;
    m_pDevice->CreateTexture(TexDesc, nullptr, &m_GBuffer.pNormal);
    VERIFY_EXPR(m_GBuffer.pNormal);

    TexDesc.Name   = "G-buffer emittance";
    TexDesc.Format = GBuffer::EmittanceFormat;
    m_pDevice->CreateTexture(TexDesc, nullptr, &m_GBuffer.pEmittance);
    VERIFY_EXPR(m_GBuffer.pEmittance);

    TexDesc.Name   = "G-buffer physical description";
    TexDesc.Format = GBuffer::PhysDescFormat;
    m_pDevice->CreateTexture(TexDesc, nullptr, &m_GBuffer.pPhysDesc);
    VERIFY_EXPR(m_GBuffer.pPhysDesc);

    // Note that since we are generating our G-buffer by ray tracing the scene,
    // we bind depth buffer as render target, not as the depth-stencil buffer.
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
    m_pPathTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_BaseColor")->Set(m_GBuffer.pBaseColor->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    m_pPathTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Normal")->Set(m_GBuffer.pNormal->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    m_pPathTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Emittance")->Set(m_GBuffer.pEmittance->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    m_pPathTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_PhysDesc")->Set(m_GBuffer.pPhysDesc->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    m_pPathTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Depth")->Set(m_GBuffer.pDepth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    m_pPathTraceSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Radiance")->Set(m_pRadianceAccumulationBuffer->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));

    m_pResolveSRB.Release();
    m_pResolvePSO->CreateShaderResourceBinding(&m_pResolveSRB, true);
    m_pResolveSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Radiance")->Set(m_pRadianceAccumulationBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

    m_SampleCount       = 0;
    m_LastFrameViewProj = {};
}

// Render a frame
void Tutorial26_StateCache::Render()
{
    // Create G-buffer, if necessary
    if (!m_GBuffer)
        CreateGBuffer();

    const auto& SCDesc = m_pSwapChain->GetDesc();

    // Update the constant buffer
    bool UpdateGBuffer = false;
    {
        MapHelper<HLSL::ShaderConstants> ShaderData{m_pImmediateContext, m_pShaderConstantsCB, MAP_WRITE, MAP_FLAG_DISCARD};
        ShaderData->u2ScreenSize = uint2{SCDesc.Width, SCDesc.Height};
        ShaderData->f2ScreenSize = float2{static_cast<float>(SCDesc.Width), static_cast<float>(SCDesc.Height)};

        std::mt19937                        gen{static_cast<unsigned int>(m_SampleCount)};
        std::uniform_int_distribution<uint> seed;
        ShaderData->uFrameSeed1 = seed(gen);
        ShaderData->uFrameSeed2 = seed(gen);

        ShaderData->iShowOnlyLastBounce = m_ShowOnlyLastBounce ? 1 : 0;
        ShaderData->iUseNEE             = m_UseNEE ? 1 : 0;

        ShaderData->fBalanceHeuristicsPower = m_BalanceHeuristicsPower;

        auto& LightPos    = m_Scene->Light.f2PosXZ;
        auto& LightSize   = m_Scene->Light.f2SizeXZ;
        auto  AdjustedPos = clamp(LightPos, float2{-4.5, -4.5} + LightSize, float2{+4.5, +4.5} - LightSize);
        if (LightPos != AdjustedPos)
        {
            LightPos            = AdjustedPos;
            m_SampleCount       = 0;
            m_LastFrameViewProj = {};
        }

        ShaderData->Scene = *m_Scene;

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
        ShaderData->fCurrSampleCount = static_cast<float>(m_SampleCount + m_NumSamplesPerFrame);

        ShaderData->iNumBounces         = m_NumBounces;
        ShaderData->iNumSamplesPerFrame = m_NumSamplesPerFrame;

        ShaderData->CameraPos      = m_Camera.GetPos();
        ShaderData->ViewProjMat    = ViewProj.Transpose();
        ShaderData->ViewProjInvMat = ViewProj.Inverse().Transpose();
    }

    // Draw the scene into G-buffer
    if (UpdateGBuffer)
    {
        ITextureView* ppRTVs[] = {
            m_GBuffer.pBaseColor->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),
            m_GBuffer.pNormal->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),
            m_GBuffer.pEmittance->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),
            m_GBuffer.pPhysDesc->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),
            m_GBuffer.pDepth->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET) //
        };
        m_pImmediateContext->SetRenderTargets(_countof(ppRTVs), ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_pImmediateContext->CommitShaderResources(m_pGBufferSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->SetPipelineState(m_pGBufferPSO);
        m_pImmediateContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
    }

    // Path trace
    if (!m_LimitSampleCount || m_SampleCount < m_MaxSamples)
    {
        // Matches the THREAD_GROUP_SIZE in the render state notation file
        static constexpr Uint32 ThreadGroupSize = 8;

        m_pImmediateContext->SetPipelineState(m_pPathTracePSO);
        m_pImmediateContext->CommitShaderResources(m_pPathTraceSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DispatchComputeAttribs DispatchArgs{(SCDesc.Width + ThreadGroupSize - 1) / ThreadGroupSize, (SCDesc.Height + ThreadGroupSize - 1) / ThreadGroupSize};
        m_pImmediateContext->DispatchCompute(DispatchArgs);

        m_SampleCount += m_NumSamplesPerFrame;
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

void Tutorial26_StateCache::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));
    {
        const auto& mouseState = m_InputController.GetMouseState();
        if (m_LastMouseState.PosX >= 0 &&
            m_LastMouseState.PosY >= 0 &&
            (m_LastMouseState.ButtonFlags & MouseState::BUTTON_FLAG_RIGHT) != 0)
        {
            float2 DeltaPos =
                {
                    mouseState.PosX - m_LastMouseState.PosX,
                    mouseState.PosY - m_LastMouseState.PosY //
                };
            if (DeltaPos != float2{})
            {
                constexpr float LightMoveSpeed = 0.01f;

                m_Scene->Light.f2PosXZ += DeltaPos * LightMoveSpeed;

                m_LastFrameViewProj = {};
                m_SampleCount       = 0;
            }
        }
        m_LastMouseState = mouseState;
    }
}

Tutorial26_StateCache::~Tutorial26_StateCache()
{
    // Save cache data
    if (m_pStateCache && !m_StateCachePath.empty())
    {
        RefCntAutoPtr<IDataBlob> pCacheData;
        if (m_pStateCache->WriteToBlob(0, &pCacheData))
        {
            if (pCacheData)
            {
                FileWrapper CacheDataFile{m_StateCachePath.c_str(), EFileAccessMode::Overwrite};
                if (CacheDataFile->Write(pCacheData->GetConstDataPtr(), pCacheData->GetSize()))
                    LOG_INFO_MESSAGE("Successfully saved state cache file ", m_StateCachePath, " (", FormatMemorySize(pCacheData->GetSize()), ").");
            }
        }
    }
}

} // namespace Diligent
