/*
 *  Copyright 2019-2024 Diligent Graphics LLC
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

#include <random>

#include "Tutorial04_Instancing.hpp"
#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "ColorConversion.h"
#include "../../Common/src/TexturedCube.hpp"
#include "imgui.h"

#define VOXELIZER_IMPLEMENTATION
#include "voxelizer.h"  // Voxelizer

#include "ufbx/ufbx.h"  // FBX importer

namespace Diligent
{
    float             voxelSize   = 0.010f;
    vx_point_cloud_t* p_voxelMesh = nullptr;

SampleBase* CreateSample()
{
    return new Tutorial04_Instancing();
}

Tutorial04_Instancing::~Tutorial04_Instancing()
{
    // Delete voxelized meshes here!
    vx_point_cloud_free(p_voxelMesh);
}

void Tutorial04_Instancing::CreatePipelineState()
{
    // clang-format off
    // Define vertex shader input layout
    // This tutorial uses two types of input: per-vertex data and per-instance data.
    LayoutElement LayoutElems[] =
    {
        // Per-vertex data - first buffer slot
        // Attribute 0 - vertex position
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        // Attribute 1 - texture coordinates
        LayoutElement{1, 0, 2, VT_FLOAT32, False},
            
        // Per-instance data - second buffer slot
        // We will use four attributes to encode instance-specific 4x4 transformation matrix
        // Attribute 2 - first row
        LayoutElement{2, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 3 - second row
        LayoutElement{3, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 4 - third row
        LayoutElement{4, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 5 - fourth row
        LayoutElement{5, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE}
    };
    // clang-format on

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);

    TexturedCube::CreatePSOInfo CubePsoCI;
    CubePsoCI.pDevice                = m_pDevice;
    CubePsoCI.RTVFormat              = m_pSwapChain->GetDesc().ColorBufferFormat;
    CubePsoCI.DSVFormat              = m_pSwapChain->GetDesc().DepthBufferFormat;
    CubePsoCI.pShaderSourceFactory   = pShaderSourceFactory;
    CubePsoCI.VSFilePath             = "cube_inst.vsh";
    CubePsoCI.PSFilePath             = "cube_inst.psh";
    CubePsoCI.ExtraLayoutElements    = LayoutElems;
    CubePsoCI.NumExtraLayoutElements = _countof(LayoutElems);

    m_pPSO = TexturedCube::CreatePipelineState(CubePsoCI, m_ConvertPSOutputToGamma);

    // Create dynamic uniform buffer that will store our transformation matrix
    // Dynamic buffers can be frequently updated by the CPU
    CreateUniformBuffer(m_pDevice, sizeof(float4x4) * 2, "VS constants CB", &m_VSConstants);

    // Since we did not explicitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
    // never change and are bound directly to the pipeline state object.
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

    // Since we are using mutable variable, we must create a shader resource binding object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    m_pPSO->CreateShaderResourceBinding(&m_SRB, true);
}

void Tutorial04_Instancing::CreateInstanceBuffer()
{
    // Create instance data buffer that will store transformation matrices
    BufferDesc InstBuffDesc;
    InstBuffDesc.Name = "Instance data buffer";
    // Use default usage as this buffer will only be updated when grid size changes
    InstBuffDesc.Usage     = USAGE_DEFAULT;
    InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    InstBuffDesc.Size      = sizeof(float4x4) * p_voxelMesh->nvertices;
    m_pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_InstanceBuffer);
    PopulateInstanceBuffer();
}

void Tutorial04_Instancing::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::Button("Reset Camera"))
        {
            fpc.SetPos({0, 5, 0});
        }

        ImGui::Checkbox("Occlusion Culling", &m_UseOcclusionCulling);
        ImGui::Checkbox("Frustum Culling", &m_UseFrustumCulling);

        ImGui::Text("Voxel count: %d", static_cast<int>(p_voxelMesh->nvertices));
    }    
    ImGui::End();
}

void Tutorial04_Instancing::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    CreatePipelineState();

    // Load textured cube
    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_pDevice, TexturedCube::VERTEX_COMPONENT_FLAG_POS_UV);
    m_CubeIndexBuffer  = TexturedCube::CreateIndexBuffer(m_pDevice);
    m_TextureSRV       = TexturedCube::LoadTexture(m_pDevice, "DGLogo.png")->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    // Set cube texture SRV in the SRB
    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);

    GetPointCloudFromMesh("models/suzanne.fbx");
    CreateInstanceBuffer();
}

void Tutorial04_Instancing::GetPointCloudFromMesh(std::string meshPath)
{
    vx_mesh_t* p_triangleMesh = nullptr;

    //Load model from file
    ufbx_load_opts opts  = {0}; // Optional, pass NULL for defaults
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
    p_voxelMesh = vx_voxelize_pc(p_triangleMesh, voxelSize, voxelSize, voxelSize, 0.001f);

    // Free the triangle mesh and the scene
    vx_mesh_free(p_triangleMesh);
    ufbx_free_scene(scene);
    // Do not vx_mesh_free(pVoxelMesh) yet. Do this in the deinitialization routine instead!
}

void Tutorial04_Instancing::PopulateInstanceBuffer()
{
    // Populate instance data buffer
    std::vector<float4x4> InstanceData(p_voxelMesh->nvertices);

    for (int i = 0; i < p_voxelMesh->nvertices; ++i)
    {
        float4x4 matrix = float4x4::Scale(voxelSize / 2.0f, voxelSize / 2.0f, voxelSize / 2.0f) * 
            float4x4::Translation(p_voxelMesh->vertices[i].x, p_voxelMesh->vertices[i].y, p_voxelMesh->vertices[i].z);
        InstanceData[i] = matrix;
    }

    // Update instance data buffer
    Uint32 DataSize = static_cast<Uint32>(sizeof(InstanceData[0]) * InstanceData.size());
    m_pImmediateContext->UpdateBuffer(m_InstanceBuffer, 0, DataSize, InstanceData.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}


// Render a frame
void Tutorial04_Instancing::Render()
{
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
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
        MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants[0] = m_ViewProjMatrix;
        CBConstants[1] = m_RotationMatrix;
    }

    // Bind vertex, instance and index buffers
    const Uint64 offsets[] = {0, 0};
    IBuffer*     pBuffs[]  = {m_CubeVertexBuffer, m_InstanceBuffer};
    m_pImmediateContext->SetVertexBuffers(0, _countof(pBuffs), pBuffs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set the pipeline state
    m_pImmediateContext->SetPipelineState(m_pPSO);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
    // makes sure that resources are transitioned to required states.
    m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs DrawAttrs;       // This is an indexed draw call
    DrawAttrs.IndexType    = VT_UINT32; // Index type
    DrawAttrs.NumIndices   = 36;
    DrawAttrs.NumInstances = static_cast<Uint32>(p_voxelMesh->nvertices); // The number of instances
    // Verify the state of vertex and index buffers
    DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);
}

void Tutorial04_Instancing::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    fpc.Update(GetInputController(), (float)ElapsedTime);
    UpdateUI();

    // Set cube view matrix
    float4x4 View = fpc.GetViewMatrix();

    // Get pretransform matrix that rotates the scene according the surface orientation
    auto SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    // Get projection matrix adjusted to the current screen orientation
    auto Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

    // Compute view-projection matrix
    m_ViewProjMatrix = View * SrfPreTransform * Proj;

    // Global rotation matrix
    m_RotationMatrix = float4x4::RotationY(static_cast<float>(CurrTime) * 1.0f) * float4x4::RotationX(-static_cast<float>(CurrTime) * 0.25f);
}

} // namespace Diligent
