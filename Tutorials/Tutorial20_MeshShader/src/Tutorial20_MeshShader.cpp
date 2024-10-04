/*
 *  Copyright 2019-2022 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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

#include <array>
#include "Tutorial20_MeshShader.hpp"
#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "ShaderMacroHelper.hpp"
#include "imgui.h"
#include "ImGuiUtils.hpp"
#include "FastRand.hpp"
#include <set>
#include <unordered_set>
#include <d3d12.h>

#define VOXELIZER_IMPLEMENTATION
#include "voxelizer.h"  // Voxelizer

#include "ufbx/ufbx.h"  // FBX importer

namespace Diligent
{

    namespace
    {
    
        #include "../assets/structures.fxh"
#include "../../../../DiligentCore/Graphics/GraphicsEngineD3D12/include/d3dx12_win.h"
        
        struct DrawStatistics
        {
            Uint32 visibleCubes;
            Uint32 visibleOctreeNodes;
        };
        
        static_assert(sizeof(DrawTask) % 16 == 0, "Structure must be 16-byte aligned");
    
    } // namespace

    SampleBase* CreateSample()
    {
        return new Tutorial20_MeshShader();
    }
    
    void Tutorial20_MeshShader::CreateDrawTasks()
    {
        // In this tutorial draw tasks contain:
        //  * cube position in the grid
        //  * cube scale factor
        //  * time that is used for animation and will be updated in the shader.
        // Additionally you can store model transformation matrix, mesh and material IDs, etc.
    
        const int2          GridDim{256, 256};
        FastRandReal<float> Rnd{0, 0.f, 1.f};
    
        std::vector<DrawTask> DrawTasks;
        DrawTasks.resize(static_cast<size_t>(GridDim.x) * static_cast<size_t>(GridDim.y));
    
        for (int y = 0; y < GridDim.y; ++y)
        {
            for (int x = 0; x < GridDim.x; ++x)
            {
                int   idx = x + y * GridDim.x;
                auto& dst = DrawTasks[idx];
    
                dst.BasePosAndScale.x  = static_cast<float>((x - GridDim.x / 2) * 2);
                dst.BasePosAndScale.y  = static_cast<float>((y - GridDim.y / 2) * 2);
                dst.BasePosAndScale.w  = 1.f; // 0.5 .. 1
                dst.RandomValue        = {Rnd(), 0, 0, 0};
            }
        }
    
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Draw tasks buffer";
        BuffDesc.Usage             = USAGE_DEFAULT;
        BuffDesc.BindFlags         = BIND_SHADER_RESOURCE;
        BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
        BuffDesc.ElementByteStride = sizeof(DrawTasks[0]);
        BuffDesc.Size              = sizeof(DrawTasks[0]) * static_cast<Uint32>(DrawTasks.size());
    
        BufferData BufData;
        BufData.pData    = DrawTasks.data();
        BufData.DataSize = BuffDesc.Size;
    
        m_pDevice->CreateBuffer(BuffDesc, &BufData, &m_pDrawTasks);
        VERIFY_EXPR(m_pDrawTasks != nullptr);
    
        m_DrawTaskCount = static_cast<Uint32>(DrawTasks.size());
    }
    
    void Tutorial20_MeshShader::CreateStatisticsBuffer()
    {
        // This buffer is used as an atomic counter in the amplification shader to show
        // how many cubes are rendered with and without frustum culling.
    
        BufferDesc BuffDesc;
        BuffDesc.Name      = "Statistics buffer";
        BuffDesc.Usage     = USAGE_DEFAULT;
        BuffDesc.BindFlags = BIND_UNORDERED_ACCESS;
        BuffDesc.Mode      = BUFFER_MODE_RAW;
        BuffDesc.Size      = sizeof(DrawStatistics);
    
        m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pStatisticsBuffer);
        VERIFY_EXPR(m_pStatisticsBuffer != nullptr);
    
        // Staging buffer is needed to read the data from statistics buffer.
    
        BuffDesc.Name           = "Statistics staging buffer";
        BuffDesc.Usage          = USAGE_STAGING;
        BuffDesc.BindFlags      = BIND_NONE;
        BuffDesc.Mode           = BUFFER_MODE_UNDEFINED;
        BuffDesc.CPUAccessFlags = CPU_ACCESS_READ;
        BuffDesc.Size           = sizeof(DrawStatistics) * m_StatisticsHistorySize;
    
        m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pStatisticsStaging);
        VERIFY_EXPR(m_pStatisticsStaging != nullptr);
    
        FenceDesc FDesc;
        FDesc.Name = "Statistics available";
        m_pDevice->CreateFence(FDesc, &m_pStatisticsAvailable);
    }
    
    void Tutorial20_MeshShader::CreateConstantsBuffer()
    {
        BufferDesc BuffDesc;
        BuffDesc.Name           = "Constant buffer";
        BuffDesc.Usage          = USAGE_DYNAMIC;
        BuffDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        BuffDesc.Size           = sizeof(Constants);
    
        m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pConstants);
        VERIFY_EXPR(m_pConstants != nullptr);
    }
    
    void Tutorial20_MeshShader::LoadTexture()
    {
        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = true;
        RefCntAutoPtr<ITexture> pTex;
        CreateTextureFromFile("DGLogo.png", loadInfo, m_pDevice, &pTex);
        VERIFY_EXPR(pTex != nullptr);
    
        m_CubeTextureSRV = pTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        VERIFY_EXPR(m_CubeTextureSRV != nullptr);
    }
    
     vx_point_cloud_t* p_voxelMesh = nullptr;
     float      voxelSize   = 2.f;
    
    Tutorial20_MeshShader::~Tutorial20_MeshShader()
    {
        delete p_occlusionOctreeRoot;
        // Delete voxelized meshes here!
        vx_point_cloud_free(p_voxelMesh);
    }
    
    void Tutorial20_MeshShader::GetPointCloudFromMesh(std::string meshPath)
    {
        vx_mesh_t* p_triangleMesh = nullptr;
    
        //Load model from file
        ufbx_load_opts opts = {0}; // Optional, pass NULL for defaults
        ufbx_scene*    scene = ufbx_load_file(meshPath.c_str(), &opts, NULL);
        
        assert(scene);

        ufbx_node* node = scene->nodes.data[0];
    
        int nVertices = static_cast<int>(node->children[0]->mesh->num_vertices);
        int nIndices  = static_cast<int>(node->children[0]->mesh->num_indices);
    
        p_triangleMesh = vx_mesh_alloc(nVertices, nIndices);
    
        // Copy value by value (try memcpy later).
        for (size_t i = 0; i < nVertices; ++i)
        {
            p_triangleMesh->vertices[i].x = (float)node->children[0]->mesh->vertices[i].x;
            p_triangleMesh->vertices[i].y = (float)node->children[0]->mesh->vertices[i].y;
            p_triangleMesh->vertices[i].z = (float)node->children[0]->mesh->vertices[i].z;
        }
    
        for (size_t i = 0; i < nIndices; ++i)
        {
            p_triangleMesh->indices[i] = node->children[0]->mesh->vertex_indices[i];
        }
    
        // Run voxelization
        p_voxelMesh = vx_voxelize_pc(p_triangleMesh, voxelSize, voxelSize, voxelSize, 0.1f);
    
        // Free the triangle mesh and the scene
        vx_mesh_free(p_triangleMesh);
        ufbx_free_scene(scene);
        // Do not vx_mesh_free(pVoxelMesh) yet. Do this in the deinitialization routine instead!
    }
    
    struct Vec4
    {
        float x, y, z, w;

        Vec4(Diligent::float4& other)
        {
            x = other.x;
            y = other.y;
            z = other.z;
            w = other.w;
        }

        bool operator<(const Vec4& other) const
        {
            if (x != other.x)
                return x < other.x;

            if (y != other.y)
                return y < other.y;

            if (z != other.z)
                return z < other.z;


            return false;
        }

        bool operator==(const Vec4& other) const
        {
            return x == other.x && y == other.y && z == other.z && w == other.w;
        }
    };

    void Tutorial20_MeshShader::CreateDrawTasksFromLoadedMesh()
    {
        FastRandReal<float> Rnd{0, 0.f, 1.f};
    
        // Draw Tasks
        std::vector<DrawTask> DrawTasks;
        unsigned long long    alignedDrawTaskSize = p_voxelMesh->nvertices + (32 - (p_voxelMesh->nvertices % 32));
        DrawTasks.resize(alignedDrawTaskSize);
       
        DirectX::XMFLOAT3 minMeshDimensions{10000.f, 10000.f, 10000.f};
        DirectX::XMFLOAT3 maxMeshDimensions{-10000.f, -10000.f, -10000.f};

        // Populate DrawTasks
        for (int i = 0; i < p_voxelMesh->nvertices; ++i)
        {
            DrawTask& dst = DrawTasks[i];

            dst.BasePosAndScale.x   = p_voxelMesh->vertices[i].x;
            dst.BasePosAndScale.y   = p_voxelMesh->vertices[i].y;
            dst.BasePosAndScale.z   = p_voxelMesh->vertices[i].z;
            dst.BasePosAndScale.w   = voxelSize / 2.f; // 0.5 .. 1 -> divide by 2 for size from middle point
            
            dst.RandomValue.x       = Rnd();
            dst.RandomValue.y       = static_cast<float>(alignedDrawTaskSize);
            dst.RandomValue.z       = static_cast<float>(alignedDrawTaskSize - p_voxelMesh->nvertices);
            dst.RandomValue.w       = 0;

            minMeshDimensions.x = dst.BasePosAndScale.x < minMeshDimensions.x ? dst.BasePosAndScale.x : minMeshDimensions.x;
            minMeshDimensions.y = dst.BasePosAndScale.y < minMeshDimensions.y ? dst.BasePosAndScale.y : minMeshDimensions.y;
            minMeshDimensions.z = dst.BasePosAndScale.z < minMeshDimensions.z ? dst.BasePosAndScale.z : minMeshDimensions.z;

            maxMeshDimensions.x = dst.BasePosAndScale.x > maxMeshDimensions.x ? dst.BasePosAndScale.x : maxMeshDimensions.x;
            maxMeshDimensions.y = dst.BasePosAndScale.y > maxMeshDimensions.y ? dst.BasePosAndScale.y : maxMeshDimensions.y;
            maxMeshDimensions.z = dst.BasePosAndScale.z > maxMeshDimensions.z ? dst.BasePosAndScale.z : maxMeshDimensions.z;
        }
    
        // Add some spatial padding to explicitly include every voxel!
        minMeshDimensions.x -= voxelSize * 2.f; minMeshDimensions.y -= voxelSize * 2.f; minMeshDimensions.z -= voxelSize * 2.f;
        maxMeshDimensions.x += voxelSize * 2.f; maxMeshDimensions.y += voxelSize * 2.f; maxMeshDimensions.z += voxelSize * 2.f;

        //Octree
        AABB world                        = {minMeshDimensions, maxMeshDimensions};
        DebugInfo getGridIndicesDebugInfo;
        DebugInfo insertOctreeDebugInfo;
        p_occlusionOctreeRoot = new OctreeNode<VoxelOC::DrawTask>(world, &getGridIndicesDebugInfo, &insertOctreeDebugInfo);
    
        const DirectX::XMVECTOR voxelSizeOffset = {voxelSize, voxelSize, voxelSize};

        {
            // Copy draw tasks to global object buffer for AABB calculations
            for (int i = 0; i < p_voxelMesh->nvertices; ++i)
            {
                VoxelOC::DrawTask task{};

                task.BasePosAndScale.x = DrawTasks[i].BasePosAndScale.x;
                task.BasePosAndScale.y = DrawTasks[i].BasePosAndScale.y;
                task.BasePosAndScale.z = DrawTasks[i].BasePosAndScale.z;
                task.BasePosAndScale.w = DrawTasks[i].BasePosAndScale.w;

                task.RandomValue.x = DrawTasks[i].RandomValue.x;
                task.RandomValue.y = DrawTasks[i].RandomValue.y;
                task.RandomValue.z = DrawTasks[i].RandomValue.z;
                task.RandomValue.w = DrawTasks[i].RandomValue.w;

                ObjectBuffer.push_back(task);
            }

            // Calculate bounds and add them to octree
            for (int i = 0; i < p_voxelMesh->nvertices; ++i)
            {
                // Calculate bounds
                DirectX::XMVECTOR minBoundVec = DirectX::XMVectorSubtract(
                    {DrawTasks[i].BasePosAndScale.x,
                     DrawTasks[i].BasePosAndScale.y,
                     DrawTasks[i].BasePosAndScale.z},
                    voxelSizeOffset);
                DirectX::XMVECTOR maxBoundVec = DirectX::XMVectorAdd(
                    {DrawTasks[i].BasePosAndScale.x,
                     DrawTasks[i].BasePosAndScale.y,
                     DrawTasks[i].BasePosAndScale.z},
                    voxelSizeOffset);

                DirectX::XMFLOAT3 minBound;
                DirectX::XMFLOAT3 maxBound;

                DirectX::XMStoreFloat3(&minBound, minBoundVec);
                DirectX::XMStoreFloat3(&maxBound, maxBoundVec);

                p_occlusionOctreeRoot->InsertObject(i, {minBound, maxBound});
            }
        }


        // Now create a buffer where objects in one node are stored contigously (start index + length)
        // Create octree node meta data for the GPU and set buffers occordingly.

        // Maybe use grid with bitmask instead of explicit positions?
        // -> Voxel culling using camera position and grid dimensions to calculate indices and compare bit mask
        
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Draw tasks buffer";
        BuffDesc.Usage             = USAGE_DEFAULT;
        BuffDesc.BindFlags         = BIND_SHADER_RESOURCE;
        BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
        BuffDesc.ElementByteStride = sizeof(DrawTasks[0]);
        BuffDesc.Size              = sizeof(DrawTasks[0]) * static_cast<Uint32>(DrawTasks.size());
    
        BufferData BufData;
        BufData.pData    = DrawTasks.data();
        BufData.DataSize = BuffDesc.Size;
    
        m_pDevice->CreateBuffer(BuffDesc, &BufData, &m_pDrawTasks);
        VERIFY_EXPR(m_pDrawTasks != nullptr);
        
        std::vector<int> sortedNodeBuffer{};
        std::vector<char> duplicateBuffer{};
        std::vector<VoxelOC::GPUOctreeNode> octreeNodeBuffer{};

        sortedNodeBuffer.reserve(DrawTasks.size());
        octreeNodeBuffer.reserve(static_cast<int>(DrawTasks.size() / 2.0f));
        duplicateBuffer.resize(DrawTasks.size());

        memset(&duplicateBuffer[0], 0, duplicateBuffer.size() * sizeof(char));

        p_occlusionOctreeRoot->GetAllGridIndices(sortedNodeBuffer, duplicateBuffer, octreeNodeBuffer);

        CreateSortedIndexBuffer(sortedNodeBuffer);
        CreateGPUOctreeNodeBuffer(octreeNodeBuffer);

        // Set draw task count and padding
        m_DrawTaskPadding = static_cast<Uint32>(octreeNodeBuffer.size());
        octreeNodeBuffer.resize(octreeNodeBuffer.size() + 32 - (octreeNodeBuffer.size() % 32));
        m_DrawTaskPadding = static_cast<Uint32>(octreeNodeBuffer.size() - m_DrawTaskPadding);

        m_DrawTaskCount = static_cast<Uint32>(octreeNodeBuffer.size());

        for (auto& task : DrawTasks)
        {
            task.RandomValue.x = static_cast<float>(m_DrawTaskCount);
            task.RandomValue.y = static_cast<float>(m_DrawTaskPadding);
        }
    }

    void Tutorial20_MeshShader::CreateSortedIndexBuffer(std::vector<int>& sortedNodeBuffer)
    {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Grid index buffer";
        BuffDesc.Usage             = USAGE_DEFAULT;
        BuffDesc.BindFlags         = BIND_SHADER_RESOURCE;
        BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
        BuffDesc.ElementByteStride = sizeof(sortedNodeBuffer[0]);
        BuffDesc.Size              = sizeof(sortedNodeBuffer[0]) * static_cast<Uint32>(sortedNodeBuffer.size());

        BufferData BufData;
        BufData.pData    = sortedNodeBuffer.data();
        BufData.DataSize = BuffDesc.Size;

        m_pDevice->CreateBuffer(BuffDesc, &BufData, &m_pGridIndices);
        VERIFY_EXPR(m_pGridIndices != nullptr);
    }

    void Tutorial20_MeshShader::CreateGPUOctreeNodeBuffer(std::vector<VoxelOC::GPUOctreeNode>& octreeNodeBuffer)
    {
        BufferDesc BuffDesc;
        BuffDesc.Name              = "Octree node buffer";
        BuffDesc.Usage             = USAGE_DEFAULT;
        BuffDesc.BindFlags         = BIND_SHADER_RESOURCE;
        BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
        BuffDesc.ElementByteStride = sizeof(octreeNodeBuffer[0]);
        BuffDesc.Size              = sizeof(octreeNodeBuffer[0]) * static_cast<Uint32>(octreeNodeBuffer.size());

        BufferData BufData;
        BufData.pData    = octreeNodeBuffer.data();
        BufData.DataSize = BuffDesc.Size;

        m_pDevice->CreateBuffer(BuffDesc, &BufData, &m_pOctreeNodes);
        VERIFY_EXPR(m_pOctreeNodes != nullptr);
    }
    
    void Tutorial20_MeshShader::CreatePipelineState()
    {
        // Pipeline state object encompasses configuration of all GPU stages
    
        GraphicsPipelineStateCreateInfo PSOCreateInfo;
        PipelineStateDesc&              PSODesc = PSOCreateInfo.PSODesc;
    
        PSODesc.Name = "Mesh shader";
    
        PSODesc.PipelineType                                                = PIPELINE_TYPE_MESH;
        PSOCreateInfo.GraphicsPipeline.NumRenderTargets                     = 1;
        PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                        = m_pSwapChain->GetDesc().ColorBufferFormat;
        PSOCreateInfo.GraphicsPipeline.DSVFormat                            = m_pSwapChain->GetDesc().DepthBufferFormat;
        PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_BACK;
        PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FillMode              = FILL_MODE_SOLID; 
        PSOCreateInfo.GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = False;
        PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable         = True;
    
        // Topology is defined in the mesh shader, this value is not used.
        PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_UNDEFINED;
    
        // Define variable type that will be used by default.
        PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
    
        ShaderCreateInfo ShaderCI;
        ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    
        // For Direct3D12 we must use the new DXIL compiler that supports mesh shaders.
        ShaderCI.ShaderCompiler = SHADER_COMPILER_DXC;
    
        ShaderCI.Desc.UseCombinedTextureSamplers = true;
    
        // Create a shader source stream factory to load shaders from files.
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
        ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    
        ShaderMacroHelper Macros;
        Macros.AddShaderMacro("GROUP_SIZE", ASGroupSize);
    
        ShaderCI.Macros = Macros;
    
        RefCntAutoPtr<IShader> pAS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_AMPLIFICATION;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "Mesh shader - AS";
            ShaderCI.FilePath        = "cube_ash.hlsl";
    
            m_pDevice->CreateShader(ShaderCI, &pAS);
            VERIFY_EXPR(pAS != nullptr);
        }
    
        RefCntAutoPtr<IShader> pMS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_MESH;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "Mesh shader - MS";
            ShaderCI.FilePath        = "cube_msh.hlsl";
    
            m_pDevice->CreateShader(ShaderCI, &pMS);
            VERIFY_EXPR(pMS != nullptr);
        }
    
        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "Mesh shader - PS";
            ShaderCI.FilePath        = "cube_psh.hlsl";
    
            m_pDevice->CreateShader(ShaderCI, &pPS);
            VERIFY_EXPR(pPS != nullptr);
        }
    
        // clang-format off
        // Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
        SamplerDesc SamLinearClampDesc
        {
            FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, 
            TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
        };
        ImmutableSamplerDesc ImtblSamplers[] = 
        {
            {SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}
        };
        // clang-format on
        PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
        PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);
    
        PSOCreateInfo.pAS = pAS;
        PSOCreateInfo.pMS = pMS;
        PSOCreateInfo.pPS = pPS;
    
        m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPSO);
        VERIFY_EXPR(m_pPSO != nullptr);
    
        m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);
        VERIFY_EXPR(m_pSRB != nullptr);
    
        if (m_pSRB->GetVariableByName(SHADER_TYPE_AMPLIFICATION, "Statistics"))
            m_pSRB->GetVariableByName(SHADER_TYPE_AMPLIFICATION, "Statistics")->Set(m_pStatisticsBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
        
        if (m_pSRB->GetVariableByName(SHADER_TYPE_AMPLIFICATION, "DrawTasks")) 
            m_pSRB->GetVariableByName(SHADER_TYPE_AMPLIFICATION, "DrawTasks")->Set(m_pDrawTasks->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
        
        if (m_pSRB->GetVariableByName(SHADER_TYPE_AMPLIFICATION, "GridIndices"))
            m_pSRB->GetVariableByName(SHADER_TYPE_AMPLIFICATION, "GridIndices")->Set(m_pGridIndices->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));

       if (m_pSRB->GetVariableByName(SHADER_TYPE_AMPLIFICATION, "OctreeNodes"))
            m_pSRB->GetVariableByName(SHADER_TYPE_AMPLIFICATION, "OctreeNodes")->Set(m_pOctreeNodes->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
        
        if (m_pSRB->GetVariableByName(SHADER_TYPE_AMPLIFICATION, "cbConstants"))
            m_pSRB->GetVariableByName(SHADER_TYPE_AMPLIFICATION, "cbConstants")->Set(m_pConstants);
        
        if (m_pSRB->GetVariableByName(SHADER_TYPE_MESH, "cbConstants"))
            m_pSRB->GetVariableByName(SHADER_TYPE_MESH, "cbConstants")->Set(m_pConstants);
    
        if (m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture"))
            m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_CubeTextureSRV);
    }

    void Tutorial20_MeshShader::CreateDepthBuffers()
    {
        /*
            |-------------------------|
            |   Main Depth Buffer     |-------------> First Prepass: Render best occluders into main depth buffer
            |-------------------------|

            |-------------------------|
            |  Main Depth Buffer Cpy  |-------------> First Prepass: Copy main depth buffer and create HiZ
            |-------------------------|
            
            |-------------------------|
            | Prev Frame Depth Buffer |-------------> EOF: Copy last frames depth buffer! 
            |-------------------------|

        */

        if (m_pDepthBufferCpy.RawPtr() != nullptr)
            m_pDepthBufferCpy.Release();
        
        if (m_pDepthBufferCpySRV.RawPtr() != nullptr)
            m_pDepthBufferCpySRV.Release();
        
        if (m_pDepthBufferCpyUAV.RawPtr() != nullptr)
            m_pDepthBufferCpyUAV.Release();

        if (m_pPrevDepthBuffer.RawPtr() != nullptr)
            m_pPrevDepthBuffer.Release();
        
        if (m_pPrevDepthBufferSRV.RawPtr() != nullptr)
            m_pPrevDepthBufferSRV.Release();

        ITextureView* pDepthBufferDSV = m_pSwapChain->GetDepthBufferDSV();
        ITexture*     pDepthTexture   = pDepthBufferDSV->GetTexture();
        const auto&   DepthTexDesc    = pDepthTexture->GetDesc();

        // Create a separate texture for occlusion culling computations
        TextureDesc OcclusionTexDesc = DepthTexDesc; // Copy the description of the depth texture
        OcclusionTexDesc.Name        = "Occlusion Computation Texture";
        OcclusionTexDesc.Usage       = USAGE_DEFAULT;
        OcclusionTexDesc.BindFlags   = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;
        
        // If the original depth buffer is using a depth-specific format, we need to use a compatible color format
        if (OcclusionTexDesc.Format == TEX_FORMAT_D32_FLOAT)
            OcclusionTexDesc.Format = TEX_FORMAT_R32_FLOAT;
        else if (OcclusionTexDesc.Format == TEX_FORMAT_D24_UNORM_S8_UINT)
            OcclusionTexDesc.Format = TEX_FORMAT_R24_UNORM_X8_TYPELESS;

        m_pDevice->CreateTexture(OcclusionTexDesc, nullptr, &m_pDepthBufferCpy);
        VERIFY_EXPR(m_pDepthBufferCpy != nullptr);

        // Create SRV for the occlusion texture
        TextureViewDesc             OcclusionSRVDesc;
        OcclusionSRVDesc.ViewType = TEXTURE_VIEW_SHADER_RESOURCE;
        m_pDepthBufferCpy->CreateView(OcclusionSRVDesc, &m_pDepthBufferCpySRV);
        VERIFY_EXPR(m_pDepthBufferCpySRV != nullptr);

        // Create UAV for the occlusion texture
        TextureViewDesc             OcclusionUAVDesc;
        OcclusionUAVDesc.ViewType = TEXTURE_VIEW_UNORDERED_ACCESS;
        m_pDepthBufferCpy->CreateView(OcclusionUAVDesc, &m_pDepthBufferCpyUAV);
        VERIFY_EXPR(m_pDepthBufferCpyUAV != nullptr);

        // ------------- Previous frames depth buffer ---------------

        // New code for the previous frame's depth buffer
        TextureDesc PrevDepthTexDesc = DepthTexDesc;
        PrevDepthTexDesc.Name        = "Previous Frame Depth Texture";
        PrevDepthTexDesc.Usage       = USAGE_DEFAULT;
        PrevDepthTexDesc.BindFlags   = BIND_SHADER_RESOURCE;

        // Use a color format compatible with the depth format
        if (PrevDepthTexDesc.Format == TEX_FORMAT_D32_FLOAT)
            PrevDepthTexDesc.Format = TEX_FORMAT_R32_FLOAT;
        else if (PrevDepthTexDesc.Format == TEX_FORMAT_D24_UNORM_S8_UINT)
            PrevDepthTexDesc.Format = TEX_FORMAT_R24_UNORM_X8_TYPELESS;

        m_pDevice->CreateTexture(PrevDepthTexDesc, nullptr, &m_pPrevDepthBuffer);
        VERIFY_EXPR(m_pPrevDepthBuffer != nullptr);

        // Create SRV for the previous depth buffer
        TextureViewDesc PrevDepthSRVDesc;
        PrevDepthSRVDesc.ViewType = TEXTURE_VIEW_SHADER_RESOURCE;
        m_pPrevDepthBuffer->CreateView(PrevDepthSRVDesc, &m_pPrevDepthBufferSRV);
        VERIFY_EXPR(m_pPrevDepthBufferSRV != nullptr);

    }
        
    void Tutorial20_MeshShader::UpdateUI()
    {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Checkbox("Frustum culling", &m_FrustumCulling);
            ImGui::Checkbox("Occlusion culling", &m_OcclusionCulling);
            ImGui::Checkbox("MS Debug Visualization", &m_MSDebugViz);
            ImGui::Checkbox("Octree Debug Visualization", &m_OTDebugViz);
            ImGui::Checkbox("Syncronize Camera Position", &m_SyncCamPosition);

            if (ImGui::Button("Reset Camera"))
            {
                fpc.SetPos({0, 5, 0});
            }
            ImGui::Text("Visible cubes: %d", m_VisibleCubes);
            ImGui::Text("Visible octree nodes: %d", m_VisibleOTNodes);

        }
        ImGui::End();
    }

    void Tutorial20_MeshShader::PreWindowResize()
    {
        
    }

    void Tutorial20_MeshShader::WindowResize(Uint32 Width, Uint32 Height)
    {
        CreateDepthBuffers();
    }
    
    void Tutorial20_MeshShader::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
    {
        SampleBase::ModifyEngineInitInfo(Attribs);
    
        Attribs.EngineCI.Features.MeshShaders = DEVICE_FEATURE_STATE_ENABLED;
    }
    
    void Tutorial20_MeshShader::Initialize(const SampleInitInfo& InitInfo)
    {
        SampleBase::Initialize(InitInfo);
    
        fpc.SetMoveSpeed(25.f);
    
        LoadTexture();
        GetPointCloudFromMesh("models/suzanne.fbx");
        //CreateDrawTasks();
        CreateDrawTasksFromLoadedMesh();
        CreateStatisticsBuffer();
        CreateConstantsBuffer();
        CreatePipelineState();
        CreateDepthBuffers();
    }
    
    // Render a frame
    void Tutorial20_MeshShader::Render()
    {
        
        auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
        auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
        // Clear the back buffer and depth buffer
        const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
        m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    

        // Reset statistics
        DrawStatistics stats;
        std::memset(&stats, 0, sizeof(stats));
        m_pImmediateContext->UpdateBuffer(m_pStatisticsBuffer, 0, sizeof(stats), &stats, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
        m_pImmediateContext->SetPipelineState(m_pPSO);
        m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
        // Draw best occluders to depth buffer


        {
            // Map the buffer and write current view, view-projection matrix and other constants.
            MapHelper<Constants> CBConstants(m_pImmediateContext, m_pConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            CBConstants->ViewMat        = m_ViewMatrix.Transpose();
            CBConstants->ViewProjMat    = m_ViewProjMatrix.Transpose();
            CBConstants->CoTanHalfFov   = m_LodScale * m_CoTanHalfFov;
            CBConstants->FrustumCulling = m_FrustumCulling ? 1 : 0;
            CBConstants->OcclusionCulling = m_OcclusionCulling ? 1 : 0;
            CBConstants->MSDebugViz     = m_MSDebugViz ? 1.0f : 0.0f;
            CBConstants->OctreeDebugViz   = m_OTDebugViz ? 1.0f : 0.0f;
    
            // Calculate frustum planes from view-projection matrix.
            if (m_SyncCamPosition)
                ExtractViewFrustumPlanesFromMatrix(m_ViewProjMatrix, Frustum, false);
    
            // Each frustum plane must be normalized.
            for (uint i = 0; i < _countof(CBConstants->Frustum); ++i)
            {
                Plane3D plane  = Frustum.GetPlane(static_cast<ViewFrustum::PLANE_IDX>(i));
                float   invlen = 1.0f / length(plane.Normal);
                plane.Normal *= invlen;
                plane.Distance *= invlen;
    
                CBConstants->Frustum[i] = plane;
            }
        }
    
        // Amplification shader executes 32 threads per group and the task count must be aligned to 32
        // to prevent loss of tasks or access outside of the data array.
        VERIFY_EXPR(m_DrawTaskCount % ASGroupSize == 0);
    
        //DrawMeshAttribs drawAttrs{m_DrawTaskCount / ASGroupSize, DRAW_FLAG_VERIFY_ALL};
        DrawMeshAttribs drawAttrs{m_DrawTaskCount, DRAW_FLAG_VERIFY_ALL};
        m_pImmediateContext->DrawMesh(drawAttrs);
    
        // Copy statistics to staging buffer
        {
            m_VisibleCubes = 0;
    
            m_pImmediateContext->CopyBuffer(m_pStatisticsBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
                                            m_pStatisticsStaging, static_cast<Uint32>(m_FrameId % m_StatisticsHistorySize) * sizeof(DrawStatistics), sizeof(DrawStatistics),
                                            RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
            // We should use synchronizations to safely access the mapped memory.
            m_pImmediateContext->EnqueueSignal(m_pStatisticsAvailable, m_FrameId);
    
            // Read statistics from previous frame.
            Uint64 AvailableFrameId = m_pStatisticsAvailable->GetCompletedValue();
    
            // Synchronize
            if (m_FrameId - AvailableFrameId > m_StatisticsHistorySize)
            {
                // In theory we should never get here as we wait for more than enough
                // frames.
                AvailableFrameId = m_FrameId - m_StatisticsHistorySize;
                m_pStatisticsAvailable->Wait(AvailableFrameId);
            }
    
            // Read the staging data
            if (AvailableFrameId > 0)
            {
                MapHelper<DrawStatistics> StagingData(m_pImmediateContext, m_pStatisticsStaging, MAP_READ, MAP_FLAG_DO_NOT_WAIT);
                if (StagingData)
                {
                    m_VisibleCubes = StagingData[AvailableFrameId % m_StatisticsHistorySize].visibleCubes;
                    m_VisibleOTNodes = StagingData[AvailableFrameId % m_StatisticsHistorySize].visibleOctreeNodes;
                }
            }
    
            // Unset render targets
            m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_NONE);

            // Store depth buffer of this frame in depth buffer copy resource!
            CopyTextureAttribs storeDepthBufAttribs{};
            storeDepthBufAttribs.pSrcTexture = m_pSwapChain->GetDepthBufferDSV()->GetTexture();
            storeDepthBufAttribs.pDstTexture = m_pDepthBufferCpy;
            storeDepthBufAttribs.SrcTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
            storeDepthBufAttribs.DstTextureTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

            m_pImmediateContext->CopyTexture(storeDepthBufAttribs);

            ++m_FrameId;
        }
    }
    
    void Tutorial20_MeshShader::Update(double CurrTime, double ElapsedTime)
    {
        SampleBase::Update(CurrTime, ElapsedTime);
        UpdateUI();
    
        fpc.Update(GetInputController(), (float)ElapsedTime);
    
        // Set camera position
        float4x4 View = fpc.GetViewMatrix();
    
        // Get pretransform matrix that rotates the scene according the surface orientation
        auto SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});
    
        // Get projection matrix adjusted to the current screen orientation
        auto Proj = GetAdjustedProjectionMatrix(m_FOV, 0.01f, 1000.f);
    
        // Compute view and view-projection matrices
        m_ViewMatrix = View * SrfPreTransform;
        m_ViewProjMatrix = m_ViewMatrix * Proj;
    }

} // namespace Diligent
