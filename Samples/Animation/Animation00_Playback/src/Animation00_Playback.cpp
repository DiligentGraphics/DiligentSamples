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

#include "Animation00_Playback.hpp"
#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "../../Common/src/AnimationUtilities.hpp"
#include "ozz/animation/runtime/local_to_model_job.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Animation00_Playback();
}

void Animation00_Playback::CreatePlanePSO()
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSOCreateInfo.PSODesc.Name = "Plane PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial renders to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
    // No cull
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_NONE;
    // Enable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    // clang-format on

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.UseCombinedTextureSamplers = true;

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create plane vertex shader
    RefCntAutoPtr<IShader> pPlaneVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Plane VS";
        ShaderCI.FilePath        = "plane.vsh";
        m_pDevice->CreateShader(ShaderCI, &pPlaneVS);
    }

    // Create plane pixel shader
    RefCntAutoPtr<IShader> pPlanePS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Plane PS";
        ShaderCI.FilePath        = "plane.psh";
        m_pDevice->CreateShader(ShaderCI, &pPlanePS);
    }

    PSOCreateInfo.pVS = pPlaneVS;
    PSOCreateInfo.pPS = pPlanePS;

    // Define variable type that will be used by default
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pPlanePSO);

    // Since we did not explcitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
    // change and are bound directly through the pipeline state object.
    m_pPlanePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

    // Create a shader resource binding object and bind all static resources in it
    m_pPlanePSO->CreateShaderResourceBinding(&m_pPlaneSRB, true);
}

void Animation00_Playback::RenderPlane()
{
    
    m_pImmediateContext->SetPipelineState(m_pPlanePSO);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
    // makes sure that resources are transitioned to required states.
    // Note that Vulkan requires shadow map to be transitioned to DEPTH_READ state, not SHADER_RESOURCE
    m_pImmediateContext->CommitShaderResources(m_pPlaneSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs DrawAttrs(4, DRAW_FLAG_VERIFY_ALL);
    m_pImmediateContext->Draw(DrawAttrs);
}

bool Animation00_Playback::InitAnimation()
{
    // Reading skeleton.
    if (!LoadSkeleton("pab_skeleton.ozz", &m_Skeleton))
    {
        return false;
    }

    // Reading animation.
    if (!LoadAnimation("pab_crossarms.ozz", &m_Animation))
    {
        return false;
    }

    // Skeleton and animation needs to match.
    if (m_Skeleton.num_joints() != m_Animation.num_tracks())
    {
        return false;
    }

    // Allocates runtime buffers.
    const int NumSoaJoints = m_Skeleton.num_soa_joints();
    m_Locals.resize(NumSoaJoints);
    const int NumJoints = m_Skeleton.num_joints();
    m_Models.resize(NumJoints);

    // Allocates a cache that matches animation requirements.
    m_Cache.Resize(NumJoints);

    // Allocates instance buffers.
    const int NumInstance = NumJoints - 1;
    m_Bones.resize(NumInstance);
    m_Joints.resize(NumInstance);

    
    return true;
}

void Animation00_Playback::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);
    
    //Initialize Animation
    InitAnimation();

    std::vector<StateTransitionDesc> Barriers;
    // Create dynamic uniform buffer that will store our transformation matrices
    // Dynamic buffers can be frequently updated by the CPU
    CreateUniformBuffer(m_pDevice, sizeof(float4x4), "VS constants CB", &m_VSConstants);
    Barriers.emplace_back(m_VSConstants, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, true);
    
    CreateSkeletonPSO();
    m_JointVertexBuffer = CreateJointVertexBuffer(m_pDevice);   
    m_BoneVertexBuffer  = CreateBoneVertexBuffer(m_pDevice);
    CreateInstanceBuffer();

    CreatePlanePSO();
}

// Render a frame
void Animation00_Playback::Render()
{
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
    // Clear the back buffer
    const float ClearColor[] = {0.350f, 0.350f, 0.350f, 1.0f};
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

   {
        struct Constants
        {
            float4x4 WorldViewProj;
        };
        MapHelper<Constants> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants->WorldViewProj = m_WorldViewProjMatrix.Transpose();
    }

    RenderPlane();
    
    RenderSkeleton();
}

void Animation00_Playback::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
        
    m_PlaybackController.UpdateUI(m_Animation);
    m_PlaybackController.Update(m_Animation, (float)ElapsedTime);

    // Samples optimized animation at t = animation_time_.
    ozz::animation::SamplingJob Sampling;
    Sampling.animation     = &m_Animation;
    Sampling.cache         = &m_Cache;
    Sampling.ratio         = m_PlaybackController.TimeRatio();
    Sampling.output        = make_span(m_Locals);
    if (!Sampling.Run())
    {
        LOG_ERROR_MESSAGE("Animation sampling job failed");
    }

    // Converts from local space to model space matrices.
    ozz::animation::LocalToModelJob LocalToModel;
    LocalToModel.skeleton    = &m_Skeleton;
    LocalToModel.input    = make_span(m_Locals);
    LocalToModel.output      = make_span(m_Models);
    if (!LocalToModel.Run())
    {
        LOG_ERROR_MESSAGE("Animation local to model job failed");
    }

    // Rebuild matrices for render
    FillInstanceBuffer(m_Skeleton, make_span(m_Models), make_span(m_Joints), make_span(m_Bones));
    // Update instance data buffer
    Uint32 DataSize = static_cast<Uint32>(sizeof(m_Joints[0]) * m_Joints.size());
    m_pImmediateContext->UpdateBuffer(m_JointInstanceBuffer, 0, DataSize, m_Joints.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DataSize = static_cast<Uint32>(sizeof(m_Bones[0]) * m_Joints.size());
    m_pImmediateContext->UpdateBuffer(m_BoneInstanceBuffer, 0, DataSize, m_Bones.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    float4x4 CameraView = float4x4::RotationY(PI_F) * float4x4::RotationX(PI_F * -0.2) * float4x4::Translation(0.f, -1.0f, 5.0f);

    // Get pretransform matrix that rotates the scene according the surface orientation
    auto SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    // Get projection matrix adjusted to the current screen orientation
    auto Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

    // Compute camera view-projection matrix
    m_WorldViewProjMatrix = CameraView * SrfPreTransform * Proj;
}

void Animation00_Playback::CreateSkeletonPSO()
{
    // Pipeline state object encompasses configuration of all GPU stages

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSOCreateInfo.PSODesc.Name = "Bone PSO";

    // This is a graphics pipeline
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // clang-format off
    // This tutorial will render to a single render target
    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Cull back faces
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    // Enable depth testing
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    // clang-format on

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.UseCombinedTextureSamplers = true;

    // In this tutorial, we will load shaders from file. To be able to do that,
    // we need to create a shader source stream factory
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Skeleton VS";
        ShaderCI.FilePath        = "skeleton.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Skeleton PS";
        ShaderCI.FilePath        = "skeleton.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    // clang-format off
    // Define vertex shader input layout
    // This tutorial uses two types of input: per-vertex data and per-instance data.
    LayoutElement LayoutElems[] =
    {
        // Per-vertex data - first buffer slot
        // Attribute 0 - vertex position
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        // Attribute 0 - vertex normal
        LayoutElement{1, 0, 3, VT_FLOAT32, False},
        // Attribute 1 - vertex color
        LayoutElement{2, 0, 4, VT_FLOAT32, False},
            
        // Per-instance data - second buffer slot
        // We will use four attributes to encode instance-specific 4x4 transformation matrix
        // Attribute 2 - first row
        LayoutElement{3, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 3 - second row
        LayoutElement{4, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 4 - third row
        LayoutElement{5, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 5 - fourth row
        LayoutElement{6, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE}
    };
    // clang-format on
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    // Define variable type that will be used by default
    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pBonePSO);

    // Since we did not explcitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never
    // change and are bound directly through the pipeline state object.
    m_pBonePSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

    // Create a shader resource binding object and bind all static resources in it
    m_pBonePSO->CreateShaderResourceBinding(&m_pSkeletonSRB, true);

    //Joint use line strip
    PSOCreateInfo.PSODesc.Name                       = "Joint PSO";
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_LINE_STRIP;
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_pJointPSO);
    m_pJointPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    m_pJointPSO->CreateShaderResourceBinding(&m_pSkeletonSRB, true);

}

void Animation00_Playback::RenderSkeleton()
{
    // Render Joints
    // Set the pipeline state
    m_pImmediateContext->SetPipelineState(m_pJointPSO);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
    // makes sure that resources are transitioned to required states.
    m_pImmediateContext->CommitShaderResources(m_pSkeletonSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Bind vertex, instance and index buffers
    Uint32   offsets[] = {0, 0};
    IBuffer* pJointBuffs[]  = {m_JointVertexBuffer, m_JointInstanceBuffer};
    m_pImmediateContext->SetVertexBuffers(0, _countof(pJointBuffs), pJointBuffs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

    DrawAttribs DrawAttrs;    
    DrawAttrs.NumVertices  = 68;
    DrawAttrs.NumInstances = m_Skeleton.num_joints() - 1;
    // Verify the state of vertex buffers
    DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->Draw(DrawAttrs);

    // Render Bones
    m_pImmediateContext->SetPipelineState(m_pBonePSO);
    // Bind vertex, instance and index buffers
    IBuffer* pBoneBuffs[]  = {m_BoneVertexBuffer, m_BoneInstanceBuffer};
    m_pImmediateContext->SetVertexBuffers(0, _countof(pBoneBuffs), pBoneBuffs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);

    DrawAttrs.NumVertices  = 24;
    m_pImmediateContext->Draw(DrawAttrs);
}

void Animation00_Playback::CreateInstanceBuffer()
{
    int NumInstance = m_Skeleton.num_joints() - 1;

    // Create instance data buffer that will store transformation matrices
    BufferDesc InstBuffDesc;
    InstBuffDesc.Name = "Joint Instance data buffer";
    // Use default usage as this buffer will only be updated when grid size changes
    InstBuffDesc.Usage         = USAGE_DEFAULT;
    InstBuffDesc.BindFlags     = BIND_VERTEX_BUFFER;
    InstBuffDesc.uiSizeInBytes = sizeof(float4x4) * NumInstance;
    m_pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_JointInstanceBuffer);

    InstBuffDesc.Name = "Bone Instance data buffer";
    m_pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_BoneInstanceBuffer);
}

} // namespace Diligent
