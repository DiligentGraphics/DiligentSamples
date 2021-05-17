/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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
#include "AdvancedMath.hpp"
#include "../../Common/src/TexturedCube.hpp"

namespace Diligent
{
namespace
{

#include "../assets/structures.fxh"

struct DrawStatistics
{
    Uint32 visibleCubes;
};

static_assert(sizeof(DrawTask) % 16 == 0, "Structure must be 16-byte aligned");

} // namespace

SampleBase* CreateSample()
{
    return new Tutorial20_MeshShader();
}

void Tutorial20_MeshShader::CreateCube()
{
    // Pack float3 positions into float4 vectors
    std::array<float4, TexturedCube::NumVertices> CubePos{};
    for (Uint32 v = 0; v < TexturedCube::NumVertices; ++v)
        CubePos[v] = TexturedCube::Positions[v];

    // Pack float2 texcoords into float4 vectors
    std::array<float4, TexturedCube::NumVertices> CubeUV{};
    for (Uint32 v = 0; v < TexturedCube::NumVertices; ++v)
    {
        const auto& UV{TexturedCube::Texcoords[v]};
        CubeUV[v] = {UV.x, UV.y, 0, 0};
    }

    // Pack each triangle indices into uint4
    std::array<uint4, TexturedCube::NumIndices / 3> Indices{};
    for (Uint32 tri = 0; tri < Indices.size(); ++tri)
    {
        const auto* src_ind{&TexturedCube::Indices[tri * 3]};
        Indices[tri] = {src_ind[0], src_ind[1], src_ind[2], 0};
    }
    CubeData Data;

    // radius of circumscribed sphere = (edge_length * sqrt(3) / 2)
    Data.SphereRadius = float4{length(CubePos[0] - CubePos[1]) * std::sqrt(3.0f) * 0.5f, 0, 0, 0};

    std::memcpy(Data.Positions, CubePos.data(), CubePos.size() * sizeof(CubePos[0]));
    std::memcpy(Data.UVs, CubeUV.data(), CubeUV.size() * sizeof(CubeUV[0]));
    std::memcpy(Data.Indices, Indices.data(), Indices.size() * sizeof(Indices[0]));

    BufferDesc BuffDesc;
    BuffDesc.Name          = "Cube vertex & index buffer";
    BuffDesc.Usage         = USAGE_IMMUTABLE;
    BuffDesc.BindFlags     = BIND_UNIFORM_BUFFER;
    BuffDesc.uiSizeInBytes = sizeof(Data);

    BufferData BufData;
    BufData.pData    = &Data;
    BufData.DataSize = sizeof(Data);

    m_pDevice->CreateBuffer(BuffDesc, &BufData, &m_CubeBuffer);
    VERIFY_EXPR(m_CubeBuffer != nullptr);
}

void Tutorial20_MeshShader::CreateDrawTasks()
{
    // In this tutorial draw tasks contain:
    //  * cube position in the grid
    //  * cube scale factor
    //  * time that is used for animation and will be updated in the shader.
    // Additionally you can store model transformation matrix, mesh and material IDs, etc.

    const int2          GridDim{128, 128};
    FastRandReal<float> Rnd{0, 0.f, 1.f};

    std::vector<DrawTask> DrawTasks;
    DrawTasks.resize(GridDim.x * GridDim.y);

    for (int y = 0; y < GridDim.y; ++y)
    {
        for (int x = 0; x < GridDim.x; ++x)
        {
            int   idx = x + y * GridDim.x;
            auto& dst = DrawTasks[idx];

            dst.BasePos.x  = (x - GridDim.x / 2) * 4.f + (Rnd() * 2.f - 1.f);
            dst.BasePos.y  = (y - GridDim.y / 2) * 4.f + (Rnd() * 2.f - 1.f);
            dst.Scale      = Rnd() * 0.5f + 0.5f; // 0.5 .. 1
            dst.TimeOffset = Rnd() * PI_F;
        }
    }

    BufferDesc BuffDesc;
    BuffDesc.Name              = "Draw tasks buffer";
    BuffDesc.Usage             = USAGE_DEFAULT;
    BuffDesc.BindFlags         = BIND_SHADER_RESOURCE;
    BuffDesc.Mode              = BUFFER_MODE_STRUCTURED;
    BuffDesc.ElementByteStride = sizeof(DrawTasks[0]);
    BuffDesc.uiSizeInBytes     = sizeof(DrawTasks[0]) * static_cast<Uint32>(DrawTasks.size());

    BufferData BufData;
    BufData.pData    = DrawTasks.data();
    BufData.DataSize = BuffDesc.uiSizeInBytes;

    m_pDevice->CreateBuffer(BuffDesc, &BufData, &m_pDrawTasks);
    VERIFY_EXPR(m_pDrawTasks != nullptr);

    m_DrawTaskCount = static_cast<Uint32>(DrawTasks.size());
}

void Tutorial20_MeshShader::CreateStatisticsBuffer()
{
    // This buffer is used as an atomic counter in the amplification shader to show
    // how many cubes are rendered with and without frustum culling.

    BufferDesc BuffDesc;
    BuffDesc.Name          = "Statistics buffer";
    BuffDesc.Usage         = USAGE_DEFAULT;
    BuffDesc.BindFlags     = BIND_UNORDERED_ACCESS;
    BuffDesc.Mode          = BUFFER_MODE_RAW;
    BuffDesc.uiSizeInBytes = sizeof(DrawStatistics);

    m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pStatisticsBuffer);
    VERIFY_EXPR(m_pStatisticsBuffer != nullptr);

    // Staging buffer is needed to read the data from statistics buffer.

    BuffDesc.Name           = "Statistics staging buffer";
    BuffDesc.Usage          = USAGE_STAGING;
    BuffDesc.BindFlags      = BIND_NONE;
    BuffDesc.Mode           = BUFFER_MODE_UNDEFINED;
    BuffDesc.CPUAccessFlags = CPU_ACCESS_READ;
    BuffDesc.uiSizeInBytes  = sizeof(DrawStatistics) * m_StatisticsHistorySize;

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
    BuffDesc.uiSizeInBytes  = sizeof(Constants);

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

    ShaderCI.UseCombinedTextureSamplers = true;

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
        ShaderCI.FilePath        = "cube.ash";

        m_pDevice->CreateShader(ShaderCI, &pAS);
        VERIFY_EXPR(pAS != nullptr);
    }

    RefCntAutoPtr<IShader> pMS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_MESH;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Mesh shader - MS";
        ShaderCI.FilePath        = "cube.msh";

        m_pDevice->CreateShader(ShaderCI, &pMS);
        VERIFY_EXPR(pMS != nullptr);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Mesh shader - PS";
        ShaderCI.FilePath        = "cube.psh";

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

    m_pSRB->GetVariableByName(SHADER_TYPE_AMPLIFICATION, "Statistics")->Set(m_pStatisticsBuffer->GetDefaultView(BUFFER_VIEW_UNORDERED_ACCESS));
    m_pSRB->GetVariableByName(SHADER_TYPE_AMPLIFICATION, "DrawTasks")->Set(m_pDrawTasks->GetDefaultView(BUFFER_VIEW_SHADER_RESOURCE));
    m_pSRB->GetVariableByName(SHADER_TYPE_AMPLIFICATION, "cbCubeData")->Set(m_CubeBuffer);
    m_pSRB->GetVariableByName(SHADER_TYPE_AMPLIFICATION, "cbConstants")->Set(m_pConstants);
    m_pSRB->GetVariableByName(SHADER_TYPE_MESH, "cbCubeData")->Set(m_CubeBuffer);
    m_pSRB->GetVariableByName(SHADER_TYPE_MESH, "cbConstants")->Set(m_pConstants);
    m_pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_CubeTextureSRV);
}

void Tutorial20_MeshShader::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Checkbox("Animate", &m_Animate);
        ImGui::Checkbox("Frustum culling", &m_FrustumCulling);
        ImGui::SliderFloat("LOD scale", &m_LodScale, 1.f, 8.f);
        ImGui::SliderFloat("Camera height", &m_CameraHeight, 5.0f, 100.0f);
        ImGui::Text("Visible cubes: %d", m_VisibleCubes);
    }
    ImGui::End();
}

void Tutorial20_MeshShader::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);

    Attribs.EngineCI.Features.MeshShaders = DEVICE_FEATURE_STATE_ENABLED;
}

void Tutorial20_MeshShader::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    LoadTexture();
    CreateCube();
    CreateDrawTasks();
    CreateStatisticsBuffer();
    CreateConstantsBuffer();
    CreatePipelineState();
}

// Render a frame
void Tutorial20_MeshShader::Render()
{
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
    // Clear the back buffer
    const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Reset statistics
    DrawStatistics stats;
    std::memset(&stats, 0, sizeof(stats));
    m_pImmediateContext->UpdateBuffer(m_pStatisticsBuffer, 0, sizeof(stats), &stats, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    m_pImmediateContext->SetPipelineState(m_pPSO);
    m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        // Map the buffer and write current view, view-projection matrix and other constants.
        MapHelper<Constants> CBConstants(m_pImmediateContext, m_pConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants->ViewMat        = m_ViewMatrix.Transpose();
        CBConstants->ViewProjMat    = m_ViewProjMatrix.Transpose();
        CBConstants->CoTanHalfFov   = m_LodScale * m_CoTanHalfFov;
        CBConstants->FrustumCulling = m_FrustumCulling ? 1 : 0;
        CBConstants->CurrTime       = static_cast<float>(m_CurrTime);

        // Calculate frustum planes from view-projection matrix.
        ViewFrustum Frustum;
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

    DrawMeshAttribs drawAttrs{m_DrawTaskCount / ASGroupSize, DRAW_FLAG_VERIFY_ALL};
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
                m_VisibleCubes = StagingData[AvailableFrameId % m_StatisticsHistorySize].visibleCubes;
        }

        ++m_FrameId;
    }
}

void Tutorial20_MeshShader::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    // Set world view matrix
    if (m_Animate)
    {
        m_RotationAngle += static_cast<float>(ElapsedTime) * 0.2f;
        if (m_RotationAngle > PI_F * 2.f)
            m_RotationAngle -= PI_F * 2.f;
        m_CurrTime += static_cast<float>(ElapsedTime);
    }

    float4x4 RotationMatrix = float4x4::RotationY(m_RotationAngle) * float4x4::RotationX(-PI_F * 0.1f);

    // Set camera position
    float4x4 View = float4x4::Translation(0.f, -4.0f, m_CameraHeight);

    // Get pretransform matrix that rotates the scene according the surface orientation
    auto SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    // Get projection matrix adjusted to the current screen orientation
    auto Proj = GetAdjustedProjectionMatrix(m_FOV, 1.f, 1000.f);

    // Compute view and view-projection matrices
    m_ViewMatrix     = RotationMatrix * View * SrfPreTransform;
    m_ViewProjMatrix = m_ViewMatrix * Proj;
}

} // namespace Diligent
