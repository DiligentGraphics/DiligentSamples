/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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
#include "Align.hpp"
#include "../../Common/src/TexturedCube.hpp"

namespace Diligent
{

static_assert(sizeof(GlobalConstants) % 16 == 0, "Structure must be 16-byte aligned");
static_assert(sizeof(ObjectConstants) % 16 == 0, "Structure must be 16-byte aligned");

SampleBase* CreateSample()
{
    return new Tutorial22_HybridRendering();
}

void Tutorial22_HybridRendering::CreateSceneMaterials(IRenderDevice* pDevice, Scene& scene, SceneTempData& temp)
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
        pDevice->CreateSampler(AnisotropicClampSampler, &pSampler);
        AnisotropicClampSampInd = static_cast<Uint32>(scene.Samplers.size());
        scene.Samplers.push_back(std::move(pSampler));

        pSampler = nullptr;
        pDevice->CreateSampler(AnisotropicWrapSampler, &pSampler);
        AnisotropicWrapSampInd = static_cast<Uint32>(scene.Samplers.size());
        scene.Samplers.push_back(std::move(pSampler));
    }

    const auto LoadMaterial = [&](const char* ColorMapName, const float4& baseColor, Uint32 SamplerInd) //
    {
        MaterialAttribs         mtr;
        TextureLoadInfo         loadInfo;
        RefCntAutoPtr<ITexture> Tex;

        loadInfo.IsSRGB       = true;
        loadInfo.GenerateMips = true;

        mtr.SampInd = SamplerInd;

        CreateTextureFromFile(ColorMapName, loadInfo, pDevice, &Tex);
        VERIFY_EXPR(Tex);
        mtr.BaseColorMask   = baseColor;
        mtr.BaseColorTexInd = static_cast<Uint32>(scene.Textures.size());
        scene.Textures.push_back(std::move(Tex));
        temp.Materials.push_back(mtr);
    };

    // Cube materials
    temp.CubeMaterialRange.x = static_cast<Uint32>(temp.Materials.size());
    LoadMaterial("DGLogo0.png", float4{1.f}, AnisotropicClampSampInd);
    LoadMaterial("DGLogo1.png", float4{1.f}, AnisotropicClampSampInd);
    LoadMaterial("DGLogo2.png", float4{1.f}, AnisotropicClampSampInd);
    LoadMaterial("DGLogo3.png", float4{1.f}, AnisotropicClampSampInd);
    temp.CubeMaterialRange.y = static_cast<Uint32>(temp.Materials.size());

    // Ground materials
    temp.GroundMaterial = static_cast<Uint32>(temp.Materials.size());
    LoadMaterial("DGLogo0.png", float4(1.f, 1.f, 1.f, 1.f), AnisotropicWrapSampInd);
}

Tutorial22_HybridRendering::Mesh Tutorial22_HybridRendering::CreateTexturedPlaneMesh(IRenderDevice* pDevice, float2 UVScale)
{
    Mesh PlaneMesh;
    PlaneMesh.Name = "Ground";

    struct Vertex
    {
        float3 pos;
        float3 norm;
        float2 uv;
    };
    std::vector<Vertex> Vertices;
    std::vector<Uint32> Indices;

    const Uint32 GridSize = 2;
    const float  Scale    = 1.0f / float(GridSize - 1);

    for (Uint32 y = 0; y < GridSize; ++y)
    {
        for (Uint32 x = 0; x < GridSize; ++x)
        {
            Vertex vert;
            float2 uv = float2{static_cast<float>(x), static_cast<float>(y)} * Scale;
            vert.norm = float3{0, 1, 0};
            vert.uv   = uv * UVScale;
            vert.pos  = float3{uv.x * 2.f - 1.f, 0.f, uv.y * 2.f - 1.f};
            Vertices.push_back(vert);

            if (x > 0 && y > 0)
            {
                Indices.push_back((y - 1) * GridSize + x - 1);
                Indices.push_back(y * GridSize + x - 1);
                Indices.push_back(y * GridSize + x);

                Indices.push_back(y * GridSize + x);
                Indices.push_back((y - 1) * GridSize + x);
                Indices.push_back((y - 1) * GridSize + x - 1);
            }
        }
    }

    BufferDesc BuffDesc;
    BuffDesc.Name              = "Plane vertex buffer";
    BuffDesc.Usage             = USAGE_IMMUTABLE;
    BuffDesc.BindFlags         = BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE | BIND_RAY_TRACING;
    BuffDesc.uiSizeInBytes     = static_cast<Uint32>(sizeof(Vertices[0]) * Vertices.size());
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    BuffDesc.ElementByteStride = sizeof(Vertices[0]);
    BufferData VBData{Vertices.data(), BuffDesc.uiSizeInBytes};
    pDevice->CreateBuffer(BuffDesc, &VBData, &PlaneMesh.VertexBuffer);

    BuffDesc.Name              = "Plane index buffer";
    BuffDesc.BindFlags         = BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE | BIND_RAY_TRACING;
    BuffDesc.uiSizeInBytes     = static_cast<Uint32>(sizeof(Indices[0]) * Indices.size());
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    BuffDesc.ElementByteStride = sizeof(Indices[0]);
    BufferData IBData{Indices.data(), BuffDesc.uiSizeInBytes};
    pDevice->CreateBuffer(BuffDesc, &IBData, &PlaneMesh.IndexBuffer);

    PlaneMesh.NumVertices = static_cast<Uint32>(Vertices.size());
    PlaneMesh.NumIndices  = static_cast<Uint32>(Indices.size());
    return PlaneMesh;
}

void Tutorial22_HybridRendering::CreateSceneObjects(IRenderDevice* pDevice, Scene& scene, SceneTempData& temp)
{
    Uint32 CubeMeshId  = 0;
    Uint32 PlaneMeshId = 0;

    // Create meshes
    {
        Mesh CubeMesh;
        CubeMesh.Name         = "Cube";
        CubeMesh.VertexBuffer = TexturedCube::CreateVertexBuffer(
            pDevice,
            TexturedCube::VERTEX_COMPONENT_FLAG_POS_NORM_UV,
            BIND_VERTEX_BUFFER | BIND_SHADER_RESOURCE | BIND_RAY_TRACING,
            BUFFER_MODE_STRUCTURED);
        CubeMesh.IndexBuffer = TexturedCube::CreateIndexBuffer(pDevice, BIND_INDEX_BUFFER | BIND_SHADER_RESOURCE | BIND_RAY_TRACING, BUFFER_MODE_STRUCTURED);
        CubeMesh.NumVertices = 12;
        CubeMesh.NumIndices  = 36;
        CubeMeshId           = static_cast<Uint32>(scene.Meshes.size());
        scene.Meshes.push_back(CubeMesh);

        auto PlaneMesh = CreateTexturedPlaneMesh(pDevice, float2{10.f, 10.f});
        PlaneMeshId    = static_cast<Uint32>(scene.Meshes.size());
        scene.Meshes.push_back(PlaneMesh);
    }

    const auto AddCubeObject = [&](float Angle, float X, float Y, float Z, float Scale, Uint32 MatId) //
    {
        const auto ModelMat  = float4x4::RotationY(Angle * PI_F) * float4x4::Scale(Scale) * float4x4::Translation(X * 2.0f, Y * 2.0f - 1.0f, Z * 2.0f);
        const auto NormalMat = float3x3{ModelMat.m00, ModelMat.m01, ModelMat.m02,
                                        ModelMat.m10, ModelMat.m11, ModelMat.m12,
                                        ModelMat.m20, ModelMat.m21, ModelMat.m22};

        ObjectAttribs obj;
        obj.ModelMat   = ModelMat.Transpose();
        obj.NormalMat  = NormalMat.Transpose();
        obj.MaterialId = (MatId % (temp.CubeMaterialRange.y - temp.CubeMaterialRange.x)) + temp.CubeMaterialRange.x;
        obj.MeshId     = CubeMeshId;
        obj.FirstIndex = scene.Meshes[obj.MeshId].FirstIndex;
        temp.Objects.push_back(obj);
    };
    // clang-format off
    AddCubeObject(0.25f,  0.0f, 1.00f,  1.5f, 0.9f, 4);
    AddCubeObject(0.00f, -1.9f, 1.00f, -0.5f, 0.5f, 4);
    AddCubeObject(0.00f, -1.0f, 1.00f,  0.0f, 1.0f, 0);
    AddCubeObject(0.30f, -0.2f, 1.00f, -1.0f, 0.7f, 1);
    AddCubeObject(0.25f, -1.7f, 1.00f, -1.6f, 1.1f, 3);
    AddCubeObject(0.28f,  0.7f, 1.00f,  3.0f, 1.3f, 0);
    AddCubeObject(0.10f,  1.5f, 1.00f,  1.0f, 1.1f, 1);
    AddCubeObject(0.21f, -3.2f, 1.00f,  0.2f, 1.2f, 3);
    AddCubeObject(0.05f, -2.1f, 1.00f,  1.6f, 1.1f, 0);
    
    AddCubeObject(0.04f, -1.4f, 2.18f, -1.4f, 0.6f, 3);
    AddCubeObject(0.24f, -1.0f, 2.10f,  0.5f, 1.1f, 0);
    AddCubeObject(0.02f, -0.5f, 2.00f, -0.9f, 0.9f, 1);
    AddCubeObject(0.08f, -2.7f, 1.96f, -0.7f, 0.7f, 3);
    AddCubeObject(0.17f,  1.5f, 2.00f,  1.1f, 0.9f, 0);
    
    AddCubeObject(1.9f, -1.0f, 3.25f, -0.2f, 1.2f, 2);
    // clang-format on

    InstancedObjects InstObj;
    InstObj.MeshInd          = CubeMeshId;
    InstObj.NumObjects       = static_cast<Uint32>(temp.Objects.size());
    InstObj.ObjectDataOffset = 0;
    scene.Objects.push_back(InstObj);

    // Create ground plane object
    InstObj.ObjectDataOffset = static_cast<Uint32>(temp.Objects.size());
    InstObj.MeshInd          = PlaneMeshId;
    {
        ObjectAttribs obj;
        obj.ModelMat   = float4x4::Scale(50.f, 1.f, 50.f).Transpose();
        obj.NormalMat  = float3x3::Identity();
        obj.MaterialId = temp.GroundMaterial;
        obj.MeshId     = PlaneMeshId;
        obj.FirstIndex = scene.Meshes[obj.MeshId].FirstIndex;
        temp.Objects.push_back(obj);
    }
    InstObj.NumObjects = static_cast<Uint32>(temp.Objects.size()) - InstObj.ObjectDataOffset;
    scene.Objects.push_back(InstObj);
}

void Tutorial22_HybridRendering::CreateSceneAccelStructs(IRenderDevice* pDevice, IDeviceContext* pContext, Scene& scene, SceneTempData& temp)
{
    // Create & build bottom level acceleration structure
    {
        RefCntAutoPtr<IBuffer> pScratchBuffer;

        for (auto& Mesh : scene.Meshes)
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

                const String      BLASName = Mesh.Name + " BLAS";
                BottomLevelASDesc ASDesc;
                ASDesc.Name          = BLASName.c_str();
                ASDesc.Flags         = RAYTRACING_BUILD_AS_PREFER_FAST_TRACE;
                ASDesc.pTriangles    = &Triangles;
                ASDesc.TriangleCount = 1;
                pDevice->CreateBLAS(ASDesc, &Mesh.BLAS);
            }

            if (!pScratchBuffer || pScratchBuffer->GetDesc().uiSizeInBytes < Mesh.BLAS->GetScratchBufferSizes().Build)
            {
                BufferDesc BuffDesc;
                BuffDesc.Name          = "BLAS Scratch Buffer";
                BuffDesc.Usage         = USAGE_DEFAULT;
                BuffDesc.BindFlags     = BIND_RAY_TRACING;
                BuffDesc.uiSizeInBytes = Mesh.BLAS->GetScratchBufferSizes().Build;

                pScratchBuffer = nullptr;
                pDevice->CreateBuffer(BuffDesc, nullptr, &pScratchBuffer);
            }

            // Build BLAS
            BLASBuildTriangleData TriangleData;
            TriangleData.GeometryName         = Triangles.GeometryName;
            TriangleData.pVertexBuffer        = Mesh.VertexBuffer;
            TriangleData.VertexStride         = Mesh.VertexBuffer->GetDesc().ElementByteStride;
            TriangleData.VertexCount          = Triangles.MaxVertexCount;
            TriangleData.VertexValueType      = Triangles.VertexValueType;
            TriangleData.VertexComponentCount = Triangles.VertexComponentCount;
            TriangleData.pIndexBuffer         = Mesh.IndexBuffer;
            TriangleData.PrimitiveCount       = Triangles.MaxPrimitiveCount;
            TriangleData.IndexType            = Triangles.IndexType;
            TriangleData.Flags                = RAYTRACING_GEOMETRY_FLAG_OPAQUE;

            BuildBLASAttribs Attribs;
            Attribs.pBLAS             = Mesh.BLAS;
            Attribs.pTriangleData     = &TriangleData;
            Attribs.TriangleDataCount = 1;

            // Scratch buffer will be used to store temporary data during BLAS build.
            // Previous content in the scratch buffer will be discarded.
            Attribs.pScratchBuffer = pScratchBuffer;

            // Allow engine to change resource states.
            Attribs.BLASTransitionMode          = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            Attribs.GeometryTransitionMode      = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            Attribs.ScratchBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

            pContext->BuildBLAS(Attribs);
        }
    }

    // Create & build top-level acceleration structure
    {
        const Uint32 NumInstances = static_cast<Uint32>(temp.Objects.size());

        // Create TLAS
        {
            TopLevelASDesc TLASDesc;
            TLASDesc.Name             = "Scene TLAS";
            TLASDesc.MaxInstanceCount = NumInstances;
            TLASDesc.Flags            = RAYTRACING_BUILD_AS_PREFER_FAST_TRACE;
            pDevice->CreateTLAS(TLASDesc, &scene.TLAS);
        }

        // Create scratch buffer
        RefCntAutoPtr<IBuffer> pScratchBuffer;
        {
            BufferDesc BuffDesc;
            BuffDesc.Name          = "TLAS Scratch Buffer";
            BuffDesc.Usage         = USAGE_DEFAULT;
            BuffDesc.BindFlags     = BIND_RAY_TRACING;
            BuffDesc.uiSizeInBytes = scene.TLAS->GetScratchBufferSizes().Build;
            pDevice->CreateBuffer(BuffDesc, nullptr, &pScratchBuffer);
        }

        // Create instance buffer
        RefCntAutoPtr<IBuffer> pInstanceBuffer;
        {
            BufferDesc BuffDesc;
            BuffDesc.Name          = "TLAS Instance Buffer";
            BuffDesc.Usage         = USAGE_DEFAULT;
            BuffDesc.BindFlags     = BIND_RAY_TRACING;
            BuffDesc.uiSizeInBytes = TLAS_INSTANCE_DATA_SIZE * NumInstances;
            pDevice->CreateBuffer(BuffDesc, nullptr, &pInstanceBuffer);
        }

        // Setup instances
        std::vector<TLASBuildInstanceData> Instances{NumInstances};
        std::vector<String>                InstanceNames{NumInstances};

        for (Uint32 i = 0; i < NumInstances; ++i)
        {
            const auto& Obj      = temp.Objects[i];
            auto&       Inst     = Instances[i];
            auto&       Name     = InstanceNames[i];
            const auto& Mesh     = scene.Meshes[Obj.MeshId];
            const auto  ModelMat = Obj.ModelMat.Transpose();

            Name = Mesh.Name + " Instance (" + std::to_string(i) + ")";

            Inst.InstanceName = Name.c_str();
            Inst.pBLAS        = Mesh.BLAS.RawPtr<IBottomLevelAS>();
            Inst.Mask         = 0xFF;
            Inst.CustomId     = i; // CommittedInstanceID()

            Inst.Transform.SetRotation(ModelMat.Data(), 4);
            Inst.Transform.SetTranslation(ModelMat.m30, ModelMat.m31, ModelMat.m32);
        }

        // Build  TLAS
        BuildTLASAttribs Attribs;
        Attribs.pTLAS = scene.TLAS;

        // Scratch buffer will be used to store temporary data during TLAS build or update.
        // Previous content in the scratch buffer will be discarded.
        Attribs.pScratchBuffer = pScratchBuffer;

        // Instance buffer will store instance data during TLAS build or update.
        // Previous content in the instance buffer will be discarded.
        Attribs.pInstanceBuffer = pInstanceBuffer;

        // Instances will be converted to the format that is required by the graphics driver and copied to the instance buffer.
        Attribs.pInstances    = Instances.data();
        Attribs.InstanceCount = static_cast<Uint32>(Instances.size());

        // Allow engine to change resource states.
        Attribs.TLASTransitionMode           = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        Attribs.BLASTransitionMode           = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        Attribs.InstanceBufferTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
        Attribs.ScratchBufferTransitionMode  = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

        pContext->BuildTLAS(Attribs);
    }
}

void Tutorial22_HybridRendering::CreateScene()
{
    SceneTempData temp;
    CreateSceneMaterials(m_pDevice, m_Scene, temp);
    CreateSceneObjects(m_pDevice, m_Scene, temp);
    CreateSceneAccelStructs(m_pDevice, m_pImmediateContext, m_Scene, temp);

    // Create buffer for object and material attribs
    {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Object attribs buffer";
        BuffDesc.BindFlags         = BIND_SHADER_RESOURCE;
        BuffDesc.uiSizeInBytes     = static_cast<Uint32>(sizeof(temp.Objects[0]) * temp.Objects.size());
        BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
        BuffDesc.ElementByteStride = sizeof(temp.Objects[0]);
        BufferData BuffData{temp.Objects.data(), BuffDesc.uiSizeInBytes};
        m_pDevice->CreateBuffer(BuffDesc, &BuffData, &m_Scene.ObjectAttribsBuffer);

        BuffDesc.Name              = "Material attribs buffer";
        BuffDesc.uiSizeInBytes     = static_cast<Uint32>(sizeof(temp.Materials[0]) * temp.Materials.size());
        BuffDesc.ElementByteStride = sizeof(temp.Materials[0]);
        BuffData                   = BufferData{temp.Materials.data(), BuffDesc.uiSizeInBytes};
        m_pDevice->CreateBuffer(BuffDesc, &BuffData, &m_Scene.MaterialAttribsBuffer);
    }
}

void Tutorial22_HybridRendering::CreateRasterizationPSO(IShaderSourceInputStreamFactory* pShaderSourceFactory)
{
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
    m_RasterizationSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_ObjectConst")->Set(m_ObjectConstants);
    m_RasterizationSRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_ObjectAttribs")->Set(m_Scene.ObjectAttribsBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_RasterizationSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_MaterialAttribs")->Set(m_Scene.MaterialAttribsBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

    auto* pTexVar = m_RasterizationSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Textures");
    for (Uint32 i = 0; i < m_Scene.Textures.size(); ++i)
    {
        IDeviceObject* pTexView = m_Scene.Textures[i]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        pTexVar->SetArray(&pTexView, i, 1);
    }

    auto* pSampVar = m_RasterizationSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Samplers");
    for (Uint32 i = 0; i < m_Scene.Samplers.size(); ++i)
        pSampVar->SetArray(m_Scene.Samplers[i].RawDblPtr<IDeviceObject>(), i, 1);
}

void Tutorial22_HybridRendering::CreatePostProcessPSO(IShaderSourceInputStreamFactory* pShaderSourceFactory)
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name         = "Post process PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    PSOCreateInfo.GraphicsPipeline.NumRenderTargets                  = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                     = m_pSwapChain->GetDesc().ColorBufferFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology                 = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable      = false;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthWriteEnable = false;

    const ShaderResourceVariableDesc Veriables[]             = {{SHADER_TYPE_PIXEL, "g_Constants", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}};
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables        = _countof(Veriables);
    PSOCreateInfo.PSODesc.ResourceLayout.Variables           = Veriables;
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC;

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
    m_PostProcessPSO->CreateShaderResourceBinding(&m_PostProcessSRB);

    m_PostProcessSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Constants")->Set(m_Constants);
}

void Tutorial22_HybridRendering::CreateRayTracingPSO(IShaderSourceInputStreamFactory* pShaderSourceFactory)
{
    ShaderMacroHelper Macros;
    Macros.AddShaderMacro("NUM_TEXTURES", static_cast<Uint32>(m_Scene.Textures.size()));
    Macros.AddShaderMacro("NUM_SAMPLERS", static_cast<Uint32>(m_Scene.Samplers.size()));
    Macros.AddShaderMacro("NUM_MESHES", static_cast<Uint32>(m_Scene.Meshes.size()));

    if (m_pDevice->GetDeviceInfo().IsMetalDevice())
    {
        Macros.AddShaderMacro("float4x3", "float3x3");
    }

    ComputePipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_COMPUTE;

    const ShaderResourceVariableDesc Veriables[] = {
        {SHADER_TYPE_COMPUTE, "g_RayTracedTex", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
        {SHADER_TYPE_COMPUTE, "g_GBuffer_Normal", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC} //
    };
    PSOCreateInfo.PSODesc.ResourceLayout.NumVariables        = _countof(Veriables);
    PSOCreateInfo.PSODesc.ResourceLayout.Variables           = Veriables;
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    ShaderCreateInfo ShaderCI;
    ShaderCI.Desc.ShaderType            = SHADER_TYPE_COMPUTE;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.EntryPoint                 = "CSMain";
    ShaderCI.Macros                     = Macros;

    if (m_pDevice->GetDeviceInfo().IsMetalDevice())
    {
        ShaderCI.ShaderCompiler = SHADER_COMPILER_DEFAULT;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_MSL;
    }
    else
    {
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        ShaderCI.ShaderCompiler = SHADER_COMPILER_DXC;
        ShaderCI.HLSLVersion    = {6, 5};
    }

    ShaderCI.Desc.Name = "Ray traced shadow CS";
    ShaderCI.FilePath  = "RayTracing.csh";
    RefCntAutoPtr<IShader> pCS;
    m_pDevice->CreateShader(ShaderCI, &pCS);
    PSOCreateInfo.pCS = pCS;

    PSOCreateInfo.PSODesc.Name = "Ray traced shadow PSO";
    m_pDevice->CreateComputePipelineState(PSOCreateInfo, &m_RayTracingPSO);
    m_RayTracingPSO->CreateShaderResourceBinding(&m_RayTracingSRB);

    m_RayTracingSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_TLAS")->Set(m_Scene.TLAS);
    m_RayTracingSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Constants")->Set(m_Constants);
    m_RayTracingSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_ObjectAttribs")->Set(m_Scene.ObjectAttribsBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_RayTracingSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_MaterialAttribs")->Set(m_Scene.MaterialAttribsBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

    auto* pVBVar = m_RayTracingSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_VertexBuffers");
    auto* pIBVar = m_RayTracingSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_IndexBuffers");
    for (Uint32 i = 0; i < m_Scene.Meshes.size(); ++i)
    {
        IDeviceObject* pVBView = m_Scene.Meshes[i].VertexBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        pVBVar->SetArray(&pVBView, i, 1);
        IDeviceObject* pIBView = m_Scene.Meshes[i].IndexBuffer->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE);
        pIBVar->SetArray(&pIBView, i, 1);
    }

    auto* pTexVar = m_RayTracingSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Textures");
    for (Uint32 i = 0; i < m_Scene.Textures.size(); ++i)
    {
        IDeviceObject* pTexView = m_Scene.Textures[i]->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        pTexVar->SetArray(&pTexView, i, 1);
    }

    auto* pSampVar = m_RayTracingSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_Samplers");
    for (Uint32 i = 0; i < m_Scene.Samplers.size(); ++i)
        pSampVar->SetArray(m_Scene.Samplers[i].RawDblPtr<IDeviceObject>(), i, 1);
}

void Tutorial22_HybridRendering::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    if ((m_pDevice->GetAdapterInfo().RayTracing.CapFlags & RAY_TRACING_CAP_FLAG_INLINE_RAY_TRACING) == 0)
    {
        UNSUPPORTED("Inline ray tracing is not supported by device");
        return;
    }

    // Setup camera.
    m_Camera.SetPos(float3(-10.1f, 7.5f, 14.f));
    m_Camera.SetRotation(16.3f, -0.39f);
    m_Camera.SetRotationSpeed(0.005f);
    m_Camera.SetMoveSpeed(5.f);
    m_Camera.SetSpeedUpScales(5.f, 10.f);

    CreateScene();

    BufferDesc BuffDesc;
    BuffDesc.Name          = "Global constants buffer";
    BuffDesc.BindFlags     = BIND_UNIFORM_BUFFER;
    BuffDesc.uiSizeInBytes = sizeof(GlobalConstants);
    m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_Constants);

    BuffDesc.uiSizeInBytes  = sizeof(ObjectConstants);
    BuffDesc.Usage          = USAGE_DYNAMIC;
    BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_ObjectConstants);

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
        const auto& TexDesc  = m_GBuffer.Color->GetDesc();
        const auto  ViewProj = m_Camera.GetViewMatrix() * m_Camera.GetProjMatrix();

        GlobalConstants GConst;
        GConst.ViewProj               = ViewProj.Transpose();
        GConst.ViewProjInv            = ViewProj.Inverse().Transpose();
        GConst.LightPos               = m_LightPos;
        GConst.SkyColor               = m_SkyColor;
        GConst.CameraPos              = float4(m_Camera.GetPos(), 0.f);
        GConst.DrawMode               = m_DrawMode;
        GConst.GBufferDimension       = uint2{TexDesc.Width, TexDesc.Height};
        GConst.MaxReflectionRayLength = 10.f;
        GConst.AmbientLight           = 0.1f;
        m_pImmediateContext->UpdateBuffer(m_Constants, 0, static_cast<Uint32>(sizeof(GConst)), &GConst, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    // Rasterization pass
    {
        ITextureView* RTVs[] = //
            {
                m_GBuffer.Color->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),
                m_GBuffer.Normal->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET) //
            };
        ITextureView* pDSV = m_GBuffer.Depth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
        m_pImmediateContext->SetRenderTargets(_countof(RTVs), RTVs, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_pImmediateContext->ClearRenderTarget(RTVs[0], m_SkyColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        const float ClearNormal[4] = {};
        m_pImmediateContext->ClearRenderTarget(RTVs[1], ClearNormal, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_pImmediateContext->SetPipelineState(m_RasterizationPSO);
        m_pImmediateContext->CommitShaderResources(m_RasterizationSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        for (auto& Obj : m_Scene.Objects)
        {
            auto&    Mesh      = m_Scene.Meshes[Obj.MeshInd];
            IBuffer* VBs[]     = {Mesh.VertexBuffer};
            Uint32   Offsets[] = {0};

            m_pImmediateContext->SetVertexBuffers(0, _countof(VBs), VBs, Offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
            m_pImmediateContext->SetIndexBuffer(Mesh.IndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

            {
                MapHelper<ObjectConstants> ObjConstants(m_pImmediateContext, m_ObjectConstants, MAP_WRITE, MAP_FLAG_DISCARD);
                ObjConstants->ObjectDataOffset = Obj.ObjectDataOffset;
            }

            DrawIndexedAttribs drawAttribs;
            drawAttribs.NumIndices         = Mesh.NumIndices;
            drawAttribs.NumInstances       = Obj.NumObjects;
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

        const auto TexDesc                = m_GBuffer.Color->GetDesc();
        dispatchAttribs.ThreadGroupCountX = (TexDesc.Width / m_BlockSize.x);
        dispatchAttribs.ThreadGroupCountY = (TexDesc.Height / m_BlockSize.y);

        m_RayTracingSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_RayTracedTex")->Set(m_RayTracedTex->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
        m_RayTracingSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_GBuffer_Depth")->Set(m_GBuffer.Depth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        m_RayTracingSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_GBuffer_Normal")->Set(m_GBuffer.Normal->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

        m_pImmediateContext->SetPipelineState(m_RayTracingPSO);
        m_pImmediateContext->CommitShaderResources(m_RayTracingSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->DispatchCompute(dispatchAttribs);
    }

    // Post process pass
    {
        m_PostProcessSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_GBuffer_Color")->Set(m_GBuffer.Color->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        m_PostProcessSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_GBuffer_Normal")->Set(m_GBuffer.Normal->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        m_PostProcessSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_GBuffer_Depth")->Set(m_GBuffer.Depth->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        m_PostProcessSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_RayTracedTex")->Set(m_RayTracedTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));

        m_pImmediateContext->SetPipelineState(m_PostProcessPSO);
        m_pImmediateContext->CommitShaderResources(m_PostProcessSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        auto*       pRTV          = m_pSwapChain->GetCurrentBackBufferRTV();
        const float ClearColor[4] = {};
        m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_pImmediateContext->SetVertexBuffers(0, 0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE, SET_VERTEX_BUFFERS_FLAG_RESET);
        m_pImmediateContext->SetIndexBuffer(nullptr, 0, RESOURCE_STATE_TRANSITION_MODE_NONE);

        m_pImmediateContext->Draw(DrawAttribs{3, DRAW_FLAG_VERIFY_ALL});
    }
}

void Tutorial22_HybridRendering::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));

    // Do not go underground,
    float3 oldPos = m_Camera.GetPos();
    if (oldPos.y < 0.1f)
    {
        oldPos.y = 0.1f;
        m_Camera.SetPos(oldPos);
    }
}

void Tutorial22_HybridRendering::WindowResize(Uint32 Width, Uint32 Height)
{
    // Round to multiple of m_BlockSize
    Width  = AlignUp(Width, m_BlockSize.x);
    Height = AlignUp(Height, m_BlockSize.y);

    // Update projection matrix.
    float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
    m_Camera.SetProjAttribs(1.f, 100.f, AspectRatio, PI_F / 4.f,
                            m_pSwapChain->GetDesc().PreTransform, m_pDevice->GetDeviceInfo().IsGLDevice());

    // Check if the image needs to be recreated.
    if (m_GBuffer.Color != nullptr &&
        m_GBuffer.Color->GetDesc().Width == Width &&
        m_GBuffer.Color->GetDesc().Height == Height)
        return;

    if (Width == 0 || Height == 0)
        return;

    m_GBuffer.Color  = nullptr;
    m_GBuffer.Normal = nullptr;
    m_GBuffer.Depth  = nullptr;
    m_RayTracedTex   = nullptr;

    // Create window-size color image.
    TextureDesc RTDesc;
    RTDesc.Name      = "GBuffer Color target";
    RTDesc.Type      = RESOURCE_DIM_TEX_2D;
    RTDesc.Width     = Width;
    RTDesc.Height    = Height;
    RTDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    RTDesc.Format    = m_ColorTargetFormat;

    m_pDevice->CreateTexture(RTDesc, nullptr, &m_GBuffer.Color);

    RTDesc.Name      = "GBuffer Normal target";
    RTDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    RTDesc.Format    = m_NormalTargetFormat;

    m_pDevice->CreateTexture(RTDesc, nullptr, &m_GBuffer.Normal);

    RTDesc.Name      = "GBuffer Depth target";
    RTDesc.BindFlags = BIND_DEPTH_STENCIL | BIND_SHADER_RESOURCE;
    RTDesc.Format    = m_DepthTargetFormat;

    m_pDevice->CreateTexture(RTDesc, nullptr, &m_GBuffer.Depth);

    RTDesc.Name      = "Ray traced shadow & reflection target";
    RTDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;
    RTDesc.Format    = m_RayTracedTexFormat;

    m_pDevice->CreateTexture(RTDesc, nullptr, &m_RayTracedTex);
}

void Tutorial22_HybridRendering::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::SliderFloat3("LightPos", &m_LightPos.x, -100.f, 100.f);

        ImGui::SliderInt("Mode", &m_DrawMode, 0, 8);

        if (ImGui::Button("Reload"))
        {
            m_pImmediateContext->Flush();
            m_pImmediateContext->WaitForIdle();

            m_RasterizationPSO.Release();
            m_RasterizationSRB.Release();
            m_PostProcessPSO.Release();
            m_PostProcessSRB.Release();
            m_RayTracingPSO.Release();
            m_RayTracingSRB.Release();

            RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
            m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);

            CreateRasterizationPSO(pShaderSourceFactory);
            CreatePostProcessPSO(pShaderSourceFactory);
            CreateRayTracingPSO(pShaderSourceFactory);
        }
    }
    ImGui::End();
}

} // namespace Diligent
