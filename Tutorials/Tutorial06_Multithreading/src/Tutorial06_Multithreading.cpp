/*     Copyright 2015-2019 Egor Yusov
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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
#include <string>
#include <algorithm>

#include "Tutorial06_Multithreading.h"
#include "MapHelper.h"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "AntTweakBar.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial06_Multithreading();
}

Tutorial06_Multithreading::~Tutorial06_Multithreading()
{
    StopWorkerThreads();
}

void Tutorial06_Multithreading::GetEngineInitializationAttribs(DeviceType        DevType,
                                                               EngineCreateInfo& Attribs)
{
    SampleBase::GetEngineInitializationAttribs(DevType, Attribs);
    Attribs.NumDeferredContexts = std::max(std::thread::hardware_concurrency()-1, 2u);
#if VULKAN_SUPPORTED
    if(DevType == DeviceType::Vulkan)
    {
        auto& VkAttrs = static_cast<EngineVkCreateInfo&>(Attribs);
        VkAttrs.DynamicHeapSize = 26 << 20; // Enough space for 32x32x32x256 bytes allocations for 3 frames
    }
#endif
}

void Tutorial06_Multithreading::CreatePipelineState(std::vector<StateTransitionDesc>& Barriers)
{
    // Pipeline state object encompasses configuration of all GPU stages

    PipelineStateDesc PSODesc;
    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSODesc.Name = "Cube PSO"; 

    // This is a graphics pipeline
    PSODesc.IsComputePipeline = false; 

    // This tutorial will render to a single render target
    PSODesc.GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSODesc.GraphicsPipeline.RTVFormats[0]                = m_pSwapChain->GetDesc().ColorBufferFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    PSODesc.GraphicsPipeline.DSVFormat                    = m_pSwapChain->GetDesc().DepthBufferFormat;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    PSODesc.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Cull back faces
    PSODesc.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    // Enable depth testing
    PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL behind the scene
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.UseCombinedTextureSamplers = true;

    // In this tutorial, we will load shaders from file. To be able to do that,
    // we need to create a shader source stream factory
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    // Create vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube VS";
        ShaderCI.FilePath        = "cube.vsh";
        m_pDevice->CreateShader(ShaderCI, &pVS);
        // Create dynamic uniform buffer that will store our transformation matrix
        // Dynamic buffers can be frequently updated by the CPU
        CreateUniformBuffer(m_pDevice, sizeof(float4x4)*2, "VS constants CB", &m_VSConstants);
        CreateUniformBuffer(m_pDevice, sizeof(float4x4), "Instance constants CB", &m_InstanceConstants);
        // Explicitly transition the buffers to RESOURCE_STATE_CONSTANT_BUFFER state
        Barriers.emplace_back(m_VSConstants, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, true);
        Barriers.emplace_back(m_InstanceConstants, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, true);
    }

    // Create pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube PS";
        ShaderCI.FilePath        = "cube.psh";
        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    // Define vertex shader input layout
    // This tutorial uses two types of input: per-vertex data and per-instance data.
    LayoutElement LayoutElems[] =
    {
        // Per-vertex data - first buffer slot
        // Attribute 0 - vertex position
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        // Attribute 1 - texture coordinates
        LayoutElement{1, 0, 2, VT_FLOAT32, False}
    };

    PSODesc.GraphicsPipeline.pVS = pVS;
    PSODesc.GraphicsPipeline.pPS = pPS;
    PSODesc.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSODesc.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

    // Define variable type that will be used by default
    PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    ShaderResourceVariableDesc Vars[] = 
    {
        {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
    PSODesc.ResourceLayout.Variables    = Vars;
    PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    // Define static sampler for g_Texture. Static samplers should be used whenever possible
    SamplerDesc SamLinearClampDesc( FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, 
                                    TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP);
    StaticSamplerDesc StaticSamplers[] = 
    {
        {SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}
    };
    PSODesc.ResourceLayout.StaticSamplers    = StaticSamplers;
    PSODesc.ResourceLayout.NumStaticSamplers = _countof(StaticSamplers);

    m_pDevice->CreatePipelineState(PSODesc, &m_pPSO);
    // Since we did not explcitly specify the type for Constants, default type 
    // (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never change and are bound directly
    // to the pipeline state object.
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "InstanceData")->Set(m_InstanceConstants);
}


void Tutorial06_Multithreading::CreateVertexBuffer(std::vector<StateTransitionDesc>& Barriers)
{
    // Layout of this structure matches the one we defined in the pipeline state
    struct Vertex
    {
        float3 pos;
        float2 uv;
    };

    // Cube vertices

    //      (-1,+1,+1)________________(+1,+1,+1) 
    //               /|              /|
    //              / |             / |
    //             /  |            /  |
    //            /   |           /   |
    //(-1,-1,+1) /____|__________/(+1,-1,+1)
    //           |    |__________|____| 
    //           |   /(-1,+1,-1) |    /(+1,+1,-1)
    //           |  /            |   /
    //           | /             |  /
    //           |/              | /
    //           /_______________|/ 
    //        (-1,-1,-1)       (+1,-1,-1)
    // 

    // We have to duplicate verices because texture coordinates cannot
    // be shared
    Vertex CubeVerts[] =
    {
        {float3(-1,-1,-1), float2(0,1)},
        {float3(-1,+1,-1), float2(0,0)},
        {float3(+1,+1,-1), float2(1,0)},
        {float3(+1,-1,-1), float2(1,1)},

        {float3(-1,-1,-1), float2(0,1)},
        {float3(-1,-1,+1), float2(0,0)},
        {float3(+1,-1,+1), float2(1,0)},
        {float3(+1,-1,-1), float2(1,1)},

        {float3(+1,-1,-1), float2(0,1)},
        {float3(+1,-1,+1), float2(1,1)},
        {float3(+1,+1,+1), float2(1,0)},
        {float3(+1,+1,-1), float2(0,0)},

        {float3(+1,+1,-1), float2(0,1)},
        {float3(+1,+1,+1), float2(0,0)},
        {float3(-1,+1,+1), float2(1,0)},
        {float3(-1,+1,-1), float2(1,1)},

        {float3(-1,+1,-1), float2(1,0)},
        {float3(-1,+1,+1), float2(0,0)},
        {float3(-1,-1,+1), float2(0,1)},
        {float3(-1,-1,-1), float2(1,1)},

        {float3(-1,-1,+1), float2(1,1)},
        {float3(+1,-1,+1), float2(0,1)},
        {float3(+1,+1,+1), float2(0,0)},
        {float3(-1,+1,+1), float2(1,0)}
    };
    // Create vertex buffer that stores cube vertices
    BufferDesc VertBuffDesc;
    VertBuffDesc.Name          = "Cube vertex buffer";
    VertBuffDesc.Usage         = USAGE_STATIC;
    VertBuffDesc.BindFlags     = BIND_VERTEX_BUFFER;
    VertBuffDesc.uiSizeInBytes = sizeof(CubeVerts);
    BufferData VBData;
    VBData.pData = CubeVerts;
    VBData.DataSize = sizeof(CubeVerts);
    m_pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_CubeVertexBuffer);
    // Explicitly transition the buffer to RESOURCE_STATE_VERTEX_BUFFER state
    Barriers.emplace_back(m_CubeVertexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_VERTEX_BUFFER, true);
}

void Tutorial06_Multithreading::CreateIndexBuffer(std::vector<StateTransitionDesc>& Barriers)
{
    Uint32 Indices[] =
    {
        2,0,1,    2,3,0,
        4,6,5,    4,7,6,
        8,10,9,   8,11,10,
        12,14,13, 12,15,14,
        16,18,17, 16,19,18,
        20,21,22, 20,22,23
    };

    BufferDesc IndBuffDesc;
    IndBuffDesc.Name          = "Cube index buffer";
    IndBuffDesc.Usage         = USAGE_STATIC;
    IndBuffDesc.BindFlags     = BIND_INDEX_BUFFER;
    IndBuffDesc.uiSizeInBytes = sizeof(Indices);
    BufferData IBData;
    IBData.pData    = Indices;
    IBData.DataSize = sizeof(Indices);
    m_pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_CubeIndexBuffer);
    // Explicitly transition the buffer to RESOURCE_STATE_INDEX_BUFFER1 state
    Barriers.emplace_back(m_CubeIndexBuffer, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_INDEX_BUFFER, true);
}

void Tutorial06_Multithreading::LoadTextures(std::vector<StateTransitionDesc>& Barriers)
{
    // Load textures
    for(int tex=0; tex < NumTextures; ++tex)
    {
        // Load current texture
        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = true;
        RefCntAutoPtr<ITexture> SrcTex;
        std::stringstream FileNameSS;
        FileNameSS << "DGLogo" << tex << ".png";
        auto FileName = FileNameSS.str();
        CreateTextureFromFile(FileName.c_str(), loadInfo, m_pDevice, &SrcTex);
        // Get shader resource view from the texture
        m_TextureSRV[tex] = SrcTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        // Transition textures to shader resource state
        Barriers.emplace_back(SrcTex, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, true);
    }

    // Set texture SRV in the SRB
    for(int tex=0; tex < NumTextures; ++tex)
    {
        // Create one Shader Resource Binding for every texture
        // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
        m_pPSO->CreateShaderResourceBinding(&m_SRB[tex], true);
        m_SRB[tex]->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV[tex]);
    }
}

void Tutorial06_Multithreading::InitUI()
{
    // Create a tweak bar
    TwBar *bar = TwNewBar("Settings");
    int barSize[2] = {224 * m_UIScale, 120 * m_UIScale};
    TwSetParam(bar, NULL, "size", TW_PARAM_INT32, 2, barSize);

    // Add grid size control
    TwAddVarCB(bar, "Grid Size", TW_TYPE_INT32,
        [](const void* value, void* clientData)
        {
            auto* pTheTutorial = reinterpret_cast<Tutorial06_Multithreading*>(clientData);
            pTheTutorial->m_GridSize = *static_cast<const int*>(value);
            pTheTutorial->PopulateInstanceData();
        },
        [](void* value, void* clientData)
        {
            auto* pTheTutorial = reinterpret_cast<Tutorial06_Multithreading*>(clientData);
            *static_cast<int*>(value) = pTheTutorial->m_GridSize;
        },
        this, "min=1 max=32");

    std::stringstream def;
    def << "min=0 max=" << m_MaxThreads;
    TwAddVarCB(bar, "Worker Threads", TW_TYPE_INT32, 
        [](const void *value, void* clientData)
        {
            auto* pTheTutorial = reinterpret_cast<Tutorial06_Multithreading*>(clientData);
            pTheTutorial->StopWorkerThreads();
            pTheTutorial->m_NumWorkerThreads = *static_cast<const int*>(value);
            pTheTutorial->StartWorkerThreads();
        },
        [](void* value, void* clientData)
        {
            auto* pTheTutorial = reinterpret_cast<Tutorial06_Multithreading*>(clientData);
            *static_cast<int*>(value) = pTheTutorial->m_NumWorkerThreads;
        },
        this, def.str().c_str());
}

void Tutorial06_Multithreading::Initialize(IEngineFactory*   pEngineFactory,
                                           IRenderDevice*    pDevice,
                                           IDeviceContext**  ppContexts,
                                           Uint32            NumDeferredCtx,
                                           ISwapChain*       pSwapChain)
{
    SampleBase::Initialize(pEngineFactory, pDevice, ppContexts, NumDeferredCtx, pSwapChain);

    m_MaxThreads = static_cast<int>(m_pDeferredContexts.size());
    m_NumWorkerThreads = std::min(4, m_MaxThreads);

    std::vector<StateTransitionDesc> Barriers;

    CreatePipelineState(Barriers);
    CreateVertexBuffer(Barriers);
    CreateIndexBuffer(Barriers);
    LoadTextures(Barriers);    

    // Execute all barriers
    m_pImmediateContext->TransitionResourceStates(static_cast<Uint32>(Barriers.size()), Barriers.data());

    PopulateInstanceData();

    InitUI();
    
    StartWorkerThreads();
}

void Tutorial06_Multithreading::PopulateInstanceData()
{
    m_InstanceData.resize(m_GridSize*m_GridSize*m_GridSize);
    // Populate instance data buffer
    float fGridSize = static_cast<float>(m_GridSize);
    
    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<float> scale_distr(0.3f, 1.0f);
    std::uniform_real_distribution<float> offset_distr(-0.15f, +0.15f);
    std::uniform_real_distribution<float> rot_distr(-PI_F, +PI_F);
    std::uniform_int_distribution<Int32> tex_distr(0, NumTextures-1);

    float BaseScale = 0.6f / fGridSize;
    int instId = 0;
    for (int x = 0; x < m_GridSize; ++x)
    {
        for (int y = 0; y < m_GridSize; ++y)
        {
            for (int z = 0; z < m_GridSize; ++z)
            {
                // Add random offset from central position in the grid
                float xOffset = 2.f * (x+0.5f + offset_distr(gen)) / fGridSize - 1.f;
                float yOffset = 2.f * (y+0.5f + offset_distr(gen)) / fGridSize - 1.f;
                float zOffset = 2.f * (z+0.5f + offset_distr(gen)) / fGridSize - 1.f;
                // Random scale
                float scale = BaseScale * scale_distr(gen);
                // Random rotation
                float4x4 rotation = float4x4::RotationX(rot_distr(gen)) * float4x4::RotationY(rot_distr(gen)) * float4x4::RotationZ(rot_distr(gen));
                // Combine rotation, scale and translation
                float4x4 matrix = rotation * float4x4::Scale(scale, scale, scale) * float4x4::Translation(xOffset, yOffset, zOffset);
                auto &CurrInst = m_InstanceData[instId++];
                CurrInst.Matrix = matrix;
                // Texture array index
                CurrInst.TextureInd = tex_distr(gen);
            }
        }
    }
}

void Tutorial06_Multithreading::StartWorkerThreads()
{
    m_WorkerThreads.resize(m_NumWorkerThreads);
    for(Uint32 t=0; t < m_WorkerThreads.size(); ++t)
    {
        m_WorkerThreads[t] = std::thread(WorkerThreadFunc, this, t );
    }
    m_CmdLists.resize(m_NumWorkerThreads);
}

void Tutorial06_Multithreading::StopWorkerThreads()
{
    m_RenderSubsetSignal.Trigger(true, -1);

    for(auto &thread : m_WorkerThreads)
    {
        thread.join();
    }
    m_RenderSubsetSignal.Reset();
}

void Tutorial06_Multithreading::WorkerThreadFunc(Tutorial06_Multithreading *pThis, Uint32 ThreadNum)
{
    // Every thread should use its own deferred context
    IDeviceContext *pDeferredCtx = pThis->m_pDeferredContexts[ThreadNum];
    for (;;)
    {
        // Wait for the signal
        auto SignaledValue = pThis->m_RenderSubsetSignal.Wait(true, pThis->m_NumWorkerThreads);
        if(SignaledValue < 0)
            return;

        // Render current subset using the deferred context
        pThis->RenderSubset(pDeferredCtx, 1+ThreadNum);

        // Finish command list
        RefCntAutoPtr<ICommandList> pCmdList;
        pDeferredCtx->FinishCommandList(&pCmdList);
        pThis->m_CmdLists[ThreadNum] = pCmdList;

        {
            std::lock_guard<std::mutex> Lock(pThis->m_NumThreadsCompletedMtx);
            // Increment the number of completed threads
            ++pThis->m_NumThreadsCompleted;
            if(pThis->m_NumThreadsCompleted == pThis->m_NumWorkerThreads)
                pThis->m_ExecuteCommandListsSignal.Trigger();
        }

        pThis->m_GotoNextFrameSignal.Wait(true, pThis->m_NumWorkerThreads);

        // Call FinishFrame() to release dynamic resources allocated by deferred contexts
        // IMPORTANT: we must wait until the command lists are submitted for execution
        // because FinishFrame() invalidates all dynamic resources
        pDeferredCtx->FinishFrame();

        ++pThis->m_NumThreadsReady;
        // We must wait until all threads reach this point, because
        // m_GotoNextFrameSignal must be unsignaled before we proceed to 
        // RenderSubsetSignal to avoid one thread go through the loop twice in 
        // a row
        while(pThis->m_NumThreadsReady < pThis->m_NumWorkerThreads)
            std::this_thread::yield();
        VERIFY_EXPR( !pThis->m_GotoNextFrameSignal.IsTriggered() );
    }
}

void Tutorial06_Multithreading::RenderSubset(IDeviceContext *pCtx, Uint32 Subset)
{
    // Deferred contexts start in default state. We must bind everything to the context
    // Render targets are set and transitioned to correct states by the main thread, here we only verify states
    pCtx->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    {
        // Map the buffer and write current world-view-projection matrix
        
        // Since this is a dynamic buffer, it must be mapped in every context before
        // it can be used even though the matrices are the same.
        MapHelper<float4x4> CBConstants(pCtx, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants[0] = m_ViewProjMatrix.Transpose();
        CBConstants[1] = m_RotationMatrix.Transpose();
    }

    // Bind vertex & instance buffers. This must be done for every context
    Uint32 offsets[] = {0, 0};
    IBuffer *pBuffs[] = {m_CubeVertexBuffer};
    pCtx->SetVertexBuffers(0, _countof(pBuffs), pBuffs, offsets, RESOURCE_STATE_TRANSITION_MODE_VERIFY, SET_VERTEX_BUFFERS_FLAG_RESET);
    pCtx->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    DrawAttribs DrawAttrs;
    DrawAttrs.IsIndexed  = true;      // This is an indexed draw call
    DrawAttrs.IndexType  = VT_UINT32; // Index type
    DrawAttrs.NumIndices = 36;
    DrawAttrs.Flags      = DRAW_FLAG_VERIFY_ALL;

    // Set pipeline state
    pCtx->SetPipelineState(m_pPSO);
    Uint32 NumSubsets = 1 + m_NumWorkerThreads;
    Uint32 NumInstances = static_cast<Uint32>(m_InstanceData.size());
    Uint32 SusbsetSize = NumInstances / NumSubsets;
    Uint32 StartInst = SusbsetSize * Subset;
    Uint32 EndInst = (Subset < NumSubsets-1) ? SusbsetSize * (Subset+1) : NumInstances;
    for(size_t inst = StartInst; inst < EndInst; ++inst)
    {
        const auto &CurrInstData = m_InstanceData[inst];
        // Shader resources have been explicitly transitioned to correct states, so
        // RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode is not needed.
        // Instead, we use RESOURCE_STATE_TRANSITION_MODE_VERIFY mode to
        // verify that all resources are in correct states. This mode only has effect
        // in debug and development builds
        pCtx->CommitShaderResources(m_SRB[CurrInstData.TextureInd], RESOURCE_STATE_TRANSITION_MODE_VERIFY);

        {
            // Map the buffer and write current world-view-projection matrix
            MapHelper<float4x4> InstData(pCtx, m_InstanceConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            if(InstData == nullptr)
            {
                LOG_ERROR_MESSAGE("Failed to map instance data buffer");
                break;
            }
            *InstData = CurrInstData.Matrix.Transpose();
        }
        
        pCtx->Draw(DrawAttrs);
    }
}

// Render a frame
void Tutorial06_Multithreading::Render()
{
    // Clear the back buffer 
    const float ClearColor[] = {  0.350f,  0.350f,  0.350f, 1.0f }; 
    m_pImmediateContext->ClearRenderTarget(nullptr, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    if (m_NumWorkerThreads > 0)
    {
        m_NumThreadsCompleted = 0;
        m_RenderSubsetSignal.Trigger(true);
    }

    RenderSubset(m_pImmediateContext, 0);

    if (m_NumWorkerThreads > 0)
    {
        m_ExecuteCommandListsSignal.Wait(true, 1);

        for(auto &cmdList : m_CmdLists)
        {
            m_pImmediateContext->ExecuteCommandList(cmdList);
            // Release command lists now to release all outstanding references
            // In d3d11 mode, command lists hold references to the swap chain's back buffer 
            // that cause swap chain resize to fail
            cmdList.Release();
        }

        m_NumThreadsReady = 0;
        m_GotoNextFrameSignal.Trigger(true);
    }
}

void Tutorial06_Multithreading::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    const bool IsGL = m_pDevice->GetDeviceCaps().IsGLDevice();

    // Set cube view matrix
    float4x4 View = float4x4::RotationX(-0.6f) * float4x4::Translation(0.f, 0.f, 4.0f);

    float NearPlane = 0.1f;
    float FarPlane = 100.f;
    float aspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);
    // Projection matrix differs between DX and OpenGL
    auto Proj = float4x4::Projection(PI_F / 4.f, aspectRatio, NearPlane, FarPlane, IsGL);
    // Compute view-projection matrix
    m_ViewProjMatrix = View * Proj;

    // Global rotation matrix
    m_RotationMatrix = float4x4::RotationY( static_cast<float>(CurrTime) * 1.0f) * float4x4::RotationX(-static_cast<float>(CurrTime)*0.25f);
}

}
