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

#include "ConstructiveSolidGeometry.hpp"

#include "DeviceContext.h"
#include "GraphicsTypes.h"
#include "imgui.h"
#include "ImGuiUtils.hpp"
#include "ImGuiImplDiligent.hpp"
#include "GraphicsTypesX.hpp"
#include "GraphicsUtilities.h"
#include "CommonlyUsedStates.h"
#include "MapHelper.hpp"
#include "ScopedDebugGroup.hpp"
#include "ShaderSourceFactoryUtils.hpp"
#include "TextureUtilities.h"
#include "RenderStateNotationLoader.h"
#include "Utilities/interface/DiligentFXShaderSourceStreamFactory.hpp"
#include "../../../Tutorials/Common/src/TexturedCube.hpp"
#include "STLLoader.hpp"

namespace Diligent
{

namespace HLSL
{

#include "Shaders/Common/public/BasicStructures.fxh"
#include "Shaders/PostProcess/ToneMapping/public/ToneMappingStructures.fxh"
#include "Shaders/PBR/public/PBR_Structures.fxh"
#include "../assets/Shaders/ConstantBufferDeclarations.hlsli"

} // namespace HLSL

SampleBase* CreateSample()
{
    return new ConstructiveSolidGeometry();
}

ConstructiveSolidGeometry::ConstructiveSolidGeometry() :
    m_CameraAttribs{std::make_unique<HLSL::CameraAttribs[]>(2)},
    m_ObjectAttribs{std::make_unique<HLSL::ObjectAttribs[]>(m_MaxObjectCount)},
    m_MaterialAttribs{std::make_unique<HLSL::MaterialAttribs[]>(m_MaxMaterialCount)},
    m_CSGOperationAttribs{std::make_unique<HLSL::CSGOperationAttribs[]>(m_MaxOperationCount)}
{
    m_Camera.SetMoveSpeed(4.0f);
    m_Camera.SetPos(float3{-8.75f, 1.25f, 6.5f});
    m_Camera.SetReferenceAxes(float3{1.0f, 0.0f, 0.0f}, float3{0.0f, 1.0f, 0.0f}, false);
    m_Camera.SetLookAt(float3{1.0f, 0.0f, 1.0f});
}

void ConstructiveSolidGeometry::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    {
        RefCntAutoPtr<IBuffer> pFrameAttribsCB;
        CreateUniformBuffer(m_pDevice, sizeof(HLSL::CameraAttribs), "ConstructiveSolidGeometry::CameraConstantBuffer", &pFrameAttribsCB);
        m_Resources.Insert(RESOURCE_IDENTIFIER_CAMERA_CONSTANT_BUFFER, pFrameAttribsCB);

        RefCntAutoPtr<IBuffer> pPBRRenderParametersCB;
        CreateUniformBuffer(m_pDevice, sizeof(HLSL::PBRRendererShaderParameters), "ConstructiveSolidGeometry::PBRRenderParameters", &pPBRRenderParametersCB, USAGE_DEFAULT, BIND_UNIFORM_BUFFER, CPU_ACCESS_NONE);
        m_Resources.Insert(RESOURCE_IDENTIFIER_PBR_ATTRIBS_CONSTANT_BUFFER, pPBRRenderParametersCB);

        RefCntAutoPtr<IBuffer> pObjectAttribsCB;
        CreateUniformBuffer(m_pDevice, m_MaxObjectCount * sizeof(HLSL::ObjectAttribs), "ConstructiveSolidGeometry::ObjectAttribs", &pObjectAttribsCB, USAGE_DEFAULT, BIND_UNIFORM_BUFFER, CPU_ACCESS_NONE);
        m_Resources.Insert(RESOURCE_IDENTIFIER_OBJECT_ATTRIBS_CONSTANT_BUFFER, pObjectAttribsCB);

        RefCntAutoPtr<IBuffer> pMaterialAttribsCB;
        CreateUniformBuffer(m_pDevice, m_MaxMaterialCount * sizeof(HLSL::MaterialAttribs), "ConstructiveSolidGeometry::MaterialAttribs", &pMaterialAttribsCB, USAGE_DEFAULT, BIND_UNIFORM_BUFFER, CPU_ACCESS_NONE);
        m_Resources.Insert(RESOURCE_IDENTIFIER_MATERIAL_ATTRIBS_CONSTANT_BUFFER, pMaterialAttribsCB);

        RefCntAutoPtr<IBuffer> pABufferConstantsCB;
        CreateUniformBuffer(m_pDevice, sizeof(HLSL::ABufferConstants), "ConstructiveSolidGeometry::ABufferConstants", &pABufferConstantsCB);
        m_Resources.Insert(RESOURCE_IDENTIFIER_ABUFFER_CONSTANTS_BUFFER, pABufferConstantsCB);

        BufferDesc CSGOpsDesc;
        CSGOpsDesc.Name              = "ConstructiveSolidGeometry::CSGOperations";
        CSGOpsDesc.Size              = m_MaxOperationCount * sizeof(HLSL::CSGOperationAttribs);
        CSGOpsDesc.Mode              = BUFFER_MODE_STRUCTURED;
        CSGOpsDesc.ElementByteStride = sizeof(HLSL::CSGOperationAttribs);
        CSGOpsDesc.BindFlags         = BIND_SHADER_RESOURCE;
        CSGOpsDesc.Usage             = USAGE_DEFAULT;
        RefCntAutoPtr<IBuffer> pCSGOpsBuf;
        m_pDevice->CreateBuffer(CSGOpsDesc, nullptr, &pCSGOpsBuf);
        m_Resources.Insert(RESOURCE_IDENTIFIER_CSG_OPERATIONS_BUFFER, pCSGOpsBuf);
    }

    {
        m_Resources.Insert(RESOURCE_IDENTIFIER_OBJECT_AABB_VERTEX_BUFFER, TexturedCube::CreateVertexBuffer(m_pDevice, GEOMETRY_PRIMITIVE_VERTEX_FLAG_POSITION));
        m_Resources.Insert(RESOURCE_IDENTIFIER_OBJECT_AABB_INDEX_BUFFER, TexturedCube::CreateIndexBuffer(m_pDevice));
    }

    LoadMesh("Models/vh_f_knee_l_NIH3D.stl");

    {
        LoadEnvironmentMap("Textures/papermill.ktx");
    }

    m_PrefilteredCubeLastMip = static_cast<float>(m_Resources[RESOURCE_IDENTIFIER_PREFILTERED_ENVIRONMENT_MAP].AsTexture()->GetDesc().MipLevels - 1);

    RefCntAutoPtr<IRenderStateNotationParser> pRSNParser;
    {
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pRSNStreamFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory("RenderStates", &pRSNStreamFactory);

        CreateRenderStateNotationParser({}, &pRSNParser);
        pRSNParser->ParseFile("RenderStatesLib.drsn", pRSNStreamFactory);
    }

    {
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory("Shaders", &pShaderSourceFactory);

        RefCntAutoPtr<IShaderSourceInputStreamFactory> pCompoundFactory =
            CreateCompoundShaderSourceFactory({&DiligentFXShaderSourceStreamFactory::GetInstance(), pShaderSourceFactory});

        CreateRenderStateNotationLoader({m_pDevice, pRSNParser, pCompoundFactory}, &m_pRSNLoader);
    }

    m_PSOManager = std::make_unique<PipelineStateManager>(m_pRSNLoader);

    auto ModifyToneMappingPSO = [](PipelineStateCreateInfo& PSODesc, void* /*pUserData*/) {
        auto& GraphicsPSOCI    = static_cast<GraphicsPipelineStateCreateInfo&>(PSODesc);
        auto& GraphicsPipeline = GraphicsPSOCI.GraphicsPipeline;

        GraphicsPipeline.NumRenderTargets = 1;
        GraphicsPipeline.RTVFormats[0]    = TEX_FORMAT_RGBA8_UNORM_SRGB;
        GraphicsPipeline.DSVFormat        = TEX_FORMAT_UNKNOWN;
    };

    m_PSOManager->RegisterPSO(PSO_IDENTIFIER_TONE_MAPPING, "ToneMapping",
                              {.PipelineType   = PIPELINE_TYPE_GRAPHICS,
                               .ModifyCallback = ModifyToneMappingPSO});

    auto ModifyGammaCorrectionPSO = [](PipelineStateCreateInfo& PSODesc, void* pUserData) {
        auto* pSelf            = static_cast<ConstructiveSolidGeometry*>(pUserData);
        auto& GraphicsPSOCI    = static_cast<GraphicsPipelineStateCreateInfo&>(PSODesc);
        auto& GraphicsPipeline = GraphicsPSOCI.GraphicsPipeline;

        GraphicsPipeline.NumRenderTargets = 1;
        GraphicsPipeline.RTVFormats[0]    = pSelf->m_pSwapChain->GetDesc().ColorBufferFormat;
        GraphicsPipeline.DSVFormat        = TEX_FORMAT_UNKNOWN;
    };

    auto ModifyGenerateCSGGeometryPSO = [](PipelineStateCreateInfo& PSODesc, void* pUserData) {
        auto* pSelf            = static_cast<ConstructiveSolidGeometry*>(pUserData);
        auto& GraphicsPSOCI    = static_cast<GraphicsPipelineStateCreateInfo&>(PSODesc);
        auto& GraphicsPipeline = GraphicsPSOCI.GraphicsPipeline;

        GraphicsPipeline.DSVFormat = pSelf->m_pSwapChain->GetDesc().DepthBufferFormat;
    };

    m_PSOManager->RegisterPSO(PSO_IDENTIFIER_GAMMA_CORRECTION, "GammaCorrection",
                              {.PipelineType   = PIPELINE_TYPE_GRAPHICS,
                               .ModifyCallback = ModifyGammaCorrectionPSO,
                               .pCallbackData  = this});

    m_PSOManager->RegisterPSO(PSO_IDENTIFIER_CLEAR_ABUFFER, "ClearABuffer",
                              {.PipelineType = PIPELINE_TYPE_COMPUTE});

    m_PSOManager->RegisterPSO(PSO_IDENTIFIER_GENERATE_CSG_GEOMETRY, "GenerateCSGGeometry",
                              {.PipelineType   = PIPELINE_TYPE_GRAPHICS,
                               .ModifyCallback = ModifyGenerateCSGGeometryPSO,
                               .pCallbackData  = this});

    m_PSOManager->RegisterPSO(PSO_IDENTIFIER_GENERATE_CSG_MESH, "GenerateCSGMesh",
                              {.PipelineType   = PIPELINE_TYPE_GRAPHICS,
                               .ModifyCallback = ModifyGenerateCSGGeometryPSO,
                               .pCallbackData  = this});

    m_PSOManager->RegisterPSO(PSO_IDENTIFIER_RESOLVE_CSG, "ResolveCSG",
                              {.PipelineType = PIPELINE_TYPE_COMPUTE});
}

void ConstructiveSolidGeometry::Render()
{
    const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0x0) & 0x1;

    const HLSL::CameraAttribs& CurrCamAttribs = m_CameraAttribs[CurrFrameIdx];
    {
        MapHelper<HLSL::CameraAttribs> FrameAttribs{m_pImmediateContext, m_Resources[RESOURCE_IDENTIFIER_CAMERA_CONSTANT_BUFFER].AsBuffer(), MAP_WRITE, MAP_FLAG_DISCARD};
        *FrameAttribs = CurrCamAttribs;
    }

    HLSL::PBRRendererShaderParameters PBRRenderParams{};
    PBRRenderParams.OcclusionStrength      = 1.0f;
    PBRRenderParams.IBLScale               = float4{1.0f};
    PBRRenderParams.AverageLogLum          = m_AverageLogLum;
    PBRRenderParams.WhitePoint             = m_WhitePoint;
    PBRRenderParams.MiddleGray             = m_MiddleGray;
    PBRRenderParams.PrefilteredCubeLastMip = m_PrefilteredCubeLastMip;
    PBRRenderParams.MipBias                = 0;

    m_pImmediateContext->UpdateBuffer(m_Resources[RESOURCE_IDENTIFIER_PBR_ATTRIBS_CONSTANT_BUFFER].AsBuffer(), 0, sizeof(HLSL::PBRRendererShaderParameters), &PBRRenderParams, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->UpdateBuffer(m_Resources[RESOURCE_IDENTIFIER_OBJECT_ATTRIBS_CONSTANT_BUFFER].AsBuffer(), 0, sizeof(HLSL::ObjectAttribs) * m_MaxObjectCount, m_ObjectAttribs.get(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->UpdateBuffer(m_Resources[RESOURCE_IDENTIFIER_MATERIAL_ATTRIBS_CONSTANT_BUFFER].AsBuffer(), 0, sizeof(HLSL::MaterialAttribs) * m_MaxMaterialCount, m_MaterialAttribs.get(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->UpdateBuffer(m_Resources[RESOURCE_IDENTIFIER_CSG_OPERATIONS_BUFFER].AsBuffer(), 0, sizeof(HLSL::CSGOperationAttribs) * m_MaxOperationCount, m_CSGOperationAttribs.get(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        const auto&            SCDesc = m_pSwapChain->GetDesc();
        HLSL::ABufferConstants ABufferConst{};
        ABufferConst.ScreenSize     = {SCDesc.Width, SCDesc.Height};
        ABufferConst.MaxNodeCount   = SCDesc.Width * SCDesc.Height * m_MaxABufferFragmentsPerPixel;
        ABufferConst.OperationCount = m_OperationCount;
        ABufferConst.LightDirection = float4{normalize(float3{0.57735f, -0.57735f, 0.57735f}), 0.0f};
        ABufferConst.LightColor     = float4{0.8f, 0.8f, 0.8f, 0.0f};
        ABufferConst.AmbientColor   = float4{0.2f, 0.2f, 0.2f, 0.0f};

        MapHelper<HLSL::ABufferConstants> MappedABuffer{m_pImmediateContext, m_Resources[RESOURCE_IDENTIFIER_ABUFFER_CONSTANTS_BUFFER].AsBuffer(), MAP_WRITE, MAP_FLAG_DISCARD};
        *MappedABuffer = ABufferConst;
    }

    PrepareResources();
    ClearABuffer();
    GenerateCSGGeometry();
    GenerateCSGMesh();
    ResolveCSG();
    ComputeToneMapping();
    ComputeGammaCorrection();
}

void ConstructiveSolidGeometry::Update(double CurrTime, double ElapsedTime, bool DoUpdateUI)
{
    m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));
    SampleBase::Update(CurrTime, ElapsedTime, DoUpdateUI);

    const Uint32 CurrFrameIdx = (m_CurrentFrameNumber + 0) & 0x01;

    const SwapChainDesc& SCDesc = m_pSwapChain->GetDesc();

    constexpr float YFov  = PI_F / 4.0f;
    constexpr float ZNear = 0.1f;
    constexpr float ZFar  = 100.f;

    const float4x4 CameraView     = m_Camera.GetViewMatrix();
    const float4x4 CameraProj     = GetAdjustedProjectionMatrix(YFov, ZNear, ZFar);
    const float4x4 CameraViewProj = CameraView * CameraProj;
    const float4x4 CameraWorld    = CameraView.Inverse();

    float2 Resolution = float2{static_cast<float>(SCDesc.Width), static_cast<float>(SCDesc.Height)};

    HLSL::CameraAttribs& CurrCamAttribs{m_CameraAttribs[CurrFrameIdx]};
    CurrCamAttribs.f4ViewportSize = float4{Resolution.x, Resolution.y, 1.f / Resolution.x, 1.f / Resolution.y};
    CurrCamAttribs.mView          = CameraView;
    CurrCamAttribs.mProj          = CameraProj;
    CurrCamAttribs.mViewProj      = CameraViewProj;
    CurrCamAttribs.mViewInv       = CameraView.Inverse();
    CurrCamAttribs.mProjInv       = CameraProj.Inverse();
    CurrCamAttribs.mViewProjInv   = CameraViewProj.Inverse();
    CurrCamAttribs.f4Position     = float4(float3::MakeVector(CameraWorld[3]), 1);
    CurrCamAttribs.f2Jitter       = float2{0.0f, 0.0f};

    {
        m_ObjectCount    = 0;
        m_MaterialCount  = 0;
        m_OperationCount = 0;

        auto CreateMaterial = [&](const float3& BaseColor, float Roughness, float Metalness) -> Uint32 {
            HLSL::MaterialAttribs MaterialAttribs{};
            MaterialAttribs.BaseColor = float4{BaseColor, 1.0f};
            MaterialAttribs.Metalness = Metalness;
            MaterialAttribs.Roughness = Roughness;

            m_MaterialAttribs[m_MaterialCount] = MaterialAttribs;
            return m_MaterialCount++;
        };

        auto CreateObject = [&](const float4x4& Transform, Uint32 ObjectType, Uint32 MaterialIdx) -> Uint32 {
            const float4x4 CurrWorldMatrix = Transform;

            HLSL::ObjectAttribs ObjectAttribs{};
            ObjectAttribs.ObjectID                   = m_ObjectCount;
            ObjectAttribs.ObjectType                 = ObjectType;
            ObjectAttribs.MaterialIdx                = MaterialIdx;
            ObjectAttribs.CurrInvWorldMatrix         = CurrWorldMatrix.Inverse();
            ObjectAttribs.CurrWorldMatrix            = CurrWorldMatrix;
            ObjectAttribs.CurrWorldViewProjectMatrix = CurrWorldMatrix * CurrCamAttribs.mViewProj;
            ObjectAttribs.CurrNormalMatrix           = ObjectAttribs.CurrInvWorldMatrix.Transpose();

            m_ObjectAttribs[m_ObjectCount] = ObjectAttribs;
            return m_ObjectCount++;
        };

        auto AddCSGOperation = [&](Uint32 PrimaryObjectID, Uint32 SecondaryObjectID, Uint32 Operation, Uint32 PrimaryMaterialIdx) {
            VERIFY_EXPR(m_OperationCount < m_MaxOperationCount);
            HLSL::CSGOperationAttribs& Op = m_CSGOperationAttribs[m_OperationCount];
            Op.PrimaryObjectID            = PrimaryObjectID;
            Op.SecondaryObjectID          = SecondaryObjectID;
            Op.Operation                  = Operation;
            Op.PrimaryMaterialIdx         = PrimaryMaterialIdx;
            return m_OperationCount++;
        };

        Uint32 MeshMaterial  = CreateMaterial(float3(0.85f, 0.85f, 0.90f), 0.8f, 0.0f);
        Uint32 PlaneMaterial = CreateMaterial(float3(0.6f, 0.1f, 0.55f), 0.9f, 0.0f);

        struct SphereDesc
        {
            float3 Color;
            float  Roughness;
            float  Metalness;
        };

        const SphereDesc SphereMaterials[] = {
            {{0.90f, 0.10f, 0.10f}, 0.15f, 0.0f}, // Red
            {{0.10f, 0.80f, 0.10f}, 0.20f, 0.0f}, // Green
            {{0.70f, 0.10f, 0.80f}, 0.10f, 0.2f}, // Purple
            {{0.90f, 0.50f, 0.10f}, 0.30f, 0.1f}, // Orange
        };

        const float3 PairCenters[] = {
            {-2.0f, 0.0f, -2.0f},
            {+2.0f, 0.0f, -2.0f},
            {-2.0f, 0.0f, +2.0f},
            {+2.0f, 0.0f, +2.0f},
        };

        const Uint32 PairCSGOps[] = {
            CSG_OP_UNION,
            CSG_OP_SUBTRACTION,
            CSG_OP_INTERSECTION,
            CSG_OP_SYMMETRIC_DIFFERENCE,
        };

        constexpr Uint32 NumPairs = _countof(PairCenters);
        constexpr float3 BBoxCenter{0.1186f, -0.3472f, -0.0680f};
        constexpr float  MeshScale = 4.6f;

        Uint32 MeshObjIndices[NumPairs]   = {};
        Uint32 SphereObjIndices[NumPairs] = {};

        // --- Mesh objects (contiguous range for GenerateCSGMesh) ---
        m_MeshObjectStart = m_ObjectCount;
        for (Uint32 Idx = 0; Idx < NumPairs; ++Idx)
        {
            float4x4 Transform = float4x4::Translation(-BBoxCenter) *
                float4x4::Scale(MeshScale) *
                float4x4::RotationX(-PI_F * 0.5f) *
                float4x4::Translation(PairCenters[Idx]);
            MeshObjIndices[Idx] = CreateObject(Transform, GEOMETRY_TYPE_MESH, MeshMaterial);
        }
        m_MeshObjectCount = m_ObjectCount - m_MeshObjectStart;

        // --- Ground plane ---
        Uint32 PlaneObj = CreateObject(
            float4x4::Scale(5.0f, 0.005f, 5.0f) * float4x4::Translation(0.0f, m_PlaneHeight, 0.0f),
            GEOMETRY_TYPE_AABB, PlaneMaterial);

        if (m_PlaneIsPrimary)
            AddCSGOperation(PlaneObj, MeshObjIndices[0], static_cast<Uint32>(m_PlaneCSGOp), PlaneMaterial);
        else
            AddCSGOperation(MeshObjIndices[0], PlaneObj, static_cast<Uint32>(m_PlaneCSGOp), MeshMaterial);

        // --- Sphere objects (contiguous range for GenerateCSGGeometry) ---
        for (Uint32 Idx = 0; Idx < NumPairs; ++Idx)
        {
            float  Phase       = m_AnimationTime * 1.5f + float(Idx) * PI_F * 0.5f;
            float  OrbitRadius = 0.08f;
            float3 AnimOffset  = float3(std::sin(Phase) * OrbitRadius, std::cos(Phase * 0.7f) * OrbitRadius * 0.5f, std::cos(Phase) * OrbitRadius);

            Uint32 SphereMat      = CreateMaterial(SphereMaterials[Idx].Color, SphereMaterials[Idx].Roughness, SphereMaterials[Idx].Metalness);
            SphereObjIndices[Idx] = CreateObject(float4x4::Scale(0.15f) * float4x4::Translation(PairCenters[Idx] + AnimOffset), GEOMETRY_TYPE_SPHERE, SphereMat);
        }

        // --- Sphere-mesh CSG operations ---
        for (Uint32 Idx = 0; Idx < NumPairs; ++Idx)
            AddCSGOperation(MeshObjIndices[Idx], SphereObjIndices[Idx], PairCSGOps[Idx], MeshMaterial);


        DEV_CHECK_ERR(m_ObjectCount < m_MaxObjectCount, "Object Count must be lower then Max Object Count");
        DEV_CHECK_ERR(m_MaterialCount < m_MaxMaterialCount, "Material Count must be lower then Max Material Count");
        DEV_CHECK_ERR(m_OperationCount <= m_MaxOperationCount, "Operation Count must not exceed Max Operation Count");

        if (m_IsAnimationActive)
            m_AnimationTime += static_cast<float>(ElapsedTime);
    }
}

void ConstructiveSolidGeometry::WindowResize(Uint32 Width, Uint32 Height)
{
    SampleBase::WindowResize(Width, Height);
}

void ConstructiveSolidGeometry::PrepareResources()
{
    RenderDeviceX_N Device{m_pDevice};
    const auto&     SCDesc = m_pSwapChain->GetDesc();

    if (!m_Resources[RESOURCE_IDENTIFIER_RADIANCE] ||
        SCDesc.Width != m_Resources[RESOURCE_IDENTIFIER_RADIANCE].AsTexture()->GetDesc().Width ||
        SCDesc.Height != m_Resources[RESOURCE_IDENTIFIER_RADIANCE].AsTexture()->GetDesc().Height)
    {

        {
            TextureDesc Desc;
            Desc.Name      = "ConstructiveSolidGeometry::Radiance";
            Desc.Type      = RESOURCE_DIM_TEX_2D;
            Desc.Width     = SCDesc.Width;
            Desc.Height    = SCDesc.Height;
            Desc.Format    = TEX_FORMAT_RGBA16_FLOAT;
            Desc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET | BIND_UNORDERED_ACCESS;
            m_Resources.Insert(RESOURCE_IDENTIFIER_RADIANCE, Device.CreateTexture(Desc));

            float4        Color = float4(0.0, 0.0, 0.0, 1.0);
            ITextureView* pRTV  = m_Resources[RESOURCE_IDENTIFIER_RADIANCE].GetTextureRTV();
            m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->ClearRenderTarget(pRTV, Color.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        }

        {
            TextureDesc Desc;
            Desc.Name      = "ConstructiveSolidGeometry::ToneMapping";
            Desc.Type      = RESOURCE_DIM_TEX_2D;
            Desc.Width     = SCDesc.Width;
            Desc.Height    = SCDesc.Height;
            Desc.Format    = TEX_FORMAT_RGBA8_UNORM_SRGB;
            Desc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
            m_Resources.Insert(RESOURCE_IDENTIFIER_TONE_MAPPING, Device.CreateTexture(Desc));
        }

        {
            const Uint32 PixelCount   = SCDesc.Width * SCDesc.Height;
            const Uint32 MaxNodeCount = PixelCount * m_MaxABufferFragmentsPerPixel;

            BufferDesc HeadDesc;
            HeadDesc.Name      = "ConstructiveSolidGeometry::ABuffer_HeadPointers";
            HeadDesc.Size      = static_cast<Uint64>(PixelCount) * sizeof(Uint32);
            HeadDesc.Mode      = BUFFER_MODE_RAW;
            HeadDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
            HeadDesc.Usage     = USAGE_DEFAULT;
            m_Resources.Insert(RESOURCE_IDENTIFIER_ABUFFER_HEAD_POINTER_BUFFER, Device.CreateBuffer(HeadDesc));

            BufferDesc NodeDesc;
            NodeDesc.Name              = "ConstructiveSolidGeometry::ABuffer_FragmentBuffer";
            NodeDesc.Size              = MaxNodeCount * sizeof(HLSL::ABufferFragment);
            NodeDesc.Mode              = BUFFER_MODE_STRUCTURED;
            NodeDesc.ElementByteStride = sizeof(HLSL::ABufferFragment);
            NodeDesc.BindFlags         = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
            NodeDesc.Usage             = USAGE_DEFAULT;
            m_Resources.Insert(RESOURCE_IDENTIFIER_ABUFFER_NODE_BUFFER, Device.CreateBuffer(NodeDesc));

            BufferDesc CounterDesc;
            CounterDesc.Name              = "ConstructiveSolidGeometry::ABuffer_Counter";
            CounterDesc.Size              = sizeof(Uint32);
            CounterDesc.Mode              = BUFFER_MODE_STRUCTURED;
            CounterDesc.ElementByteStride = sizeof(Uint32);
            CounterDesc.BindFlags         = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
            CounterDesc.Usage             = USAGE_DEFAULT;
            m_Resources.Insert(RESOURCE_IDENTIFIER_ABUFFER_COUNTER_BUFFER, Device.CreateBuffer(CounterDesc));

            m_PSOManager->InvalidateSRBs();
        }
    }

    {
        float4        ClearColor = float4(0.0, 0.0, 0.0, 1.0);
        ITextureView* pRTV       = m_Resources[RESOURCE_IDENTIFIER_RADIANCE].GetTextureRTV();

        m_pImmediateContext->SetRenderTargets(1, &pRTV, m_pSwapChain->GetDepthBufferDSV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(m_pSwapChain->GetDepthBufferDSV(), CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
}

void ConstructiveSolidGeometry::ClearABuffer()
{
    auto [pPSO, pSRB] = m_PSOManager->RequestPSO(PSO_IDENTIFIER_CLEAR_ABUFFER);
    if (!pPSO || !pSRB) return;

    ShaderResourceVariableX{pSRB, SHADER_TYPE_COMPUTE, "cbABufferConstants"}.Set(m_Resources[RESOURCE_IDENTIFIER_ABUFFER_CONSTANTS_BUFFER].AsBuffer());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_COMPUTE, "g_BufferHeadPointers"}.Set(m_Resources[RESOURCE_IDENTIFIER_ABUFFER_HEAD_POINTER_BUFFER].GetBufferUAV());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_COMPUTE, "g_BufferCounter"}.Set(m_Resources[RESOURCE_IDENTIFIER_ABUFFER_COUNTER_BUFFER].GetBufferUAV());

    ScopedDebugGroup DebugGroup{m_pImmediateContext, "ClearABuffer"};

    const auto& SCDesc = m_pSwapChain->GetDesc();
    m_pImmediateContext->SetPipelineState(pPSO);
    m_pImmediateContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DispatchComputeAttribs DispatchAttrs{DivCeil(SCDesc.Width, m_ThreadGroupSize), DivCeil(SCDesc.Height, m_ThreadGroupSize), 1};
    m_pImmediateContext->DispatchCompute(DispatchAttrs);
}

void ConstructiveSolidGeometry::GenerateCSGGeometry()
{
    auto [pPSO, pSRB] = m_PSOManager->RequestPSO(PSO_IDENTIFIER_GENERATE_CSG_GEOMETRY);
    if (!pPSO || !pSRB) return;

    ShaderResourceVariableX{pSRB, SHADER_TYPE_PIXEL, "cbCameraAttribs"}.Set(m_Resources[RESOURCE_IDENTIFIER_CAMERA_CONSTANT_BUFFER].AsBuffer());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_PIXEL, "cbABufferConstants"}.Set(m_Resources[RESOURCE_IDENTIFIER_ABUFFER_CONSTANTS_BUFFER].AsBuffer());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_PIXEL, "g_BufferHeadPointers"}.Set(m_Resources[RESOURCE_IDENTIFIER_ABUFFER_HEAD_POINTER_BUFFER].GetBufferUAV());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_PIXEL, "g_BufferNodes"}.Set(m_Resources[RESOURCE_IDENTIFIER_ABUFFER_NODE_BUFFER].GetBufferUAV());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_PIXEL, "g_BufferCounter"}.Set(m_Resources[RESOURCE_IDENTIFIER_ABUFFER_COUNTER_BUFFER].GetBufferUAV());
    ShaderResourceVariableX ObjectAttribVariable{pSRB, SHADER_TYPE_PIXEL, "cbObjectAttribs"};
    ObjectAttribVariable.Set(m_Resources[RESOURCE_IDENTIFIER_OBJECT_ATTRIBS_CONSTANT_BUFFER].AsBuffer());

    ScopedDebugGroup DebugGroup{m_pImmediateContext, "GenerateCSGGeometry"};

    Uint64   Offsets[]  = {0};
    IBuffer* pBuffers[] = {m_Resources[RESOURCE_IDENTIFIER_OBJECT_AABB_VERTEX_BUFFER].AsBuffer()};

    m_pImmediateContext->SetRenderTargets(0, nullptr, m_pSwapChain->GetDepthBufferDSV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->SetPipelineState(pPSO);
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffers, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->SetIndexBuffer(m_Resources[RESOURCE_IDENTIFIER_OBJECT_AABB_INDEX_BUFFER].AsBuffer(), 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    for (Uint32 ObjectIdx = 0; ObjectIdx < m_ObjectCount; ObjectIdx++)
    {
        // Skip mesh objects — they are rendered by GenerateCSGMesh with their own VB/IB
        if (ObjectIdx >= m_MeshObjectStart && ObjectIdx < m_MeshObjectStart + m_MeshObjectCount)
            continue;

        ObjectAttribVariable.SetBufferRange(m_Resources[RESOURCE_IDENTIFIER_OBJECT_ATTRIBS_CONSTANT_BUFFER].AsBuffer(), ObjectIdx * sizeof(HLSL::ObjectAttribs), sizeof(HLSL::ObjectAttribs));
        m_pImmediateContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->DrawIndexed({36, VT_UINT32, DRAW_FLAG_VERIFY_ALL});
    }
}

void ConstructiveSolidGeometry::GenerateCSGMesh()
{
    auto [pPSO, pSRB] = m_PSOManager->RequestPSO(PSO_IDENTIFIER_GENERATE_CSG_MESH);
    if (!pPSO || !pSRB) return;

    ShaderResourceVariableX{pSRB, SHADER_TYPE_PIXEL, "cbCameraAttribs"}.Set(m_Resources[RESOURCE_IDENTIFIER_CAMERA_CONSTANT_BUFFER].AsBuffer());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_PIXEL, "cbABufferConstants"}.Set(m_Resources[RESOURCE_IDENTIFIER_ABUFFER_CONSTANTS_BUFFER].AsBuffer());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_PIXEL, "g_BufferHeadPointers"}.Set(m_Resources[RESOURCE_IDENTIFIER_ABUFFER_HEAD_POINTER_BUFFER].GetBufferUAV());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_PIXEL, "g_BufferNodes"}.Set(m_Resources[RESOURCE_IDENTIFIER_ABUFFER_NODE_BUFFER].GetBufferUAV());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_PIXEL, "g_BufferCounter"}.Set(m_Resources[RESOURCE_IDENTIFIER_ABUFFER_COUNTER_BUFFER].GetBufferUAV());
    ShaderResourceVariableX ObjectAttribVariable{pSRB, SHADER_TYPE_PIXEL, "cbObjectAttribs"};
    ObjectAttribVariable.Set(m_Resources[RESOURCE_IDENTIFIER_OBJECT_ATTRIBS_CONSTANT_BUFFER].AsBuffer());

    ScopedDebugGroup DebugGroup{m_pImmediateContext, "GenerateCSGMesh"};

    Uint64   Offsets[]  = {0};
    IBuffer* pBuffers[] = {m_Resources[RESOURCE_IDENTIFIER_MESH_VERTEX_BUFFER].AsBuffer()};

    m_pImmediateContext->SetPipelineState(pPSO);
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffers, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->SetIndexBuffer(m_Resources[RESOURCE_IDENTIFIER_MESH_INDEX_BUFFER].AsBuffer(), 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    for (Uint32 ObjectIdx = m_MeshObjectStart; ObjectIdx < m_MeshObjectStart + m_MeshObjectCount; ObjectIdx++)
    {
        ObjectAttribVariable.SetBufferRange(m_Resources[RESOURCE_IDENTIFIER_OBJECT_ATTRIBS_CONSTANT_BUFFER].AsBuffer(), ObjectIdx * sizeof(HLSL::ObjectAttribs), sizeof(HLSL::ObjectAttribs));
        m_pImmediateContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->DrawIndexed({m_MeshIndexCount, VT_UINT32, DRAW_FLAG_VERIFY_ALL});
    }
    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void ConstructiveSolidGeometry::ResolveCSG()
{
    auto [pPSO, pSRB] = m_PSOManager->RequestPSO(PSO_IDENTIFIER_RESOLVE_CSG);
    if (!pPSO || !pSRB) return;

    ShaderResourceVariableX{pSRB, SHADER_TYPE_COMPUTE, "cbABufferConstants"}.Set(m_Resources[RESOURCE_IDENTIFIER_ABUFFER_CONSTANTS_BUFFER].AsBuffer());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_COMPUTE, "cbCameraAttribs"}.Set(m_Resources[RESOURCE_IDENTIFIER_CAMERA_CONSTANT_BUFFER].AsBuffer());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_COMPUTE, "cbPBRRendererAttibs"}.Set(m_Resources[RESOURCE_IDENTIFIER_PBR_ATTRIBS_CONSTANT_BUFFER].AsBuffer());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_COMPUTE, "cbMaterialAttribs"}.Set(m_Resources[RESOURCE_IDENTIFIER_MATERIAL_ATTRIBS_CONSTANT_BUFFER].AsBuffer());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_COMPUTE, "g_BufferHeadPointers"}.Set(m_Resources[RESOURCE_IDENTIFIER_ABUFFER_HEAD_POINTER_BUFFER].GetBufferSRV());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_COMPUTE, "g_NodeBuffer"}.Set(m_Resources[RESOURCE_IDENTIFIER_ABUFFER_NODE_BUFFER].GetBufferSRV());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_COMPUTE, "g_CSGOperations"}.Set(m_Resources[RESOURCE_IDENTIFIER_CSG_OPERATIONS_BUFFER].GetBufferSRV());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_COMPUTE, "g_TexturePrefilteredEnvironmentMap"}.Set(m_Resources[RESOURCE_IDENTIFIER_PREFILTERED_ENVIRONMENT_MAP].GetTextureSRV());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_COMPUTE, "g_TextureBRDFIntegrationMap"}.Set(m_Resources[RESOURCE_IDENTIFIER_BRDF_INTEGRATION_MAP].GetTextureSRV());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_COMPUTE, "g_TextureRadiance"}.Set(m_Resources[RESOURCE_IDENTIFIER_RADIANCE].GetTextureUAV());

    ScopedDebugGroup DebugGroup{m_pImmediateContext, "ResolveCSG"};

    m_pImmediateContext->SetPipelineState(pPSO);
    m_pImmediateContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DispatchComputeAttribs DispatchAttrs{DivCeil(m_pSwapChain->GetDesc().Width, 8), DivCeil(m_pSwapChain->GetDesc().Height, 8), 1};
    m_pImmediateContext->DispatchCompute(DispatchAttrs);
}

void ConstructiveSolidGeometry::ComputeToneMapping()
{
    auto [pPSO, pSRB] = m_PSOManager->RequestPSO(PSO_IDENTIFIER_TONE_MAPPING);
    if (!pPSO || !pSRB) return;

    ShaderResourceVariableX{pSRB, SHADER_TYPE_PIXEL, "cbPBRRendererAttibs"}.Set(m_Resources[RESOURCE_IDENTIFIER_PBR_ATTRIBS_CONSTANT_BUFFER].AsBuffer());
    ShaderResourceVariableX{pSRB, SHADER_TYPE_PIXEL, "g_TextureHDR"}.Set(m_Resources[RESOURCE_IDENTIFIER_RADIANCE].GetTextureSRV());

    ScopedDebugGroup DebugGroup{m_pImmediateContext, "ComputeToneMapping"};

    ITextureView* pRTV = m_Resources[RESOURCE_IDENTIFIER_TONE_MAPPING].GetTextureRTV();
    m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->SetPipelineState(pPSO);
    m_pImmediateContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

void ConstructiveSolidGeometry::ComputeGammaCorrection()
{
    const bool ConvertOutputToGamma = (m_pSwapChain->GetDesc().ColorBufferFormat == TEX_FORMAT_RGBA8_UNORM ||
                                       m_pSwapChain->GetDesc().ColorBufferFormat == TEX_FORMAT_BGRA8_UNORM);

    ITextureView* pSRV = m_Resources[RESOURCE_IDENTIFIER_TONE_MAPPING].GetTextureSRV();
    ITextureView* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();

    if (ConvertOutputToGamma)
    {
        auto [pPSO, pSRB] = m_PSOManager->RequestPSO(PSO_IDENTIFIER_GAMMA_CORRECTION);
        if (!pPSO || !pSRB) return;

        ShaderResourceVariableX{pSRB, SHADER_TYPE_PIXEL, "g_TextureColor"}.Set(pSRV);

        ScopedDebugGroup DebugGroup{m_pImmediateContext, "GammaCorrection"};

        m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->SetPipelineState(pPSO);
        m_pImmediateContext->CommitShaderResources(pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
        m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    else
    {
        // If the swap chain format is already sRGB, just copy the tone-mapped result
        CopyTextureAttribs CopyAttribs;
        CopyAttribs.pSrcTexture              = m_Resources[RESOURCE_IDENTIFIER_TONE_MAPPING].AsTexture();
        CopyAttribs.pDstTexture              = m_pSwapChain->GetCurrentBackBufferRTV()->GetTexture();
        CopyAttribs.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        CopyAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        m_pImmediateContext->CopyTexture(CopyAttribs);
    }
}

void ConstructiveSolidGeometry::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysUseWindowPadding | ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("FPS: %f", m_fSmoothFPS);

        if (ImGui::TreeNode("Rendering"))
        {
            ImGui::Checkbox("Enable Animation", &m_IsAnimationActive);
            ImGui::SliderFloat("Plane Height", &m_PlaneHeight, -1.0f, 1.0f);

            const char* CSGOpNames[] = {"Union", "Subtraction", "Intersection", "Symmetric Difference"};
            ImGui::Combo("Plane CSG Op", &m_PlaneCSGOp, CSGOpNames, _countof(CSGOpNames));
            ImGui::Checkbox("Plane is Primary", &m_PlaneIsPrimary);

            ImGui::TreePop();
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::TreeNode("Tone Mapping"))
        {
            ImGui::SliderFloat("Average log lum", &m_AverageLogLum, 0.01f, 10.0f);
            ImGui::SliderFloat("Middle gray", &m_MiddleGray, 0.01f, 1.0f);
            ImGui::SliderFloat("White point", &m_WhitePoint, 0.1f, 20.0f);
            ImGui::TreePop();
        }
    }
    ImGui::End();
}

void ConstructiveSolidGeometry::LoadMesh(const char* FileName)
{
    STLMesh Mesh;
    if (!LoadSTL(FileName, Mesh))
    {
        LOG_ERROR_MESSAGE("Failed to load STL mesh: ", FileName);
        return;
    }

    m_MeshIndexCount = static_cast<Uint32>(Mesh.Indices.size());

    LOG_INFO_MESSAGE("STL Mesh loaded: ", Mesh.Vertices.size(), " vertices, ", Mesh.Indices.size(), " indices");
    LOG_INFO_MESSAGE("STL BBox Min: (", Mesh.BBoxMin.x, ", ", Mesh.BBoxMin.y, ", ", Mesh.BBoxMin.z, ")");
    LOG_INFO_MESSAGE("STL BBox Max: (", Mesh.BBoxMax.x, ", ", Mesh.BBoxMax.y, ", ", Mesh.BBoxMax.z, ")");

    BufferDesc VBDesc;
    VBDesc.Name      = "ConstructiveSolidGeometry::MeshVertexBuffer";
    VBDesc.Size      = static_cast<Uint64>(Mesh.Vertices.size() * sizeof(STLVertex));
    VBDesc.BindFlags = BIND_VERTEX_BUFFER;
    VBDesc.Usage     = USAGE_IMMUTABLE;

    BufferData VBData;
    VBData.pData    = Mesh.Vertices.data();
    VBData.DataSize = VBDesc.Size;

    RefCntAutoPtr<IBuffer> pVB;
    m_pDevice->CreateBuffer(VBDesc, &VBData, &pVB);
    m_Resources.Insert(RESOURCE_IDENTIFIER_MESH_VERTEX_BUFFER, pVB);

    BufferDesc IBDesc;
    IBDesc.Name      = "ConstructiveSolidGeometry::MeshIndexBuffer";
    IBDesc.Size      = static_cast<Uint64>(Mesh.Indices.size() * sizeof(uint32_t));
    IBDesc.BindFlags = BIND_INDEX_BUFFER;
    IBDesc.Usage     = USAGE_IMMUTABLE;

    BufferData IBData;
    IBData.pData    = Mesh.Indices.data();
    IBData.DataSize = IBDesc.Size;

    RefCntAutoPtr<IBuffer> pIB;
    m_pDevice->CreateBuffer(IBDesc, &IBData, &pIB);
    m_Resources.Insert(RESOURCE_IDENTIFIER_MESH_INDEX_BUFFER, pIB);
}

void ConstructiveSolidGeometry::LoadEnvironmentMap(const char* FileName)
{
    // We only need PBR renderer to precompute environment maps
    if (!m_IBLBacker)
        m_IBLBacker = std::make_unique<PBR_Renderer>(m_pDevice, nullptr, m_pImmediateContext, PBR_Renderer::CreateInfo{});

    for (Uint32 TextureIdx = RESOURCE_IDENTIFIER_PREFILTERED_ENVIRONMENT_MAP; TextureIdx <= RESOURCE_IDENTIFIER_BRDF_INTEGRATION_MAP; TextureIdx++)
        m_Resources[TextureIdx].Release();

    RefCntAutoPtr<ITexture> pEnvironmentMap;
    CreateTextureFromFile(FileName, TextureLoadInfo{"ConstructiveSolidGeometry::EnvironmentMap"}, m_pDevice, &pEnvironmentMap);
    DEV_CHECK_ERR(pEnvironmentMap, "Failed to load environment map");
    m_IBLBacker->PrecomputeCubemaps(m_pImmediateContext, pEnvironmentMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

    m_Resources.Insert(RESOURCE_IDENTIFIER_PREFILTERED_ENVIRONMENT_MAP, m_IBLBacker->GetPrefilteredEnvMapSRV()->GetTexture());
    m_Resources.Insert(RESOURCE_IDENTIFIER_BRDF_INTEGRATION_MAP, m_IBLBacker->GetPreintegratedGGX_SRV()->GetTexture());
}

void ConstructiveSolidGeometry::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);

    Attribs.EngineCI.Features.ComputeShaders           = DEVICE_FEATURE_STATE_ENABLED;
    Attribs.EngineCI.Features.PixelUAVWritesAndAtomics = DEVICE_FEATURE_STATE_ENABLED;

    SwapChainDesc* pSCDesc     = static_cast<SwapChainDesc*>(&Attribs.SCDesc);
    pSCDesc->DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
    pSCDesc->ColorBufferFormat = SRGBFormatToUnorm(pSCDesc->ColorBufferFormat);
}

} // namespace Diligent
