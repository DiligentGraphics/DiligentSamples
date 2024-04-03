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

#include "Tutorial27_PostProcessing.hpp"

#include "imgui.h"
#include "ImGuiUtils.hpp"
#include "ImGuiImplDiligent.hpp"
#include "GBuffer.hpp"
#include "EnvMapRenderer.hpp"
#include "GraphicsTypesX.hpp"
#include "GraphicsUtilities.h"
#include "CommonlyUsedStates.h"
#include "RenderStateCache.hpp"
#include "MapHelper.hpp"
#include "PostFXContext.hpp"
#include "ScopedDebugGroup.hpp"
#include "TemporalAntiAliasing.hpp"
#include "ScreenSpaceReflection.hpp"
#include "ScreenSpaceAmbientOcclusion.hpp"
#include "Bloom.hpp"
#include "ShaderMacroHelper.hpp"
#include "ShaderSourceFactoryUtils.hpp"
#include "TextureUtilities.h"
#include "Utilities/interface/DiligentFXShaderSourceStreamFactory.hpp"
#include "../../Common/src/TexturedCube.hpp"

namespace Diligent
{

static RefCntAutoPtr<IShader> CreateShader(IRenderDevice*          pDevice,
                                           IRenderStateCache*      pStateCache,
                                           const Char*             FileName,
                                           const Char*             EntryPoint,
                                           SHADER_TYPE             Type,
                                           const ShaderMacroArray& Macros = {})
{

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders", &pShaderSourceFactory);
    // Create a compound shader source factory that will be able to load DiligentFX shaders.
    auto pCompoundShaderSourceFactory = CreateCompoundShaderSourceFactory({&DiligentFXShaderSourceStreamFactory::GetInstance(), pShaderSourceFactory});

    ShaderCreateInfo ShaderCI;
    ShaderCI.EntryPoint                      = EntryPoint;
    ShaderCI.FilePath                        = FileName;
    ShaderCI.Macros                          = Macros;
    ShaderCI.SourceLanguage                  = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.Desc.ShaderType                 = Type;
    ShaderCI.Desc.Name                       = EntryPoint;
    ShaderCI.pShaderSourceStreamFactory      = pCompoundShaderSourceFactory;
    ShaderCI.Desc.UseCombinedTextureSamplers = true;
    return RenderDeviceWithCache<false>{pDevice, pStateCache}.CreateShader(ShaderCI);
}

namespace HLSL
{

#include "Shaders/Common/public/BasicStructures.fxh"
#include "Shaders/PostProcess/ToneMapping/public/ToneMappingStructures.fxh"
#include "Shaders/PostProcess/TemporalAntiAliasing/public/TemporalAntiAliasingStructures.fxh"
#include "Shaders/PostProcess/ScreenSpaceReflection/public/ScreenSpaceReflectionStructures.fxh"
#include "Shaders/PostProcess/ScreenSpaceAmbientOcclusion/public/ScreenSpaceAmbientOcclusionStructures.fxh"
#include "Shaders/PostProcess/Bloom/public/BloomStructures.fxh"
#include "Shaders/PBR/public/PBR_Structures.fxh"
#include "../assets/shaders/GeometryStructures.fxh"

} // namespace HLSL

enum GBUFFER_RT : Uint32
{
    GBUFFER_RT_BASE_COLOR,
    GBUFFER_RT_MATERIAL_DATA,
    GBUFFER_RT_NORMAL,
    GBUFFER_RT_MOTION_VECTORS,
    GBUFFER_RT_COUNT
};

enum GBUFFER_RT_FLAG : Uint32
{
    GBUFFER_RT_FLAG_BASE_COLOR     = 1u << GBUFFER_RT_BASE_COLOR,
    GBUFFER_RT_FLAG_MATERIAL_DATA  = 1u << GBUFFER_RT_MATERIAL_DATA,
    GBUFFER_RT_FLAG_NORMAL         = 1u << GBUFFER_RT_NORMAL,
    GBUFFER_RT_FLAG_MOTION_VECTORS = 1u << GBUFFER_RT_MOTION_VECTORS,
    GBUFFER_RT_FLAG_LAST           = GBUFFER_RT_FLAG_MOTION_VECTORS,
    GBUFFER_RT_FLAG_ALL            = (GBUFFER_RT_FLAG_LAST << 1u) - 1u,
};
DEFINE_FLAG_ENUM_OPERATORS(GBUFFER_RT_FLAG);

SampleBase* CreateSample()
{
    return new Tutorial27_PostProcessing();
}

struct Tutorial27_PostProcessing::ShaderSettings
{
    HLSL::PBRRendererShaderParameters        PBRRenderParams = {};
    HLSL::ScreenSpaceReflectionAttribs       SSRSettings     = {};
    HLSL::ScreenSpaceAmbientOcclusionAttribs SSAOSettings    = {};
    HLSL::TemporalAntiAliasingAttribs        TAASettings     = {};
    HLSL::BloomAttribs                       BloomSettings   = {};

    bool  TAAEnabled   = true;
    bool  BloomEnabled = true;
    float SSAOStrength = 1.0;
    float SSRStrength  = 1.0;

    ScreenSpaceAmbientOcclusion::FEATURE_FLAGS SSAOFeatureFlags  = ScreenSpaceAmbientOcclusion::FEATURE_FLAG_NONE;
    ScreenSpaceReflection::FEATURE_FLAGS       SSRFeatureFlags   = ScreenSpaceReflection::FEATURE_FLAG_PREVIOUS_FRAME;
    TemporalAntiAliasing::FEATURE_FLAGS        TAAFeatureFlags   = TemporalAntiAliasing::FEATURE_FLAG_BICUBIC_FILTER;
    Bloom::FEATURE_FLAGS                       BloomFeatureFlags = Bloom::FEATURE_FLAG_NONE;
};

Tutorial27_PostProcessing::Tutorial27_PostProcessing() :
    m_Resources{RESOURCE_IDENTIFIER_COUNT},
    m_CameraAttribs{std::make_unique<HLSL::CameraAttribs[]>(2)},
    m_ObjectAttribs{std::make_unique<HLSL::ObjectAttribs[]>(m_MaxObjectCount)},
    m_MaterialAttribs{std::make_unique<HLSL::MaterialAttribs[]>(m_MaxMaterialCount)}
{
    m_Camera.SetMoveSpeed(4.0f);
    m_Camera.SetPos(float3{-8.75f, 1.25f, 6.5f});
    m_Camera.SetReferenceAxes(float3{1.0f, 0.0f, 0.0f}, float3{0.0f, 1.0f, 0.0f}, false);
    m_Camera.SetLookAt(float3{1.0f, 0.0f, 1.0f});

    m_ObjectTransforms[0].resize(m_MaxObjectCount, float4x4::Identity());
    m_ObjectTransforms[1].resize(m_MaxObjectCount, float4x4::Identity());
}

void Tutorial27_PostProcessing::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    // Initialize G-Buffer
    {
        GBuffer::ElementDesc GBufferElems[GBUFFER_RT_COUNT]{};
        GBufferElems[GBUFFER_RT_BASE_COLOR]     = {TEX_FORMAT_RGBA8_UNORM};
        GBufferElems[GBUFFER_RT_MATERIAL_DATA]  = {TEX_FORMAT_RG8_UNORM};
        GBufferElems[GBUFFER_RT_NORMAL]         = {TEX_FORMAT_RGBA16_FLOAT};
        GBufferElems[GBUFFER_RT_MOTION_VECTORS] = {TEX_FORMAT_RG16_FLOAT};
        static_assert(GBUFFER_RT_COUNT == 4, "Not all G-buffer elements are initialized");
        m_GBuffer = std::make_unique<GBuffer>(GBufferElems, _countof(GBufferElems));
    }

    // Create necessary constant buffers for rendering
    {
        RefCntAutoPtr<IBuffer> pFrameAttribsCB;
        CreateUniformBuffer(m_pDevice, 2 * sizeof(HLSL::CameraAttribs), "Tutorial27_PostProcessing::CameraConstantBuffer", &pFrameAttribsCB);
        m_Resources.Insert(RESOURCE_IDENTIFIER_CAMERA_CONSTANT_BUFFER, pFrameAttribsCB);

        RefCntAutoPtr<IBuffer> pPBRRenderParametersCB;
        CreateUniformBuffer(m_pDevice, sizeof(HLSL::PBRRendererShaderParameters), "Tutorial27_PostProcessing::PBRRenderParameters", &pPBRRenderParametersCB, USAGE_DEFAULT, BIND_UNIFORM_BUFFER, CPU_ACCESS_NONE);
        m_Resources.Insert(RESOURCE_IDENTIFIER_PBR_ATTRIBS_CONSTANT_BUFFER, pPBRRenderParametersCB);

        RefCntAutoPtr<IBuffer> pObjectAttribsCB;
        CreateUniformBuffer(m_pDevice, m_MaxObjectCount * sizeof(HLSL::ObjectAttribs), "Tutorial27_PostProcessing::ObjectAttribs", &pObjectAttribsCB, USAGE_DEFAULT, BIND_UNIFORM_BUFFER, CPU_ACCESS_NONE);
        m_Resources.Insert(RESOURCE_IDENTIFIER_OBJECT_ATTRIBS_CONSTANT_BUFFER, pObjectAttribsCB);

        RefCntAutoPtr<IBuffer> pMaterialAttribsCB;
        CreateUniformBuffer(m_pDevice, m_MaxMaterialCount * sizeof(HLSL::MaterialAttribs), "Tutorial27_PostProcessing::MaterialAttribs", &pMaterialAttribsCB, USAGE_DEFAULT, BIND_UNIFORM_BUFFER, CPU_ACCESS_NONE);
        m_Resources.Insert(RESOURCE_IDENTIFIER_MATERIAL_ATTRIBS_CONSTANT_BUFFER, pMaterialAttribsCB);
    }

    // Create bounding box vertex and index buffers
    {
        m_Resources.Insert(RESOURCE_IDENTIFIER_OBJECT_AABB_VERTEX_BUFFER, TexturedCube::CreateVertexBuffer(m_pDevice, TexturedCube::VERTEX_COMPONENT_FLAG_POSITION));
        m_Resources.Insert(RESOURCE_IDENTIFIER_OBJECT_AABB_INDEX_BUFFER, TexturedCube::CreateIndexBuffer(m_pDevice));
    }

    // Create necessary textures for IBL
    {
        // We only need PBR renderer to precompute environment maps
        const auto pIBLGenerator = std::make_unique<PBR_Renderer>(m_pDevice, nullptr, m_pImmediateContext, PBR_Renderer::CreateInfo{});

        RefCntAutoPtr<ITexture> pEnvironmentMap;
        CreateTextureFromFile("textures/papermill.ktx", TextureLoadInfo{"Tutorial27_PostProcessing::EnvironmentMap"}, m_pDevice, &pEnvironmentMap);
        pIBLGenerator->PrecomputeCubemaps(m_pImmediateContext, pEnvironmentMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE), 128, 64);

        m_Resources.Insert(RESOURCE_IDENTIFIER_ENVIRONMENT_MAP, pEnvironmentMap);
        m_Resources.Insert(RESOURCE_IDENTIFIER_PREFILTERED_ENVIRONMENT_MAP, pIBLGenerator->GetPrefilteredEnvMapSRV()->GetTexture());
        m_Resources.Insert(RESOURCE_IDENTIFIER_IRRADIANCE_MAP, pIBLGenerator->GetIrradianceCubeSRV()->GetTexture());
        m_Resources.Insert(RESOURCE_IDENTIFIER_BRDF_INTEGRATION_MAP, pIBLGenerator->GetPreintegratedGGX_SRV()->GetTexture());
    }

    m_PostFXContext               = std::make_unique<PostFXContext>(m_pDevice);
    m_TemporalAntiAliasing        = std::make_unique<TemporalAntiAliasing>(m_pDevice);
    m_ScreenSpaceReflection       = std::make_unique<ScreenSpaceReflection>(m_pDevice);
    m_ScreenSpaceAmbientOcclusion = std::make_unique<ScreenSpaceAmbientOcclusion>(m_pDevice);
    m_Bloom                       = std::make_unique<Bloom>(m_pDevice);
    m_ShaderSettings              = std::make_unique<ShaderSettings>();

    m_ShaderSettings->PBRRenderParams.OcclusionStrength      = 1.0f;
    m_ShaderSettings->PBRRenderParams.IBLScale               = 1.0f;
    m_ShaderSettings->PBRRenderParams.AverageLogLum          = 0.2f;
    m_ShaderSettings->PBRRenderParams.WhitePoint             = HLSL::ToneMappingAttribs{}.fWhitePoint;
    m_ShaderSettings->PBRRenderParams.MiddleGray             = HLSL::ToneMappingAttribs{}.fMiddleGray;
    m_ShaderSettings->PBRRenderParams.PrefilteredCubeLastMip = static_cast<float>(m_Resources[RESOURCE_IDENTIFIER_PREFILTERED_ENVIRONMENT_MAP].AsTexture()->GetDesc().MipLevels - 1);
    m_ShaderSettings->PBRRenderParams.MipBias                = 0;

    m_ShaderSettings->SSRSettings.MaxTraversalIntersections = 64;
    m_ShaderSettings->SSRSettings.RoughnessThreshold        = 1.0f;
    m_ShaderSettings->SSRSettings.IsRoughnessPerceptual     = true;
    m_ShaderSettings->SSRSettings.RoughnessChannel          = 0;
}

// Render a frame
void Tutorial27_PostProcessing::Render()
{
    const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;
    const Uint32 PrevFrameIdx = (m_CurrentFrameNumber + 0x1) & 0x1;

    const auto& CurrCamAttribs = m_CameraAttribs[CurrFrameIdx];
    const auto& PrevCamAttribs = m_CameraAttribs[PrevFrameIdx];
    {
        MapHelper<HLSL::CameraAttribs> FrameAttribs{m_pImmediateContext, m_Resources[RESOURCE_IDENTIFIER_CAMERA_CONSTANT_BUFFER].AsBuffer(), MAP_WRITE, MAP_FLAG_DISCARD};
        FrameAttribs[0] = CurrCamAttribs;
        FrameAttribs[1] = PrevCamAttribs;
    }

    m_pImmediateContext->UpdateBuffer(m_Resources[RESOURCE_IDENTIFIER_PBR_ATTRIBS_CONSTANT_BUFFER].AsBuffer(), 0, sizeof(HLSL::PBRRendererShaderParameters), &m_ShaderSettings->PBRRenderParams, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->UpdateBuffer(m_Resources[RESOURCE_IDENTIFIER_OBJECT_ATTRIBS_CONSTANT_BUFFER].AsBuffer(), 0, sizeof(HLSL::ObjectAttribs) * m_MaxObjectCount, m_ObjectAttribs.get(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->UpdateBuffer(m_Resources[RESOURCE_IDENTIFIER_MATERIAL_ATTRIBS_CONSTANT_BUFFER].AsBuffer(), 0, sizeof(HLSL::MaterialAttribs) * m_MaxMaterialCount, m_MaterialAttribs.get(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    PrepareResources();
    GenerateGeometry();
    ComputePostFX();
    ComputeSSR();
    ComputeSSAO();
    ComputeLighting();
    ComputeTAA();
    ComputeBloom();
    ApplyToneMap();
}

void Tutorial27_PostProcessing::Update(double CurrTime, double ElapsedTime)
{
    m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0) & 0x01;
    const Uint32 PrevFrameIdx = (m_CurrentFrameNumber + 1) & 0x01;

    constexpr float YFov  = PI_F / 4.0f;
    constexpr float ZNear = 0.1f;
    constexpr float ZFar  = 100.f;

    auto ComputeProjJitterMatrix = [&](const float4x4& ProjMatrix, float2 Jitter) -> float4x4 {
        float4x4 Result = ProjMatrix;
        Result[2][0]    = Jitter.x;
        Result[2][1]    = Jitter.y;
        return Result;
    };

    const float2 Jitter = m_ShaderSettings->TAAEnabled ? m_TemporalAntiAliasing->GetJitterOffset() : float2{0.0f, 0.0f};

    const float4x4 CameraView     = m_Camera.GetViewMatrix();
    const float4x4 CameraProj     = ComputeProjJitterMatrix(GetAdjustedProjectionMatrix(YFov, ZNear, ZFar), Jitter);
    const float4x4 CameraViewProj = CameraView * CameraProj;
    const float4x4 CameraWorld    = CameraView.Inverse();

    const auto& SCDesc = m_pSwapChain->GetDesc();

    auto& CurrCamAttribs          = m_CameraAttribs[CurrFrameIdx];
    CurrCamAttribs.f4ViewportSize = float4{static_cast<float>(SCDesc.Width), static_cast<float>(SCDesc.Height), 1.f / SCDesc.Width, 1.f / SCDesc.Height};
    CurrCamAttribs.mViewT         = CameraView.Transpose();
    CurrCamAttribs.mProjT         = CameraProj.Transpose();
    CurrCamAttribs.mViewProjT     = CameraViewProj.Transpose();
    CurrCamAttribs.mViewInvT      = CameraView.Inverse().Transpose();
    CurrCamAttribs.mProjInvT      = CameraProj.Inverse().Transpose();
    CurrCamAttribs.mViewProjInvT  = CameraViewProj.Inverse().Transpose();
    CurrCamAttribs.f4Position     = float4(float3::MakeVector(CameraWorld[3]), 1);

    CurrCamAttribs.f2Jitter.x       = Jitter.x;
    CurrCamAttribs.f2Jitter.y       = Jitter.y;
    CurrCamAttribs.f4ExtraData[0].x = m_ShaderSettings->SSRStrength;
    CurrCamAttribs.f4ExtraData[0].y = m_ShaderSettings->SSAOStrength;
    {
        m_ObjectCount   = 0;
        m_MaterialCount = 0;

        auto CreateMaterial = [&](const float3& BaseColor, float Roughness, float Metalness) -> Uint32 {
            HLSL::MaterialAttribs MaterialAttribs{};
            MaterialAttribs.BaseColor = float4{BaseColor, 1.0f};
            MaterialAttribs.Metalness = Metalness;
            MaterialAttribs.Roughness = Roughness;

            m_MaterialAttribs[m_MaterialCount] = MaterialAttribs;
            return m_MaterialCount++;
        };

        auto CreateGeometryObjectWithFrequency = [&](const float4x4& Transform,
                                                     Uint32          ObjectType,
                                                     Uint32          MaterialIdx0,
                                                     Uint32          MaterialIdx1,
                                                     Uint32          Dim0,
                                                     Uint32          Dim1,
                                                     float           Frequency0,
                                                     float           Frequency1) -> Uint32 {
            m_ObjectTransforms[CurrFrameIdx][m_ObjectCount] = Transform.Transpose();

            const float4x4 CurrWorldMatrix = m_ObjectTransforms[CurrFrameIdx][m_ObjectCount];
            const float4x4 PrevWorldMatrix = m_ObjectTransforms[PrevFrameIdx][m_ObjectCount];

            HLSL::ObjectAttribs ObjectAttribs{};
            ObjectAttribs.ObjectType                 = ObjectType;
            ObjectAttribs.CurrInvWorldMatrix         = CurrWorldMatrix.Inverse();
            ObjectAttribs.PrevWorldTransform         = PrevWorldMatrix;
            ObjectAttribs.CurrWorldViewProjectMatrix = CurrCamAttribs.mViewProjT * CurrWorldMatrix;
            ObjectAttribs.CurrNormalMatrix           = ObjectAttribs.CurrInvWorldMatrix.Transpose();

            ObjectAttribs.ObjectMaterialIdx0       = MaterialIdx0;
            ObjectAttribs.ObjectMaterialIdx1       = MaterialIdx1;
            ObjectAttribs.ObjectMaterialDim0       = Dim0;
            ObjectAttribs.ObjectMaterialDim1       = Dim1;
            ObjectAttribs.ObjectMaterialFrequency0 = Frequency0;
            ObjectAttribs.ObjectMaterialFrequency1 = Frequency1;

            m_ObjectAttribs[m_ObjectCount] = ObjectAttribs;
            return m_ObjectCount++;
        };

        auto CreateGeometryObject = [&](const float4x4& Transform, Uint32 ObjectType, Uint32 MaterialIdx) -> Uint32 {
            return CreateGeometryObjectWithFrequency(Transform, ObjectType, MaterialIdx, MaterialIdx, 0, 0, 0.0, 0.0);
        };

        constexpr Uint32 SphereCount = 5;

        for (Uint32 SphereIdx = 0; SphereIdx < SphereCount; SphereIdx++)
        {
            const float4x4 Transform   = float4x4::Scale(0.45f) * float4x4::Translation(3.0f - static_cast<float>(SphereIdx) * 0.75f, -0.5f, 1.5f + static_cast<float>(SphereIdx));
            const Uint32   MaterialIdx = CreateMaterial(float3(0.56f, 0.57f, 0.58f), static_cast<float>(SphereIdx) / static_cast<float>(SphereCount - 1), 1.0);
            CreateGeometryObject(Transform, GEOMETRY_TYPE_SPHERE, MaterialIdx);
        }

        for (Uint32 SphereIdx = 0; SphereIdx < SphereCount; SphereIdx++)
        {
            const float4x4 Transform   = float4x4::Scale(0.45f) * float4x4::Translation(3.5f - static_cast<float>(SphereIdx) * 0.75f, +0.5f, 1.5f + static_cast<float>(SphereIdx));
            const Uint32   MaterialIdx = CreateMaterial(float3(0.56f, 0.57f, 0.58f), static_cast<float>(SphereIdx) / static_cast<float>(SphereCount - 1), 0.0);
            CreateGeometryObject(Transform, GEOMETRY_TYPE_SPHERE, MaterialIdx);
        }

        Uint32 Material0 = CreateMaterial(float3(1.00f, 0.71f, 0.29f), 0.05f, 1.0f);
        Uint32 Material1 = CreateMaterial(float3(0.03f, 0.05f, 0.10f), 0.15f, 0.5f);
        Uint32 Material2 = CreateMaterial(float3(0.56f, 0.57f, 0.58f), 0.01f, 1.0f);
        Uint32 Material3 = CreateMaterial(float3(0.24f, 0.24f, 0.84f), 0.50f, 1.0f);
        Uint32 Material4 = CreateMaterial(float3(0.87f, 0.07f, 0.17f), 0.50f, 0.1f);
        Uint32 Material5 = CreateMaterial(float3(0.07f, 0.80f, 0.17f), 0.00f, 0.1f);

        float4x4 Transform0 = float4x4::Scale(20.0f, 0.01f, 20.0f) * float4x4::Translation(0.0f, -1.0f, 0.0f);
        float4x4 Transform1 = float4x4::Scale(1.0f, 1.0f, 0.1f) * float4x4::RotationX(m_AnimationTime) * float4x4::Translation(+3.0f, 0.0f, 0.0f);
        float4x4 Transform2 = float4x4::Scale(1.0f, 1.0f, 0.1f) * float4x4::RotationY(m_AnimationTime) * float4x4::Translation(-3.0f, 0.0f, 0.0f);
        float4x4 Transform3 = float4x4::Translation(0.0f, ::abs(sinf(m_AnimationTime)), 0.0f);
        float4x4 Transform4 = float4x4::Scale(0.3f, 0.3f, 0.3f) * float4x4::RotationZ(m_AnimationTime) * float4x4::Translation(1.0f, 0.5f, 1.0f) * float4x4::RotationY(m_AnimationTime);
        float4x4 Transform5 = float4x4::Scale(0.3f, 0.3f, 0.3f) * float4x4::RotationX(m_AnimationTime) * float4x4::Translation(1.0f, 0.5f, 1.0f) * float4x4::RotationY(m_AnimationTime + PI_F);

        CreateGeometryObjectWithFrequency(Transform0, GEOMETRY_TYPE_AABB, Material0, Material1, 0, 2, 2.0, 2.0);
        CreateGeometryObjectWithFrequency(Transform1, GEOMETRY_TYPE_AABB, Material2, Material3, 0, 2, 4.0, 4.0);
        CreateGeometryObjectWithFrequency(Transform2, GEOMETRY_TYPE_AABB, Material4, Material5, 0, 1, 4.0, 4.0);
        CreateGeometryObject(Transform3, GEOMETRY_TYPE_SPHERE, Material2);
        CreateGeometryObject(Transform4, GEOMETRY_TYPE_AABB, Material3);
        CreateGeometryObject(Transform5, GEOMETRY_TYPE_SPHERE, Material4);

        DEV_CHECK_ERR(m_ObjectCount < m_MaxObjectCount, "Object Count must be lower then Max Object Count");
        DEV_CHECK_ERR(m_MaterialCount < m_MaxMaterialCount, "Material Count must be lower then Max Material Count");

        if (m_IsAnimationActive)
            m_AnimationTime += static_cast<float>(ElapsedTime);
    }
}

void Tutorial27_PostProcessing::WindowResize(Uint32 Width, Uint32 Height)
{
    SampleBase::WindowResize(Width, Height);

    RenderDeviceX_N Device{m_pDevice};

    for (Uint32 TextureIdx = RESOURCE_IDENTIFIER_RADIANCE0; TextureIdx <= RESOURCE_IDENTIFIER_RADIANCE1; TextureIdx++)
    {
        TextureDesc Desc;
        Desc.Name      = "Tutorial27_PostProcessing::Radiance";
        Desc.Type      = RESOURCE_DIM_TEX_2D;
        Desc.Width     = Width;
        Desc.Height    = Height;
        Desc.Format    = TEX_FORMAT_R11G11B10_FLOAT;
        Desc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
        m_Resources.Insert(TextureIdx, Device.CreateTexture(Desc));

        float4        Color = float4(0.0, 0.0, 0.0, 1.0);
        ITextureView* pRTV  = m_Resources[TextureIdx].GetTextureRTV();
        m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearRenderTarget(pRTV, Color.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    for (Uint32 TextureIdx = RESOURCE_IDENTIFIER_DEPTH0; TextureIdx <= RESOURCE_IDENTIFIER_DEPTH1; TextureIdx++)
    {
        TextureDesc Desc;
        Desc.Name      = "Tutorial27_PostProcessing::Depth";
        Desc.Type      = RESOURCE_DIM_TEX_2D;
        Desc.Width     = Width;
        Desc.Height    = Height;
        Desc.Format    = TEX_FORMAT_D32_FLOAT;
        Desc.BindFlags = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
        m_Resources.Insert(TextureIdx, Device.CreateTexture(Desc));

        ITextureView* pDSV = m_Resources[TextureIdx].GetTextureDSV();
        m_pImmediateContext->SetRenderTargets(0, nullptr, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.0, 0xFF, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
}

void Tutorial27_PostProcessing::PrepareResources()
{
    {
        PostFXContext::FrameDesc FrameDesc;
        FrameDesc.Width  = m_pSwapChain->GetDesc().Width;
        FrameDesc.Height = m_pSwapChain->GetDesc().Height;
        FrameDesc.Index  = m_CurrentFrameNumber;
        m_PostFXContext->PrepareResources(m_pDevice, FrameDesc, PostFXContext::FEATURE_FLAG_NONE);
    }

    if (m_ShaderSettings->SSRStrength > 0.0)
    {
        auto ActiveFeatures = m_ShaderSettings->SSRFeatureFlags;
        m_ScreenSpaceReflection->PrepareResources(m_pDevice, m_pImmediateContext, m_PostFXContext.get(), ActiveFeatures);
    }

    if (m_ShaderSettings->SSAOStrength > 0.0)
    {
        auto ActiveFeatures = m_ShaderSettings->SSAOFeatureFlags;
        m_ScreenSpaceAmbientOcclusion->PrepareResources(m_pDevice, m_pImmediateContext, m_PostFXContext.get(), ActiveFeatures);
    }

    if (m_ShaderSettings->TAAEnabled)
    {
        auto ActiveFeatures = m_ShaderSettings->TAAFeatureFlags;
        m_TemporalAntiAliasing->PrepareResources(m_pDevice, m_pImmediateContext, m_PostFXContext.get(), ActiveFeatures);
    }

    if (m_ShaderSettings->BloomEnabled)
    {
        auto ActiveFeatures = m_ShaderSettings->BloomFeatureFlags;
        m_Bloom->PrepareResources(m_pDevice, m_pImmediateContext, m_PostFXContext.get(), ActiveFeatures);
    }
}

void Tutorial27_PostProcessing::GenerateGeometry()
{
    const SwapChainDesc& SCDesc = m_pSwapChain->GetDesc();
    m_GBuffer->Resize(m_pDevice, SCDesc.Width, SCDesc.Height);

    auto& RenderTech = m_RenderTech[RENDER_TECH_GENERATE_GEOMETRY];
    if (!RenderTech.IsInitializedPSO())
    {
        ShaderMacroHelper Macros;
        Macros.Add("MAX_MATERIAL_COUNT", m_MaxMaterialCount);

        const auto VS = CreateShader(m_pDevice, nullptr, "GenerateGeometry.vsh", "GenerateGeometryVS", SHADER_TYPE_VERTEX);
        const auto PS = CreateShader(m_pDevice, nullptr, "GenerateGeometry.psh", "GenerateGeometryPS", SHADER_TYPE_PIXEL, Macros);

        PipelineResourceLayoutDescX ResourceLayout;
        ResourceLayout
            .AddVariable(SHADER_TYPE_PIXEL, "cbCameraAttribs", SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            .AddVariable(SHADER_TYPE_PIXEL, "cbObjectMaterial", SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            .AddVariable(SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL, "cbObjectAttribs", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);

        InputLayoutDescX InputLayout;
        InputLayout.Add(0u, 0u, 3u, VT_FLOAT32, false);

        GraphicsPipelineStateCreateInfoX PipelineCI;
        PipelineCI
            .SetName("Tutorial27_PostProcessing::GenerateGeometry")
            .AddShader(VS)
            .AddShader(PS)
            .AddRenderTarget(m_GBuffer->GetBuffer(GBUFFER_RT_BASE_COLOR)->GetDesc().Format)
            .AddRenderTarget(m_GBuffer->GetBuffer(GBUFFER_RT_MATERIAL_DATA)->GetDesc().Format)
            .AddRenderTarget(m_GBuffer->GetBuffer(GBUFFER_RT_NORMAL)->GetDesc().Format)
            .AddRenderTarget(m_GBuffer->GetBuffer(GBUFFER_RT_MOTION_VECTORS)->GetDesc().Format)
            .SetDepthFormat(m_Resources[RESOURCE_IDENTIFIER_DEPTH0].AsTexture()->GetDesc().Format)
            .SetResourceLayout(ResourceLayout)
            .SetInputLayout(InputLayout)
            .SetBlendDesc(BS_Default)
            .SetDepthStencilDesc(DSS_Default)
            .SetPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLE_LIST)
            .SetRasterizerDesc(RS_SolidFillCullFront);

        RenderTech.PSO = RenderDeviceWithCache<false>{m_pDevice, nullptr}.CreateGraphicsPipelineState(PipelineCI);
        ShaderResourceVariableX{RenderTech.PSO, SHADER_TYPE_PIXEL, "cbCameraAttribs"}.Set(m_Resources[RESOURCE_IDENTIFIER_CAMERA_CONSTANT_BUFFER].AsBuffer());
        ShaderResourceVariableX{RenderTech.PSO, SHADER_TYPE_PIXEL, "cbObjectMaterial"}.Set(m_Resources[RESOURCE_IDENTIFIER_MATERIAL_ATTRIBS_CONSTANT_BUFFER].AsBuffer());
        RenderTech.InitializeSRB(true);
    }

    const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;

    ShaderResourceVariableX ObjectAttribVariable{RenderTech.SRB, SHADER_TYPE_PIXEL, "cbObjectAttribs"};
    ObjectAttribVariable.Set(m_Resources[RESOURCE_IDENTIFIER_OBJECT_ATTRIBS_CONSTANT_BUFFER].AsBuffer());

    ScopedDebugGroup DebugGroup{m_pImmediateContext, "GenerateGeometry"};

    Uint64   Offsets[]  = {0};
    IBuffer* pBuffers[] = {m_Resources[RESOURCE_IDENTIFIER_OBJECT_AABB_VERTEX_BUFFER].AsBuffer()};

    m_GBuffer->Bind(m_pImmediateContext,
                    GBUFFER_RT_FLAG_ALL, // Bind all render targets
                    m_Resources[RESOURCE_IDENTIFIER_DEPTH0 + CurrFrameIdx].GetTextureDSV(),
                    GBUFFER_RT_FLAG_ALL // Clear all render targets
    );
    m_pImmediateContext->ClearDepthStencil(m_Resources[RESOURCE_IDENTIFIER_DEPTH0 + CurrFrameIdx].GetTextureDSV(), CLEAR_DEPTH_FLAG, 1.0, 0xFF, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->SetPipelineState(RenderTech.PSO);
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffers, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->SetIndexBuffer(m_Resources[RESOURCE_IDENTIFIER_OBJECT_AABB_INDEX_BUFFER].AsBuffer(), 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    for (Uint32 ObjectIdx = 0; ObjectIdx < m_ObjectCount; ObjectIdx++)
    {
        ObjectAttribVariable.SetBufferRange(m_Resources[RESOURCE_IDENTIFIER_OBJECT_ATTRIBS_CONSTANT_BUFFER].AsBuffer(), ObjectIdx * sizeof(HLSL::ObjectAttribs), sizeof(HLSL::ObjectAttribs));
        m_pImmediateContext->CommitShaderResources(RenderTech.SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->DrawIndexed({36, VT_UINT32, DRAW_FLAG_VERIFY_ALL});
    }
    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
}

void Tutorial27_PostProcessing::ComputePostFX()
{
    const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;
    const Uint32 PrevFrameIdx = (m_CurrentFrameNumber + 0x1) & 0x1;

    {
        PostFXContext::RenderAttributes PostFXAttibs;
        PostFXAttibs.pDevice             = m_pDevice;
        PostFXAttibs.pDeviceContext      = m_pImmediateContext;
        PostFXAttibs.pCameraAttribsCB    = m_Resources[RESOURCE_IDENTIFIER_CAMERA_CONSTANT_BUFFER].AsBuffer();
        PostFXAttibs.pCurrDepthBufferSRV = m_Resources[RESOURCE_IDENTIFIER_DEPTH0 + CurrFrameIdx].GetTextureSRV();
        PostFXAttibs.pPrevDepthBufferSRV = m_Resources[RESOURCE_IDENTIFIER_DEPTH0 + PrevFrameIdx].GetTextureSRV();
        PostFXAttibs.pMotionVectorsSRV   = m_GBuffer->GetBuffer(GBUFFER_RT_MOTION_VECTORS)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        m_PostFXContext->Execute(PostFXAttibs);
    }
}

void Tutorial27_PostProcessing::ComputeSSR()
{
    if (m_ShaderSettings->SSRStrength > 0.0)
    {
        const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;
        const Uint32 PrevFrameIdx = (m_CurrentFrameNumber + 0x1) & 0x1;

        ScreenSpaceReflection::RenderAttributes SSRRenderAttribs{};
        SSRRenderAttribs.pDevice            = m_pDevice;
        SSRRenderAttribs.pDeviceContext     = m_pImmediateContext;
        SSRRenderAttribs.pPostFXContext     = m_PostFXContext.get();
        SSRRenderAttribs.pColorBufferSRV    = m_ShaderSettings->TAAEnabled ? m_TemporalAntiAliasing->GetAccumulatedFrameSRV(true) : m_Resources[RESOURCE_IDENTIFIER_RADIANCE0 + PrevFrameIdx].GetTextureSRV();
        SSRRenderAttribs.pDepthBufferSRV    = m_Resources[RESOURCE_IDENTIFIER_DEPTH0 + CurrFrameIdx].GetTextureSRV();
        SSRRenderAttribs.pNormalBufferSRV   = m_GBuffer->GetBuffer(GBUFFER_RT_NORMAL)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        SSRRenderAttribs.pMaterialBufferSRV = m_GBuffer->GetBuffer(GBUFFER_RT_MATERIAL_DATA)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        SSRRenderAttribs.pMotionVectorsSRV  = m_GBuffer->GetBuffer(GBUFFER_RT_MOTION_VECTORS)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        SSRRenderAttribs.pSSRAttribs        = &m_ShaderSettings->SSRSettings;
        m_ScreenSpaceReflection->Execute(SSRRenderAttribs);
    }
}

void Tutorial27_PostProcessing::ComputeSSAO()
{
    if (m_ShaderSettings->SSAOStrength > 0.0)
    {
        const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;

        ScreenSpaceAmbientOcclusion::RenderAttributes SSAORenderAttribs{};
        SSAORenderAttribs.pDevice          = m_pDevice;
        SSAORenderAttribs.pDeviceContext   = m_pImmediateContext;
        SSAORenderAttribs.pPostFXContext   = m_PostFXContext.get();
        SSAORenderAttribs.pDepthBufferSRV  = m_Resources[RESOURCE_IDENTIFIER_DEPTH0 + CurrFrameIdx].GetTextureSRV();
        SSAORenderAttribs.pNormalBufferSRV = m_GBuffer->GetBuffer(GBUFFER_RT_NORMAL)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        SSAORenderAttribs.pSSAOAttribs     = &m_ShaderSettings->SSAOSettings;
        m_ScreenSpaceAmbientOcclusion->Execute(SSAORenderAttribs);
    }
}

void Tutorial27_PostProcessing::ComputeLighting()
{
    auto& RenderTech = m_RenderTech[RENDER_TECH_COMPUTE_LIGHTING];
    if (!RenderTech.IsInitializedPSO())
    {
        const auto VS = CreateShader(m_pDevice, nullptr, "FullScreenTriangleVS.fx", "FullScreenTriangleVS", SHADER_TYPE_VERTEX);
        const auto PS = CreateShader(m_pDevice, nullptr, "ComputeLighting.fx", "ComputeLightingPS", SHADER_TYPE_PIXEL);

        PipelineResourceLayoutDescX ResourceLayout;
        ResourceLayout
            .AddVariable(SHADER_TYPE_PIXEL, "cbCameraAttribs", SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            .AddVariable(SHADER_TYPE_PIXEL, "cbPBRRendererAttibs", SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureBaseColor", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureMaterialData", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureNormal", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureDepth", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureSSR", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureSSAO", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureEnvironmentMap", SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureIrradianceMap", SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TexturePrefilteredEnvironmentMap", SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureBRDFIntegrationMap", SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

        ResourceLayout
            .AddImmutableSampler(SHADER_TYPE_PIXEL, "g_TextureEnvironmentMap", Sam_Aniso16xClamp)
            .AddImmutableSampler(SHADER_TYPE_PIXEL, "g_TextureIrradianceMap", Sam_LinearClamp)
            .AddImmutableSampler(SHADER_TYPE_PIXEL, "g_TexturePrefilteredEnvironmentMap", Sam_LinearClamp)
            .AddImmutableSampler(SHADER_TYPE_PIXEL, "g_TextureBRDFIntegrationMap", Sam_LinearClamp);

        RenderTech.InitializePSO(m_pDevice,
                                 nullptr, "Tutorial27_PostProcessing::ComputeLighting",
                                 VS, PS, ResourceLayout,
                                 {
                                     m_Resources[RESOURCE_IDENTIFIER_RADIANCE0].AsTexture()->GetDesc().Format,
                                 },
                                 TEX_FORMAT_UNKNOWN,
                                 DSS_DisableDepth, BS_Default, false);
        ShaderResourceVariableX{RenderTech.PSO, SHADER_TYPE_PIXEL, "cbCameraAttribs"}.Set(m_Resources[RESOURCE_IDENTIFIER_CAMERA_CONSTANT_BUFFER].AsBuffer());
        ShaderResourceVariableX{RenderTech.PSO, SHADER_TYPE_PIXEL, "cbPBRRendererAttibs"}.Set(m_Resources[RESOURCE_IDENTIFIER_PBR_ATTRIBS_CONSTANT_BUFFER].AsBuffer());
        ShaderResourceVariableX{RenderTech.PSO, SHADER_TYPE_PIXEL, "g_TextureEnvironmentMap"}.Set(m_Resources[RESOURCE_IDENTIFIER_ENVIRONMENT_MAP].GetTextureSRV());
        ShaderResourceVariableX{RenderTech.PSO, SHADER_TYPE_PIXEL, "g_TextureIrradianceMap"}.Set(m_Resources[RESOURCE_IDENTIFIER_IRRADIANCE_MAP].GetTextureSRV());
        ShaderResourceVariableX{RenderTech.PSO, SHADER_TYPE_PIXEL, "g_TexturePrefilteredEnvironmentMap"}.Set(m_Resources[RESOURCE_IDENTIFIER_PREFILTERED_ENVIRONMENT_MAP].GetTextureSRV());
        ShaderResourceVariableX{RenderTech.PSO, SHADER_TYPE_PIXEL, "g_TextureBRDFIntegrationMap"}.Set(m_Resources[RESOURCE_IDENTIFIER_BRDF_INTEGRATION_MAP].GetTextureSRV());
        RenderTech.InitializeSRB(true);
    }

    const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;

    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureBaseColor"}.Set(m_GBuffer->GetBuffer(GBUFFER_RT_BASE_COLOR)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureMaterialData"}.Set(m_GBuffer->GetBuffer(GBUFFER_RT_MATERIAL_DATA)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureNormal"}.Set(m_GBuffer->GetBuffer(GBUFFER_RT_NORMAL)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureDepth"}.Set(m_Resources[RESOURCE_IDENTIFIER_DEPTH0 + CurrFrameIdx].GetTextureSRV());
    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureSSR"}.Set(m_ScreenSpaceReflection->GetSSRRadianceSRV());
    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureSSAO"}.Set(m_ScreenSpaceAmbientOcclusion->GetAmbientOcclusionSRV());

    ScopedDebugGroup DebugGroup{m_pImmediateContext, "ComputeLighting"};

    float4 ClearColor = float4(0.0, 0.0, 0.0, 1.0);

    ITextureView* pRTV = m_Resources[RESOURCE_IDENTIFIER_RADIANCE0 + CurrFrameIdx].GetTextureRTV();
    m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->SetPipelineState(RenderTech.PSO);
    m_pImmediateContext->CommitShaderResources(RenderTech.SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
}

void Tutorial27_PostProcessing::ComputeTAA()
{
    if (m_ShaderSettings->TAAEnabled)
    {
        const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;

        TemporalAntiAliasing::RenderAttributes TAARenderAttribs{};
        TAARenderAttribs.pDevice         = m_pDevice;
        TAARenderAttribs.pDeviceContext  = m_pImmediateContext;
        TAARenderAttribs.pPostFXContext  = m_PostFXContext.get();
        TAARenderAttribs.pColorBufferSRV = m_Resources[RESOURCE_IDENTIFIER_RADIANCE0 + CurrFrameIdx].GetTextureSRV();
        TAARenderAttribs.pTAAAttribs     = &m_ShaderSettings->TAASettings;
        m_TemporalAntiAliasing->Execute(TAARenderAttribs);
    }
}

void Tutorial27_PostProcessing::ComputeBloom()
{
    if (m_ShaderSettings->BloomEnabled)
    {
        const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;

        Bloom::RenderAttributes BloomRenderAttribs{};
        BloomRenderAttribs.pDevice         = m_pDevice;
        BloomRenderAttribs.pDeviceContext  = m_pImmediateContext;
        BloomRenderAttribs.pPostFXContext  = m_PostFXContext.get();
        BloomRenderAttribs.pColorBufferSRV = m_ShaderSettings->TAAEnabled ? m_TemporalAntiAliasing->GetAccumulatedFrameSRV() : m_Resources[RESOURCE_IDENTIFIER_RADIANCE0 + CurrFrameIdx].GetTextureSRV();
        BloomRenderAttribs.pBloomAttribs   = &m_ShaderSettings->BloomSettings;
        m_Bloom->Execute(BloomRenderAttribs);
    }
}

void Tutorial27_PostProcessing::ApplyToneMap()
{
    auto& RenderTech = m_RenderTech[RENDER_TECH_APPLY_TONE_MAP];
    if (!RenderTech.IsInitializedPSO())
    {
        const bool ConvertOutputToGamma = (m_pSwapChain->GetDesc().ColorBufferFormat == TEX_FORMAT_RGBA8_UNORM ||
                                           m_pSwapChain->GetDesc().ColorBufferFormat == TEX_FORMAT_BGRA8_UNORM);

        ShaderMacroHelper Macros;
        Macros.Add("TONE_MAPPING_MODE", TONE_MAPPING_MODE_UNCHARTED2);
        Macros.Add("CONVERT_OUTPUT_TO_SRGB", ConvertOutputToGamma);

        const auto VS = CreateShader(m_pDevice, nullptr, "FullScreenTriangleVS.fx", "FullScreenTriangleVS", SHADER_TYPE_VERTEX);
        const auto PS = CreateShader(m_pDevice, nullptr, "ApplyToneMap.fx", "ApplyToneMapPS", SHADER_TYPE_PIXEL, Macros);

        PipelineResourceLayoutDescX ResourceLayout;
        ResourceLayout
            .AddVariable(SHADER_TYPE_PIXEL, "cbPBRRendererAttibs", SHADER_RESOURCE_VARIABLE_TYPE_STATIC)
            .AddVariable(SHADER_TYPE_PIXEL, "g_TextureHDR", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);

        RenderTech.InitializePSO(m_pDevice,
                                 nullptr, "Tutorial27_PostProcessing::ApplyToneMap",
                                 VS, PS, ResourceLayout,
                                 {m_pSwapChain->GetDesc().ColorBufferFormat},
                                 TEX_FORMAT_UNKNOWN,
                                 DSS_DisableDepth, BS_Default, false);
        ShaderResourceVariableX{RenderTech.PSO, SHADER_TYPE_PIXEL, "cbPBRRendererAttibs"}.Set(m_Resources[RESOURCE_IDENTIFIER_PBR_ATTRIBS_CONSTANT_BUFFER].AsBuffer());
        RenderTech.InitializeSRB(true);
    }

    const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;


    ITextureView* HDRTextureSRV = nullptr;
    if (m_ShaderSettings->BloomEnabled)
        HDRTextureSRV = m_Bloom->GetBloomTextureSRV();
    else if (m_ShaderSettings->TAAEnabled)
        HDRTextureSRV = m_TemporalAntiAliasing->GetAccumulatedFrameSRV();
    else
        HDRTextureSRV = m_Resources[RESOURCE_IDENTIFIER_RADIANCE0 + CurrFrameIdx].GetTextureSRV();
    ShaderResourceVariableX{RenderTech.SRB, SHADER_TYPE_PIXEL, "g_TextureHDR"}.Set(HDRTextureSRV);

    ScopedDebugGroup DebugGroup{m_pImmediateContext, "ApplyToneMap"};

    ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->SetPipelineState(RenderTech.PSO);
    m_pImmediateContext->CommitShaderResources(RenderTech.SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);
}

void Tutorial27_PostProcessing::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        ImGui::Text("FPS: %f", m_fSmoothFPS);

        if (ImGui::TreeNode("Rendering"))
        {
            ImGui::SliderFloat("Screen Space Reflection Strength", &m_ShaderSettings->SSRStrength, 0.0f, 1.0f);
            ImGui::SliderFloat("Screen Space Ambient Occlusion Strength", &m_ShaderSettings->SSAOStrength, 0.0f, 1.0f);
            ImGui::Checkbox("Enable Animation", &m_IsAnimationActive);
            ImGui::Checkbox("Enable TAA", &m_ShaderSettings->TAAEnabled);
            ImGui::Checkbox("Enable Bloom", &m_ShaderSettings->BloomEnabled);
            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::TreeNode("Post Processing"))
        {
            if (ImGui::TreeNode("Screen Space Reflections"))
            {
                ScreenSpaceReflection::UpdateUI(m_ShaderSettings->SSRSettings, m_ShaderSettings->SSRFeatureFlags, m_SSRSettingsDisplayMode);
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Screen Space Ambient Occlusion"))
            {
                ScreenSpaceAmbientOcclusion::UpdateUI(m_ShaderSettings->SSAOSettings, m_ShaderSettings->SSAOFeatureFlags);
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Tone mapping"))
            {
                // clang-format off
                ImGui::SliderFloat("Average log lum", &m_ShaderSettings->PBRRenderParams.AverageLogLum, 0.01f, 10.0f);
                ImGui::SliderFloat("Middle gray", &m_ShaderSettings->PBRRenderParams.MiddleGray, 0.01f, 1.0f);
                ImGui::SliderFloat("White point", &m_ShaderSettings->PBRRenderParams.WhitePoint, 0.1f, 20.0f);
                // clang-format on
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Temporal Anti Aliasing"))
            {
                TemporalAntiAliasing::UpdateUI(m_ShaderSettings->TAASettings, m_ShaderSettings->TAAFeatureFlags);
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Bloom"))
            {
                Bloom::UpdateUI(m_ShaderSettings->BloomSettings, m_ShaderSettings->BloomFeatureFlags);
                ImGui::TreePop();
            }

            ImGui::TreePop();
        }
    }
    ImGui::End();
}

void Tutorial27_PostProcessing::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);
    if (Attribs.DeviceType == RENDER_DEVICE_TYPE_GL)
    {
#if GL_SUPPORTED
        EngineGLCreateInfo* pEngineCI   = static_cast<EngineGLCreateInfo*>(&Attribs.EngineCI);
        pEngineCI->PreferredAdapterType = ADAPTER_TYPE_DISCRETE;
#endif
    }
}

} // namespace Diligent
