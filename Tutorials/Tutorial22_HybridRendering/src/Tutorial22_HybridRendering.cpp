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

#include "Tutorial22_HybridRendering.hpp"

#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "ShaderMacroHelper.hpp"
#include "imgui.h"
#include "ImGuiUtils.hpp"
#include "imGuIZMO.h"
#include "Align.hpp"
#include "../../Common/src/TexturedCube.hpp"

namespace Diligent
{

static_assert(sizeof(HLSL::GlobalConstants) % 16 == 0, "Structure must be 16-byte aligned");
static_assert(sizeof(HLSL::ObjectConstants) % 16 == 0, "Structure must be 16-byte aligned");

SampleBase* CreateSample()
{
    return new Tutorial22_HybridRendering();
}

void Tutorial22_HybridRendering::CreateSceneMaterials(uint2& CubeMaterialRange, Uint32& GroundMaterial, std::vector<HLSL::MaterialAttribs>& Materials)
{
    Uint32 AnisotropicClampSampInd = 0;
    Uint32 AnisotropicWrapSampInd  = 0;

    // Create samplers
    {
        const SamplerDesc AnisotropicClampSampler{
            FILTER_TYPE_ANISOTROPIC, FILTER_TYPE_ANISOTROPIC, FILTER_TYPE_ANISOTROPIC,
            TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, 0.f, 8 //
        };
        const SamplerDesc AnisotropicWrapSampler{
            FILTER_TYPE_ANISOTROPIC, FILTER_TYPE_ANISOTROPIC, FILTER_TYPE_ANISOTROPIC,
            TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, 0.f, 8 //
        };

        RefCntAutoPtr<ISampler> pSampler;
        m_pDevice->CreateSampler(AnisotropicClampSampler, &pSampler);
        AnisotropicClampSampInd = static_cast<Uint32>(m_Scene.Samplers.size());
        m_Scene.Samplers.push_back(std::move(pSampler));

        pSampler = nullptr;
        m_pDevice->CreateSampler(AnisotropicWrapSampler, &pSampler);
        AnisotropicWrapSampInd = static_cast<Uint32>(m_Scene.Samplers.size());
        m_Scene.Samplers.push_back(std::move(pSampler));
    }

    const auto LoadMaterial = [&](const char* ColorMapName, const float4& BaseColor, Uint32 SamplerInd) //
    {
        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB       = true;
        loadInfo.GenerateMips = true;
        RefCntAutoPtr<ITexture> Tex;
        CreateTextureFromFile(ColorMapName, loadInfo, m_pDevice, &Tex);
        VERIFY_EXPR(Tex);

        HLSL::MaterialAttribs mtr;
        mtr.SampInd         = SamplerInd;
        mtr.BaseColorMask   = BaseColor;
        mtr.BaseColorTexInd = static_cast<Uint32>(m_Scene.Textures.size());
        m_Scene.Textures.push_back(std::move(Tex));
        Materials.push_back(mtr);
    };

    // Cube materials
    CubeMaterialRange.x = static_cast<Uint32>(Materials.size());
    LoadMaterial("DGLogo0.png", float4{1.f}, AnisotropicClampSampInd);
    LoadMaterial("DGLogo1.png", float4{1.f}, AnisotropicClampSampInd);
    LoadMaterial("DGLogo2.png", float4{1.f}, AnisotropicClampSampInd);
    LoadMaterial("DGLogo3.png", float4{1.f}, AnisotropicClampSampInd);
    CubeMaterialRange.y = static_cast<Uint32>(Materials.size());

    // Ground material
    GroundMaterial = static_cast<Uint32>(Materials.size());
    LoadMaterial("Marble.jpg", float4{1.f}, AnisotropicWrapSampInd);
}

Tutorial22_HybridRendering::Mesh Tutorial22_HybridRendering::CreateTexturedPlaneMesh(IRenderDevice* pDevice, float2 UVScale)
{
    Mesh PlaneMesh;
    PlaneMesh.Name = "Ground";

    {
        struct PlaneVertex // Alias for HLSL::Vertex
        {
            float3 pos;
            float3 norm;
            float2 uv;
        };
        static_assert(sizeof(PlaneVertex) == sizeof(HLSL::Vertex), "Vertex size mismatch");

        // clang-format off
        const PlaneVertex Vertices[] = 
        {
            {float3{-1, 0, -1}, float3{0, 1, 0}, float2{0,         0        }},
            {float3{ 1, 0, -1}, float3{0, 1, 0}, float2{UVScale.x, 0        }},
            {float3{-1, 0,  1}, float3{0, 1, 0}, float2{0,         UVScale.y}},
            {float3{ 1, 0,  1}, float3{0, 1, 0}, float2{UVScale.x, UVScale.y}}
        };
        // clang-format on
        PlaneMesh.NumVertices = _countof(Vertices);

        BufferDesc VBDesc;
        VBDesc.Name              = "Plane vertex buffer";
        VBDesc.Usage             = USAGE_IMMUTABLE;
        VBDesc.BindFlags         = BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE | BIND_RAY_TRACING;
        VBDesc.Size              = sizeof(Vertices);
        VBDesc.Mode              = BUFFER_MODE_STRUCTURED;
        VBDesc.ElementByteStride = sizeof(Vertices[0]);
        BufferData VBData{Vertices, VBDesc.Size};
        pDevice->CreateBuffer(VBDesc, &VBData, &PlaneMesh.VertexBuffer);
    }

    {
        const Uint32 Indices[] = {0, 2, 3, 3, 1, 0};
        PlaneMesh.NumIndices   = _countof(Indices);

        BufferDesc IBDesc;
        IBDesc.Name              = "Plane index buffer";
        IBDesc.BindFlags         = BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE | BIND_RAY_TRACING;
        IBDesc.Size              = sizeof(Indices);
        IBDesc.Mode              = BUFFER_MODE_STRUCTURED;
        IBDesc.ElementByteStride = sizeof(Indices[0]);
        BufferData IBData{Indices, IBDesc.Size};
        pDevice->CreateBuffer(IBDesc, &IBData, &PlaneMesh.IndexBuffer);
    }

    return PlaneMesh;
}

void Tutorial22_HybridRendering::CreateSceneObjects(const uint2 CubeMaterialRange, const Uint32 GroundMaterial)
{
    Uint32 CubeMeshId  = 0;
    Uint32 PlaneMeshId = 0;

    // Create meshes
    {
        Mesh CubeMesh;
        CubeMesh.Name         = "Cube";
        CubeMesh.VertexBuffer = TexturedCube::CreateVertexBuffer(
            m_pDevice,
            TexturedCube::VERTEX_COMPONENT_FLAG_POS_NORM_UV,
            BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE | BIND_RAY_TRACING,
            BUFFER_MODE_STRUCTURED);
        CubeMesh.IndexBuffer = TexturedCube::CreateIndexBuffer(m_pDevice, BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE | BIND_RAY_TRACING, BUFFER_MODE_STRUCTURED);
        CubeMesh.NumVertices = TexturedCube::NumVertices;
        CubeMesh.NumIndices  = TexturedCube::NumIndices;

        auto PlaneMesh = CreateTexturedPlaneMesh(m_pDevice, float2{25});

        const auto RTProps = m_pDevice->GetAdapterInfo().RayTracing;

        // Cube mesh will be copied to the beginning of the buffers
        CubeMesh.FirstVertex = 0;
        CubeMesh.FirstIndex  = 0;
        // Plane mesh data will reside after the cube. Offsets must be properly aligned!
        PlaneMesh.FirstVertex = AlignUp(CubeMesh.NumVertices * Uint32{sizeof(HLSL::Vertex)}, RTProps.VertexBufferAlignment) / sizeof(HLSL::Vertex);
        PlaneMesh.FirstIndex  = AlignUp(CubeMesh.NumIndices * Uint32{sizeof(uint)}, RTProps.IndexBufferAlignment) / sizeof(uint);

        // Merge vertex buffers
        {
            BufferDesc VBDesc;
            VBDesc.Name              = "Shared vertex buffer";
            VBDesc.BindFlags         = BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE | BIND_RAY_TRACING;
            VBDesc.Size              = (Uint64{PlaneMesh.FirstVertex} + Uint64{PlaneMesh.NumVertices}) * sizeof(HLSL::Vertex);
            VBDesc.Mode              = BUFFER_MODE_STRUCTURED;
            VBDesc.ElementByteStride = sizeof(HLSL::Vertex);

            RefCntAutoPtr<IBuffer> pSharedVB;
            m_pDevice->CreateBuffer(VBDesc, nullptr, &pSharedVB);

            // Copy cube vertices
            m_pImmediateContext->CopyBuffer(CubeMesh.VertexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                            pSharedVB, CubeMesh.FirstVertex * sizeof(HLSL::Vertex), CubeMesh.NumVertices * sizeof(HLSL::Vertex),
                                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // Copy plane vertices
            m_pImmediateContext->CopyBuffer(PlaneMesh.VertexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                            pSharedVB, PlaneMesh.FirstVertex * sizeof(HLSL::Vertex), PlaneMesh.NumVertices * sizeof(HLSL::Vertex),
                                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            CubeMesh.VertexBuffer  = pSharedVB;
            PlaneMesh.VertexBuffer = pSharedVB;
        }

        // Merge index buffers
        {
            BufferDesc IBDesc;
            IBDesc.Name              = "Shared index buffer";
            IBDesc.BindFlags         = BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE | BIND_RAY_TRACING;
            IBDesc.Size              = (Uint64{PlaneMesh.FirstIndex} + Uint64{PlaneMesh.NumIndices}) * sizeof(uint);
            IBDesc.Mode              = BUFFER_MODE_STRUCTURED;
            IBDesc.ElementByteStride = sizeof(uint);

            RefCntAutoPtr<IBuffer> pSharedIB;
            m_pDevice->CreateBuffer(IBDesc, nullptr, &pSharedIB);

            // Copy cube indices
            m_pImmediateContext->CopyBuffer(CubeMesh.IndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                            pSharedIB, CubeMesh.FirstIndex * sizeof(uint), CubeMesh.NumIndices * sizeof(uint),
                                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            // Copy plane indices
            m_pImmediateContext->CopyBuffer(PlaneMesh.IndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                            pSharedIB, PlaneMesh.FirstIndex * sizeof(uint), PlaneMesh.NumIndices * sizeof(uint),
                                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            CubeMesh.IndexBuffer  = pSharedIB;
            PlaneMesh.IndexBuffer = pSharedIB;
        }

        CubeMeshId = static_cast<Uint32>(m_Scene.Meshes.size());
        m_Scene.Meshes.push_back(CubeMesh);
        PlaneMeshId = static_cast<Uint32>(m_Scene.Meshes.size());
        m_Scene.Meshes.push_back(PlaneMesh);
    }

    // Create cube objects
    const auto AddCubeObject = [&](float Angle, float X, float Y, float Z, float Scale, bool IsDynamic = false) //
    {
        const auto ModelMat = float4x4::RotationY(Angle * PI_F) * float4x4::Scale(Scale) * float4x4::Translation(X * 2.0f, Y * 2.0f - 1.0f, Z * 2.0f);

        HLSL::ObjectAttribs obj;
        obj.ModelMat    = ModelMat.Transpose();
        obj.NormalMat   = obj.ModelMat;
        obj.MaterialId  = (m_Scene.Objects.size() % (CubeMaterialRange.y - CubeMaterialRange.x)) + CubeMaterialRange.x;
        obj.MeshId      = CubeMeshId;
        obj.FirstIndex  = m_Scene.Meshes[obj.MeshId].FirstIndex;
        obj.FirstVertex = m_Scene.Meshes[obj.MeshId].FirstVertex;
        m_Scene.Objects.push_back(obj);

        if (IsDynamic)
        {
            DynamicObject DynObj;
            DynObj.ObjectAttribsIndex = static_cast<Uint32>(m_Scene.Objects.size() - 1);
            m_Scene.DynamicObjects.push_back(DynObj);
        }
    };
    // clang-format off
    AddCubeObject(0.25f,  0.0f, 1.00f,  1.5f, 0.9f);
    AddCubeObject(0.00f, -1.9f, 1.00f, -0.5f, 0.5f);
    AddCubeObject(0.00f, -1.0f, 1.00f,  0.0f, 1.0f);
    AddCubeObject(0.30f, -0.2f, 1.00f, -1.0f, 0.7f);
    AddCubeObject(0.25f, -1.7f, 1.00f, -1.6f, 1.1f, true);
    AddCubeObject(0.28f,  0.7f, 1.00f,  3.0f, 1.3f);
    AddCubeObject(0.10f,  1.5f, 1.00f,  1.0f, 1.1f);
    AddCubeObject(0.21f, -3.2f, 1.00f,  0.2f, 1.2f);
    AddCubeObject(0.05f, -2.1f, 1.00f,  1.6f, 1.1f);
    
    AddCubeObject(0.04f, -1.4f, 2.18f, -1.4f, 0.6f);
    AddCubeObject(0.24f, -1.0f, 2.10f,  0.5f, 1.1f, true);
    AddCubeObject(0.02f, -0.5f, 2.00f, -0.9f, 0.9f);
    AddCubeObject(0.08f, -1.7f, 1.96f,  1.7f, 0.7f);
    AddCubeObject(0.17f,  1.5f, 2.00f,  1.1f, 0.9f);
    
    AddCubeObject(0.6f, -1.0f, 3.25f, -0.2f, 1.2f);
    // clang-format on

    InstancedObjects InstObj;
    InstObj.MeshInd             = CubeMeshId;
    InstObj.NumObjects          = static_cast<Uint32>(m_Scene.Objects.size());
    InstObj.ObjectAttribsOffset = 0;
    m_Scene.ObjectInstances.push_back(InstObj);

    // Create ground plane object
    InstObj.ObjectAttribsOffset = static_cast<Uint32>(m_Scene.Objects.size());
    InstObj.MeshInd             = PlaneMeshId;
    {
        HLSL::ObjectAttribs obj;
        obj.ModelMat    = (float4x4::Scale(50.f, 1.f, 50.f) * float4x4::Translation(0.f, -0.2f, 0.f)).Transpose();
        obj.NormalMat   = float3x3::Identity();
        obj.MaterialId  = GroundMaterial;
        obj.MeshId      = PlaneMeshId;
        obj.FirstIndex  = m_Scene.Meshes[obj.MeshId].FirstIndex;
        obj.FirstVertex = m_Scene.Meshes[obj.MeshId].FirstVertex;
        m_Scene.Objects.push_back(obj);
    }
    InstObj.NumObjects = static_cast<Uint32>(m_Scene.Objects.size()) - InstObj.ObjectAttribsOffset;
    m_Scene.ObjectInstances.push_back(InstObj);
}

void Tutorial22_HybridRendering::CreateSceneAccelStructs()
{
    // Create and build bottom-level acceleration structure
    {
        RefCntAutoPtr<IBuffer> pScratchBuffer;

        for (auto& Mesh : m_Scene.Meshes)
        {
            // Create BLAS
            BLASTriangleDesc Triangles;
            {
                Triangles.GeometryName         = Mesh.Name.c_str();
                Triangles.MaxVertexCount       = Mesh.NumVertices;
                Triangles.VertexValueType      = VT_FLOAT32;
                Triangles.VertexComponentCount = 3;
                Triangles.MaxPrimitiveCount    = Mesh.NumIndices / 3;
                Triangles.IndexType            = VT_UINT32;

                const auto BLASName{Mesh.Name + " BLAS"};

                BottomLevelASDesc ASDesc;
                ASDesc.Name          = BLASName.c_str();
                ASDesc.Flags         = RAYTRACING_BUILD_AS_PREFER_FAST_TRACE;
                ASDesc.pTriangles    = &Triangles;
                ASDesc.TriangleCount = 1;
                m_pDevice->CreateBLAS(ASDesc, &Mesh.BLAS);
            }

            // Create or reuse scratch buffer; this will insert the barrier between BuildBLAS invocations, which may be suboptimal.
            if (!pScratchBuffer || pScratchBuffer->GetDesc().Size < Mesh.BLAS->GetScratchBufferSizes().Build)
            {
                BufferDesc BuffDesc;
                BuffDesc.Name      = "BLAS Scratch Buffer";
                BuffDesc.Usage     = USAGE_DEFAULT;
                BuffDesc.BindFlags = BIND_RAY_TRACING;
                BuffDesc.Size      = Mesh.BLAS->GetScratchBufferSizes().Build;

                pScratchBuffer = nullptr;
                m_pDevice->CreateBuffer(BuffDesc, nullptr, &pScratchBuffer);
            }

            // Build BLAS
            BLASBuildTriangleData TriangleData;
            TriangleData.GeometryName         = Triangles.GeometryName;
            TriangleData.pVertexBuffer        = Mesh.VertexBuffer;
            TriangleData.VertexStride         = Mesh.VertexBuffer->GetDesc().ElementByteStride;
            TriangleData.VertexOffset         = Uint64{Mesh.FirstVertex} * Uint64{TriangleData.VertexStride};
            TriangleData.VertexCount          = Mesh.NumVertices;
            TriangleData.VertexValueType      = Triangles.VertexValueType;
            TriangleData.VertexComponentCount = Triangles.VertexComponentCount;
            TriangleData.pIndexBuffer         = Mesh.IndexBuffer;
            TriangleData.IndexOffset          = Uint64{Mesh.FirstIndex} * Uint64{Mesh.IndexBuffer->GetDesc().ElementByteStride};
            TriangleData.PrimitiveCount       = Triangles.MaxPrimitiveCount;
            TriangleData.IndexType            = Triangles.IndexType;
            TriangleData.Flags                = RAYTRACING_GEOMETRY_FLAG_OPAQUE;

            BuildBLASAttribs Attribs;
            Attribs.pBLAS             = Mesh.BLAS;
            Attribs.pTriangleData     = &TriangleData;
            Attribs.TriangleDataCount = 1;

            // Scratch buffer will be used to store temporary data during the BLAS build.
            // Previous content in the scratch buffer will be discarded.
            Attribs.pScratchBuffer = pScratchBuffer;

            // Allow engine to change resource states.
            Attribs.BLASTransitionMode          = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            Attribs.GeometryTransitionMode      = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            Attribs.ScratchBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

            m_pImmediateContext->BuildBLAS(Attribs);
        }
    }

    // Create TLAS
    {
        TopLevelASDesc TLASDesc;
        TLASDesc.Name             = "Scene TLAS";
        TLASDesc.MaxInstanceCount = static_cast<Uint32>(m_Scene.Objects.size());
        TLASDesc.Flags            = RAYTRACING_BUILD_AS_ALLOW_UPDATE | RAYTRACING_BUILD_AS_PREFER_FAST_TRACE;
        m_pDevice->CreateTLAS(TLASDesc, &m_Scene.TLAS);
    }
}

void Tutorial22_HybridRendering::UpdateTLAS()
{
    const Uint32 NumInstances = static_cast<Uint32>(m_Scene.Objects.size());
    bool         Update       = true;

    // Create scratch buffer
    if (!m_Scene.TLASScratchBuffer)
    {
        BufferDesc BuffDesc;
        BuffDesc.Name      = "TLAS Scratch Buffer";
        BuffDesc.Usage     = USAGE_DEFAULT;
        BuffDesc.BindFlags = BIND_RAY_TRACING;
        BuffDesc.Size      = std::max(m_Scene.TLAS->GetScratchBufferSizes().Build, m_Scene.TLAS->GetScratchBufferSizes().Update);
        m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_Scene.TLASScratchBuffer);
        Update = false; // this is the first build
    }

    // Create instance buffer
    if (!m_Scene.TLASInstancesBuffer)
    {
        BufferDesc BuffDesc;
        BuffDesc.Name      = "TLAS Instance Buffer";
        BuffDesc.Usage     = USAGE_DEFAULT;
        BuffDesc.BindFlags = BIND_RAY_TRACING;
        BuffDesc.Size      = Uint64{TLAS_INSTANCE_DATA_SIZE} * Uint64{NumInstances};
        m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_Scene.TLASInstancesBuffer);
    }

    // Setup instances
    std::vector<TLASBuildInstanceData> Instances(NumInstances);
    std::vector<String>                InstanceNames(NumInstances);
    for (Uint32 i = 0; i < NumInstances; ++i)
    {
        const auto& Obj      = m_Scene.Objects[i];
        auto&       Inst     = Instances[i];
        auto&       Name     = InstanceNames[i];
        const auto& Mesh     = m_Scene.Meshes[Obj.MeshId];
        const auto  ModelMat = Obj.ModelMat.Transpose();

        Name = Mesh.Name + " Instance (" + std::to_string(i) + ")";

        Inst.InstanceName = Name.c_str();
        Inst.pBLAS        = Mesh.BLAS;
        Inst.Mask         = 0xFF;

        // CustomId will be read in shader by RayQuery::CommittedInstanceID()
        Inst.CustomId = i;

        Inst.Transform.SetRotation(ModelMat.Data(), 4);
        Inst.Transform.SetTranslation(ModelMat.m30, ModelMat.m31, ModelMat.m32);
    }

    // Build  TLAS
    BuildTLASAttribs Attribs;
    Attribs.pTLAS  = m_Scene.TLAS;
    Attribs.Update = Update;

    // Scratch buffer will be used to store temporary data during TLAS build or update.
    // Previous content in the scratch buffer will be discarded.
    Attribs.pScratchBuffer = m_Scene.TLASScratchBuffer;

    // Instance buffer will store instance data during TLAS build or update.
    // Previous content in the instance buffer will be discarded.
    Attribs.pInstanceBuffer = m_Scene.TLASInstancesBuffer;

    // Instances will be converted to the format that is required by the graphics driver and copied to the instance buffer.
    Attribs.pInstances    = Instances.data();
    Attribs.InstanceCount = NumInstances;

    // Allow engine to change resource states.
    Attribs.TLASTransitionMode           = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.BLASTransitionMode           = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.InstanceBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
    Attribs.ScratchBufferTransitionMode  = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

    m_pImmediateContext->BuildTLAS(Attribs);
}

void Tutorial22_HybridRendering::CreateScene()
{
    uint2                              CubeMaterialRange;
    Uint32                             GroundMaterial;
    std::vector<HLSL::MaterialAttribs> Materials;
    CreateSceneMaterials(CubeMaterialRange, GroundMaterial, Materials);
    CreateSceneObjects(CubeMaterialRange, GroundMaterial);
    CreateSceneAccelStructs();

    // Create buffer for object attribs
    {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Object attribs buffer";
        BuffDesc.Usage             = USAGE_DEFAULT;
        BuffDesc.BindFlags         = BIND_SHADER_RESOURCE;
        BuffDesc.Size              = static_cast<Uint64>(sizeof(m_Scene.Objects[0]) * m_Scene.Objects.size());
        BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
        BuffDesc.ElementByteStride = sizeof(m_Scene.Objects[0]);
        m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_Scene.ObjectAttribsBuffer);
    }

    // Create and initialize buffer for material attribs
    {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Material attribs buffer";
        BuffDesc.Usage             = USAGE_DEFAULT;
        BuffDesc.BindFlags         = BIND_SHADER_RESOURCE;
        BuffDesc.Size              = static_cast<Uint64>(sizeof(Materials[0]) * Materials.size());
        BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
        BuffDesc.ElementByteStride = sizeof(Materials[0]);

        BufferData BuffData{Materials.data(), BuffDesc.Size};
        m_pDevice->CreateBuffer(BuffDesc, &BuffData, &m_Scene.MaterialAttribsBuffer);
    }

    // Create dynamic buffer for scene object constants (unique for each draw call)
    {
        BufferDesc BuffDesc;
        BuffDesc.Name           = "Global constants buffer";
        BuffDesc.Usage          = USAGE_DYNAMIC;
        BuffDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        BuffDesc.Size           = sizeof(HLSL::ObjectConstants);
        BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_Scene.ObjectConstants);
    }
}

void Tutorial22_HybridRendering::CreateRasterizationPSO(IShaderSourceInputStreamFactory* pShaderSourceFactory)
{
    // Create PSO for rendering to GBuffer

    ShaderMacroHelper Macros;
    Macros.AddShaderMacro("NUM_TEXTURES", static_cast<Uint32>(m_Scene.Textures.size()));
    Macros.AddShaderMacro("NUM_SAMPLERS", static_cast<Uint32>(m_Scene.Samplers.size()));

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name         = "Rasterization PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 2;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_ColorTargetFormat;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[1]                = m_NormalTargetFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_DepthTargetFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler             = m_ShaderCompiler;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.Macros                     = Macros;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Rasterization VS";
        ShaderCI.FilePath        = "Rasterization.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Rasterization PS";
        ShaderCI.FilePath        = "Rasterization.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    LayoutElement LayoutElems[] =
        {
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            LayoutElement{1, 0, 3, VT_FLOAT32, False},
            LayoutElement{2, 0, 2, VT_FLOAT32, False} //
        };
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType        = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableMergeStages = SHADER_TYPE_VERTEX | SHADER_TYPE_PIXEL;

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_RasterizationPSO);

    m_RasterizationPSO->CreateShaderResourceBinding(&m_RasterizationSRB);
    m_RasterizationSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_Constants")->Set(m_Constants);
    m_RasterizationSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_ObjectConst")->Set(m_Scene.ObjectConstants);
    m_RasterizationSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_ObjectAttribs")->Set(m_Scene.ObjectAttribsBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_RasterizationSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_MaterialAttribs")->Set(m_Scene.MaterialAttribsBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

    // Bind textures
    {
        const auto                  NumTextures = static_cast<Uint32>(m_Scene.Textures.size());
        std::vector<IDeviceObject*> ppTextures(NumTextures);
        for (Uint32 i = 0; i < NumTextures; ++i)
            ppTextures[i] = m_Scene.Textures[i]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        m_RasterizationSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Textures")->SetArray(ppTextures.data(), 0, NumTextures);
    }

    // Bind samplers
    {
        const auto                  NumSamplers = static_cast<Uint32>(m_Scene.Samplers.size());
        std::vector<IDeviceObject*> ppSamplers(NumSamplers);
        for (Uint32 i = 0; i < NumSamplers; ++i)
            ppSamplers[i] = m_Scene.Samplers[i];
        m_RasterizationSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Samplers")->SetArray(ppSamplers.data(), 0, NumSamplers);
    }
}

void Tutorial22_HybridRendering::CreatePostProcessPSO(IShaderSourceInputStreamFactory* pShaderSourceFactory)
{
    // Create PSO for post process pass

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name         = "Post process PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    PSOCreateInfo.GraphicsPipeline.NumRenderTargets                  = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                     = m_pSwapChain->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology                 = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable      = false;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;

    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler             = m_ShaderCompiler;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Post process VS";
        ShaderCI.FilePath        = "PostProcess.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Post process PS";
        ShaderCI.FilePath        = "PostProcess.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_PostProcessPSO);
}

void Tutorial22_HybridRendering::CreateRayTracingPSO(IShaderSourceInputStreamFactory* pShaderSourceFactory)
{
    // Create compute shader that performs inline ray tracing

    ShaderMacroHelper Macros;
    Macros.AddShaderMacro("NUM_TEXTURES", static_cast<Uint32>(m_Scene.Textures.size()));
    Macros.AddShaderMacro("NUM_SAMPLERS", static_cast<Uint32>(m_Scene.Samplers.size()));

    ComputePipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

    const auto NumTextures = static_cast<Uint32>(m_Scene.Textures.size());
    const auto NumSamplers = static_cast<Uint32>(m_Scene.Samplers.size());

    // Split the resources of the ray tracing PSO into two groups.
    // The first group will contain scene resources. These resources
    // may be bound only once.
    // The second group will contain screen-dependent resources.
    // These resources will need to be bound every time the screen is resized.

    // Resource signature for scene resources
    {
        PipelineResourceSignatureDesc PRSDesc;
        PRSDesc.Name = "Ray tracing scene resources";

        // clang-format off
        const PipelineResourceDesc Resources[] =
        {
            {SHADER_TYPE_COMPUTE, "g_TLAS",            1,           SHADER_RESOURCE_TYPE_ACCEL_STRUCT},
            {SHADER_TYPE_COMPUTE, "g_Constants",       1,           SHADER_RESOURCE_TYPE_CONSTANT_BUFFER},
            {SHADER_TYPE_COMPUTE, "g_ObjectAttribs",   1,           SHADER_RESOURCE_TYPE_BUFFER_SRV},
            {SHADER_TYPE_COMPUTE, "g_MaterialAttribs", 1,           SHADER_RESOURCE_TYPE_BUFFER_SRV},
            {SHADER_TYPE_COMPUTE, "g_VertexBuffer",    1,           SHADER_RESOURCE_TYPE_BUFFER_SRV},
            {SHADER_TYPE_COMPUTE, "g_IndexBuffer",     1,           SHADER_RESOURCE_TYPE_BUFFER_SRV},
            {SHADER_TYPE_COMPUTE, "g_Textures",        NumTextures, SHADER_RESOURCE_TYPE_TEXTURE_SRV},
            {SHADER_TYPE_COMPUTE, "g_Samplers",        NumSamplers, SHADER_RESOURCE_TYPE_SAMPLER}
        };
        // clang-format on
        PRSDesc.BindingIndex = 0;
        PRSDesc.Resources    = Resources;
        PRSDesc.NumResources = _countof(Resources);
        m_pDevice->CreatePipelineResourceSignature(PRSDesc, &m_pRayTracingSceneResourcesSign);
        VERIFY_EXPR(m_pRayTracingSceneResourcesSign);
    }

    // Resource signature for screen resources
    {
        PipelineResourceSignatureDesc PRSDesc;
        PRSDesc.Name = "Ray tracing screen resources";

        // clang-format off
        const PipelineResourceDesc Resources[] =
        {
            {SHADER_TYPE_COMPUTE, "g_RayTracedTex",   1, SHADER_RESOURCE_TYPE_TEXTURE_UAV},
            {SHADER_TYPE_COMPUTE, "g_GBuffer_Normal", 1, SHADER_RESOURCE_TYPE_TEXTURE_SRV},
            {SHADER_TYPE_COMPUTE, "g_GBuffer_Depth",  1, SHADER_RESOURCE_TYPE_TEXTURE_SRV}
        };
        // clang-format on
        PRSDesc.BindingIndex = 1;
        PRSDesc.Resources    = Resources;
        PRSDesc.NumResources = _countof(Resources);
        m_pDevice->CreatePipelineResourceSignature(PRSDesc, &m_pRayTracingScreenResourcesSign);
        VERIFY_EXPR(m_pRayTracingScreenResourcesSign);
    }

    IPipelineResourceSignature* ppSignatures[]{m_pRayTracingSceneResourcesSign, m_pRayTracingScreenResourcesSign};
    PSOCreateInfo.ppResourceSignatures    = ppSignatures;
    PSOCreateInfo.ResourceSignaturesCount = _countof(ppSignatures);

    ShaderCreateInfo ShaderCI;
    ShaderCI.Desc.ShaderType            = SHADER_TYPE_COMPUTE;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.EntryPoint                 = "CSMain";
    ShaderCI.Macros                     = Macros;

    if (m_pDevice->GetDeviceInfo().IsMetalDevice())
    {
        // HLSL and MSL are very similar, so we can use the same code for all
        // platforms, with some macros help.
        ShaderCI.ShaderCompiler = SHADER_COMPILER_DEFAULT;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_MSL;
    }
    else
    {
        // Inline ray tracing requires shader model 6.5
        // Only DXC can compile HLSL for ray tracing.
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.ShaderCompiler = SHADER_COMPILER_DXC;
        ShaderCI.HLSLVersion    = {6, 5};
    }

    ShaderCI.Desc.Name = "Ray tracing CS";
    ShaderCI.FilePath  = "RayTracing.csh";
    if (m_pDevice->GetDeviceInfo().IsMetalDevice())
    {
        // The shader uses macros that are not supported by MSL parser in Metal backend
        ShaderCI.CompileFlags = SHADER_COMPILE_FLAG_SKIP_REFLECTION;
    }
    RefCntAutoPtr<IShader> pCS;
    m_pDevice->CreateShader(ShaderCI, &pCS);
    PSOCreateInfo.pCS = pCS;

    PSOCreateInfo.PSODesc.Name = "Ray tracing PSO";
    m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_RayTracingPSO);
    VERIFY_EXPR(m_RayTracingPSO);

    // Initialize SRB containing scene resources
    m_pRayTracingSceneResourcesSign->CreateShaderResourceBinding(&m_RayTracingSceneSRB);
    m_RayTracingSceneSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_TLAS")->Set(m_Scene.TLAS);
    m_RayTracingSceneSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Constants")->Set(m_Constants);
    m_RayTracingSceneSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_ObjectAttribs")->Set(m_Scene.ObjectAttribsBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_RayTracingSceneSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_MaterialAttribs")->Set(m_Scene.MaterialAttribsBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

    // Bind mesh geometry buffers. All meshes use shared vertex and index buffers.
    m_RayTracingSceneSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_VertexBuffer")->Set(m_Scene.Meshes[0].VertexBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_RayTracingSceneSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_IndexBuffer")->Set(m_Scene.Meshes[0].IndexBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

    // Bind material textures
    {
        std::vector<IDeviceObject*> ppTextures(NumTextures);
        for (Uint32 i = 0; i < NumTextures; ++i)
            ppTextures[i] = m_Scene.Textures[i]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        m_RayTracingSceneSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Textures")->SetArray(ppTextures.data(), 0, NumTextures);
    }

    // Bind samplers
    {
        std::vector<IDeviceObject*> ppSamplers(NumSamplers);
        for (Uint32 i = 0; i < NumSamplers; ++i)
            ppSamplers[i] = m_Scene.Samplers[i];
        m_RayTracingSceneSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Samplers")->SetArray(ppSamplers.data(), 0, NumSamplers);
    }
}

void Tutorial22_HybridRendering::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    // RayTracing feature indicates that some of ray tracing functionality is supported.
    // Acceleration structures are always supported if RayTracing feature is enabled.
    // Inline ray tracing may be unsupported by old DirectX 12 drivers or if this feature is not supported by Vulkan.
    if ((m_pDevice->GetAdapterInfo().RayTracing.CapFlags & RAY_TRACING_CAP_FLAG_INLINE_RAY_TRACING) == 0)
    {
        UNSUPPORTED("Inline ray tracing is not supported by device");
        return;
    }

    // Setup camera.
    m_Camera.SetPos(float3{-15.7f, 3.7f, -5.8f});
    m_Camera.SetRotation(17.7f, -0.1f);
    m_Camera.SetRotationSpeed(0.005f);
    m_Camera.SetMoveSpeed(5.f);
    m_Camera.SetSpeedUpScales(5.f, 10.f);

    CreateScene();

    // Create buffer for constants that is shared between all PSOs
    {
        BufferDesc BuffDesc;
        BuffDesc.Name      = "Global constants buffer";
        BuffDesc.BindFlags = BIND_UNIFORM_BUFFER;
        BuffDesc.Size      = sizeof(HLSL::GlobalConstants);
        m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_Constants);
    }

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);

    CreateRasterizationPSO(pShaderSourceFactory);
    CreatePostProcessPSO(pShaderSourceFactory);
    CreateRayTracingPSO(pShaderSourceFactory);
}

void Tutorial22_HybridRendering::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);

    // Require ray tracing feature.
    Attribs.EngineCI.Features.RayTracing = DEVICE_FEATURE_STATE_ENABLED;
}

void Tutorial22_HybridRendering::Render()
{
    // Update constants
    {
        const auto ViewProj = m_Camera.GetViewMatrix() * m_Camera.GetProjMatrix();

        HLSL::GlobalConstants GConst;
        GConst.ViewProj     = ViewProj.Transpose();
        GConst.ViewProjInv  = ViewProj.Inverse().Transpose();
        GConst.LightDir     = normalize(-m_LightDir);
        GConst.CameraPos    = float4(m_Camera.GetPos(), 0.f);
        GConst.DrawMode     = m_DrawMode;
        GConst.MaxRayLength = 100.f;
        GConst.AmbientLight = 0.1f;
        m_pImmediateContext->UpdateBuffer(m_Constants, 0, static_cast<Uint32>(sizeof(GConst)), &GConst, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // Update transformation for scene objects
        m_pImmediateContext->UpdateBuffer(m_Scene.ObjectAttribsBuffer, 0, static_cast<Uint32>(sizeof(HLSL::ObjectAttribs) * m_Scene.Objects.size()),
                                          m_Scene.Objects.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    UpdateTLAS();

    // Rasterization pass
    {
        ITextureView* RTVs[] = //
            {
                m_GBuffer.Color->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),
                m_GBuffer.Normal->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET) //
            };
        ITextureView* pDSV = m_GBuffer.Depth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
        m_pImmediateContext->SetRenderTargets(_countof(RTVs), RTVs, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        // All transitions for render targets happened in SetRenderTargets()
        const float ClearColor[4] = {};
        m_pImmediateContext->ClearRenderTarget(RTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_NONE);
        m_pImmediateContext->ClearRenderTarget(RTVs[1], ClearColor, RESOURCE_STATE_TRANSITION_MODE_NONE);
        m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_NONE);

        m_pImmediateContext->SetPipelineState(m_RasterizationPSO);
        m_pImmediateContext->CommitShaderResources(m_RasterizationSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        for (auto& ObjInst : m_Scene.ObjectInstances)
        {
            auto&        Mesh      = m_Scene.Meshes[ObjInst.MeshInd];
            IBuffer*     VBs[]     = {Mesh.VertexBuffer};
            const Uint64 Offsets[] = {Mesh.FirstVertex * sizeof(HLSL::Vertex)};

            m_pImmediateContext->SetVertexBuffers(0, _countof(VBs), VBs, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
            m_pImmediateContext->SetIndexBuffer(Mesh.IndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            {
                MapHelper<HLSL::ObjectConstants> ObjConstants{m_pImmediateContext, m_Scene.ObjectConstants, MAP_WRITE, MAP_FLAG_DISCARD};
                ObjConstants->ObjectAttribsOffset = ObjInst.ObjectAttribsOffset;
            }

            DrawIndexedAttribs drawAttribs;
            drawAttribs.NumIndices         = Mesh.NumIndices;
            drawAttribs.NumInstances       = ObjInst.NumObjects;
            drawAttribs.FirstIndexLocation = Mesh.FirstIndex;
            drawAttribs.IndexType          = VT_UINT32;
            drawAttribs.Flags              = DRAW_FLAG_VERIFY_ALL;
            m_pImmediateContext->DrawIndexed(drawAttribs);
        }
    }

    // Ray tracing pass
    {
        DispatchComputeAttribs dispatchAttribs;
        dispatchAttribs.MtlThreadGroupSizeX = m_BlockSize.x;
        dispatchAttribs.MtlThreadGroupSizeY = m_BlockSize.y;
        dispatchAttribs.MtlThreadGroupSizeZ = 1;

        const auto& TexDesc               = m_GBuffer.Color->GetDesc();
        dispatchAttribs.ThreadGroupCountX = (TexDesc.Width / m_BlockSize.x);
        dispatchAttribs.ThreadGroupCountY = (TexDesc.Height / m_BlockSize.y);

        m_pImmediateContext->SetPipelineState(m_RayTracingPSO);
        m_pImmediateContext->CommitShaderResources(m_RayTracingSceneSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->CommitShaderResources(m_RayTracingScreenSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->DispatchCompute(dispatchAttribs);
    }

    // Post process pass
    {
        auto*       pRTV          = m_pSwapChain->GetCurrentBackBufferRTV();
        const float ClearColor[4] = {};
        m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_pImmediateContext->SetPipelineState(m_PostProcessPSO);
        m_pImmediateContext->CommitShaderResources(m_PostProcessSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_pImmediateContext->SetVertexBuffers(0, 0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE, SET_VERTEX_BUFFERS_FLAG_RESET);
        m_pImmediateContext->SetIndexBuffer(nullptr, 0, RESOURCE_STATE_TRANSITION_MODE_NONE);

        m_pImmediateContext->Draw(DrawAttribs{3, DRAW_FLAG_VERIFY_ALL});
    }
}

void Tutorial22_HybridRendering::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    const float dt = static_cast<float>(ElapsedTime);

    m_Camera.Update(m_InputController, dt);

    // Restrict camera movement
    float3       Pos = m_Camera.GetPos();
    const float3 MinXYZ{-20.f, 0.1f, -20.f};
    const float3 MaxXYZ{+20.f, +20.f, 20.f};
    if (Pos.x < MinXYZ.x || Pos.y < MinXYZ.y || Pos.z < MinXYZ.z ||
        Pos.x > MaxXYZ.x || Pos.y > MaxXYZ.y || Pos.z > MaxXYZ.z)
    {
        Pos = clamp(Pos, MinXYZ, MaxXYZ);
        m_Camera.SetPos(Pos);
        m_Camera.Update(m_InputController, 0);
    }

    // Update dynamic objects
    float RotationSpeed = 0.15f;
    for (auto& DynObj : m_Scene.DynamicObjects)
    {
        auto& Obj      = m_Scene.Objects[DynObj.ObjectAttribsIndex];
        auto  ModelMat = Obj.ModelMat.Transpose();
        Obj.ModelMat   = (float4x4::RotationY(PI_F * dt * RotationSpeed) * ModelMat).Transpose();
        Obj.NormalMat  = float4x3{Obj.ModelMat};

        RotationSpeed *= 1.5f;
    }
}

void Tutorial22_HybridRendering::WindowResize(Uint32 Width, Uint32 Height)
{
    if (Width == 0 || Height == 0)
        return;

    // Round to multiple of m_BlockSize
    Width  = AlignUp(Width, m_BlockSize.x);
    Height = AlignUp(Height, m_BlockSize.y);

    // Update projection matrix.
    float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
    m_Camera.SetProjAttribs(0.1f, 100.f, AspectRatio, PI_F / 4.f,
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
    RTDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    RTDesc.Format    = m_ColorTargetFormat;
    m_pDevice->CreateTexture(RTDesc, nullptr, &m_GBuffer.Color);

    RTDesc.Name      = "GBuffer Normal";
    RTDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    RTDesc.Format    = m_NormalTargetFormat;
    m_pDevice->CreateTexture(RTDesc, nullptr, &m_GBuffer.Normal);

    RTDesc.Name      = "GBuffer Depth";
    RTDesc.BindFlags = BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE;
    RTDesc.Format    = m_DepthTargetFormat;
    m_pDevice->CreateTexture(RTDesc, nullptr, &m_GBuffer.Depth);

    RTDesc.Name      = "Ray traced shadow & reflection";
    RTDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    RTDesc.Format    = m_RayTracedTexFormat;
    m_RayTracedTex.Release();
    m_pDevice->CreateTexture(RTDesc, nullptr, &m_RayTracedTex);


    // Create post-processing SRB
    {
        m_PostProcessSRB.Release();
        m_PostProcessPSO->CreateShaderResourceBinding(&m_PostProcessSRB);
        m_PostProcessSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Constants")->Set(m_Constants);
        m_PostProcessSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_GBuffer_Color")->Set(m_GBuffer.Color->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        m_PostProcessSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_GBuffer_Normal")->Set(m_GBuffer.Normal->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        m_PostProcessSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_GBuffer_Depth")->Set(m_GBuffer.Depth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        m_PostProcessSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_RayTracedTex")->Set(m_RayTracedTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    }

    // Create ray-tracing screen SRB
    {
        m_RayTracingScreenSRB.Release();
        m_pRayTracingScreenResourcesSign->CreateShaderResourceBinding(&m_RayTracingScreenSRB);
        m_RayTracingScreenSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_RayTracedTex")->Set(m_RayTracedTex->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
        m_RayTracingScreenSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_GBuffer_Depth")->Set(m_GBuffer.Depth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        m_RayTracingScreenSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_GBuffer_Normal")->Set(m_GBuffer.Normal->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    }
}

void Tutorial22_HybridRendering::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Combo("Render mode", &m_DrawMode,
                     "Shaded\0"
                     "G-buffer color\0"
                     "G-buffer normal\0"
                     "Diffuse lighting\0"
                     "Reflections\0"
                     "Fresnel term\0\0");

        if (ImGui::gizmo3D("##LightDirection", m_LightDir))
        {
            if (m_LightDir.y > -0.06f)
            {
                m_LightDir.y = -0.06f;
                m_LightDir   = normalize(m_LightDir);
            }
        }
    }
    ImGui::End();
}

} // namespace Diligent
