/*     Copyright 2015-2018 Egor Yusov
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

#include "Tutorial06_Multithreading.h"
#include "MapHelper.h"
#include "BasicShaderSourceStreamFactory.h"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "AntTweakBar.h"

using namespace Diligent;

SampleBase* CreateSample()
{
    return new Tutorial06_Multithreading();
}

Tutorial06_Multithreading::~Tutorial06_Multithreading()
{
    StopWorkerThreads();
}

void Tutorial06_Multithreading::GetEngineInitializationAttribs(DeviceType DevType, EngineCreationAttribs &Attribs, Uint32 &NumDeferredContexts)
{
    SampleBase::GetEngineInitializationAttribs(DevType, Attribs, NumDeferredContexts);
    NumDeferredContexts = std::max(std::thread::hardware_concurrency()-1, 2u);
}

void Tutorial06_Multithreading::Initialize(IRenderDevice *pDevice, IDeviceContext **ppContexts, Uint32 NumDeferredCtx, ISwapChain *pSwapChain)
{
    SampleBase::Initialize(pDevice, ppContexts, NumDeferredCtx, pSwapChain);

    m_MaxThreads = static_cast<int>(m_pDeferredContexts.size());

    {
        // Pipeline state object encompasses configuration of all GPU stages

        PipelineStateDesc PSODesc;
        // Pipeline state name is used by the engine to report issues
        // It is always a good idea to give objects descriptive names
        PSODesc.Name = "Cube PSO"; 

        // This is a graphics pipeline
        PSODesc.IsComputePipeline = false; 

        // This tutorial will render to a single render target
        PSODesc.GraphicsPipeline.NumRenderTargets = 1;
        // Set render target format which is the format of the swap chain's color buffer
        PSODesc.GraphicsPipeline.RTVFormats[0] = pSwapChain->GetDesc().ColorBufferFormat;
        // Set depth buffer format which is the format of the swap chain's back buffer
        PSODesc.GraphicsPipeline.DSVFormat = pSwapChain->GetDesc().DepthBufferFormat;
        // Primitive topology type defines what kind of primitives will be rendered by this pipeline state
        PSODesc.GraphicsPipeline.PrimitiveTopologyType = PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        // Cull back faces
        PSODesc.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
        // Enable depth testing
        PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

        ShaderCreationAttribs CreationAttribs;
        // Tell the system that the shader source code is in HLSL.
        // For OpenGL, the engine will convert this into GLSL behind the scene
        CreationAttribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

        // In this tutorial, we will load shaders from file. To be able to do that,
        // we need to create a shader source stream factory
        BasicShaderSourceStreamFactory BasicSSSFactory;
        CreationAttribs.pShaderSourceStreamFactory = &BasicSSSFactory;
        // Define variable type that will be used by default
        CreationAttribs.Desc.DefaultVariableType = SHADER_VARIABLE_TYPE_STATIC;
        // Create vertex shader
        RefCntAutoPtr<IShader> pVS;
        {
            CreationAttribs.Desc.ShaderType = SHADER_TYPE_VERTEX;
            CreationAttribs.EntryPoint = "main";
            CreationAttribs.Desc.Name = "Cube VS";
            CreationAttribs.FilePath = "cube.vsh";
            pDevice->CreateShader(CreationAttribs, &pVS);
            // Create dynamic uniform buffer that will store our transformation matrix
            // Dynamic buffers can be frequently updated by the CPU
            CreateUniformBuffer(pDevice, sizeof(float4x4)*2, "VS constants CB", &m_VSConstants);
            CreateUniformBuffer(pDevice, sizeof(float4x4), "Instance constants CB", &m_InstanceConstants);
            
            // Since we did not explcitly specify the type for Constants, default type 
            // (SHADER_VARIABLE_TYPE_STATIC) will be used. Static variables never change and are bound directly
            // through the shader (http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/)
            pVS->GetShaderVariable("Constants")->Set(m_VSConstants);
            pVS->GetShaderVariable("InstanceData")->Set(m_InstanceConstants);
        }

        // Create pixel shader
        RefCntAutoPtr<IShader> pPS;
        {
            CreationAttribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
            CreationAttribs.EntryPoint = "main";
            CreationAttribs.Desc.Name = "Cube PS";
            CreationAttribs.FilePath = "cube.psh";
            // Shader variables should typically be mutable, which means they are expected
            // to change on a per-instance basis
            ShaderVariableDesc Vars[] = 
            {
                {"g_Texture", SHADER_VARIABLE_TYPE_MUTABLE}
            };
            CreationAttribs.Desc.VariableDesc = Vars;
            CreationAttribs.Desc.NumVariables = _countof(Vars);

            // Define static sampler for g_Texture. Static samplers should be used whenever possible
            SamplerDesc SamLinearClampDesc( FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, 
                                            TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP);
            StaticSamplerDesc StaticSamplers[] = 
            {
                {"g_Texture", SamLinearClampDesc}
            };
            CreationAttribs.Desc.StaticSamplers = StaticSamplers;
            CreationAttribs.Desc.NumStaticSamplers = _countof(StaticSamplers);

            pDevice->CreateShader(CreationAttribs, &pPS);
        }

        // Define vertex shader input layout
        // This tutorial uses two types of input: per-vertex data and per-instance data.
        LayoutElement LayoutElems[] =
        {
            // Per-vertex data - first buffer slot
            // Attribute 0 - vertex position
            LayoutElement(0, 0, 3, VT_FLOAT32, False),
            // Attribute 1 - texture coordinates
            LayoutElement(1, 0, 2, VT_FLOAT32, False)
        };

        PSODesc.GraphicsPipeline.pVS = pVS;
        PSODesc.GraphicsPipeline.pPS = pPS;
        PSODesc.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
        PSODesc.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

        pDevice->CreatePipelineState(PSODesc, &m_pPSO);
    }

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
        VertBuffDesc.Name = "Cube vertex buffer";
        VertBuffDesc.Usage = USAGE_STATIC;
        VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        VertBuffDesc.uiSizeInBytes = sizeof(CubeVerts);
        BufferData VBData;
        VBData.pData = CubeVerts;
        VBData.DataSize = sizeof(CubeVerts);
        pDevice->CreateBuffer(VertBuffDesc, VBData, &m_CubeVertexBuffer);
    }

    {
        // Indices
        Uint32 Indices[] =
        {
            2,0,1,    2,3,0,
            4,6,5,    4,7,6,
            8,10,9,   8,11,10,
            12,14,13, 12,15,14,
            16,18,17, 16,19,18,
            20,21,22, 20,22,23
        };
        // Create index buffer
        BufferDesc IndBuffDesc;
        IndBuffDesc.Name = "Cube index buffer";
        IndBuffDesc.Usage = USAGE_STATIC;
        IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
        IndBuffDesc.uiSizeInBytes = sizeof(Indices);
        BufferData IBData;
        IBData.pData = Indices;
        IBData.DataSize = sizeof(Indices);
        pDevice->CreateBuffer(IndBuffDesc, IBData, &m_CubeIndexBuffer);
    }

    PopulateInstanceData();

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
    }
    
    // Set texture SRV in the SRB
    for(int tex=0; tex < NumTextures; ++tex)
    {
        // Create one Shader Resource Binding for every texture
        // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
        m_pPSO->CreateShaderResourceBinding(&m_SRB[tex]);
        m_SRB[tex]->GetVariable(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV[tex]);
    }

    // Create a tweak bar
    TwBar *bar = TwNewBar("Settings");
    int barSize[2] = {224 * m_UIScale, 120 * m_UIScale};
    TwSetParam(bar, NULL, "size", TW_PARAM_INT32, 2, barSize);

    // Add grid size control
    TwAddVarCB(bar, "Grid Size", TW_TYPE_INT32, SetGridSize, GetGridSize, this, "min=1 max=32");
    std::stringstream def;
    def << "min=0 max=" << m_MaxThreads;
    TwAddVarCB(bar, "Worker Threads", TW_TYPE_INT32, SetWorkerThreadCount, GetWorkerThreadCount, this, def.str().c_str());
    m_NumWorkerThreads = std::min(4, m_MaxThreads);

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
    std::uniform_real_distribution<float> rot_distr(-static_cast<float>(M_PI), +static_cast<float>(M_PI));
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
                float4x4 rotation = rotationX(rot_distr(gen)) * rotationY(rot_distr(gen)) * rotationZ(rot_distr(gen));
                // Combine rotation, scale and translation
                float4x4 matrix = rotation * scaleMatrix(scale, scale, scale) * translationMatrix(xOffset, yOffset, zOffset);
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
        auto SignalledValue = pThis->m_RenderSubsetSignal.Wait(true, pThis->m_NumWorkerThreads);
        if(SignalledValue < 0)
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
    pCtx->SetRenderTargets(0, nullptr, nullptr);

    {
        // Map the buffer and write current world-view-projection matrix
        
        // Since this is a dynamic buffer, it must be mapped in every context before
        // it can be used even though the matricesa are the same.
        MapHelper<float4x4> CBConstants(pCtx, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants[0] = transposeMatrix(m_ViewProjMatrix);
        CBConstants[1] = transposeMatrix(m_RotationMatrix);
    }

    // Bind vertex & instance buffers. This must be done for every context
    Uint32 strides[] = {sizeof(float) * 5};
    Uint32 offsets[] = {0, 0};
    IBuffer *pBuffs[] = {m_CubeVertexBuffer};
    pCtx->SetVertexBuffers(0, _countof(pBuffs), pBuffs, strides, offsets, SET_VERTEX_BUFFERS_FLAG_RESET);
    pCtx->SetIndexBuffer(m_CubeIndexBuffer, 0);

    DrawAttribs DrawAttrs;
    DrawAttrs.IsIndexed = true; // This is an indexed draw call
    DrawAttrs.IndexType = VT_UINT32; // Index type
    DrawAttrs.NumIndices = 36;
    DrawAttrs.Topology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

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
        // no COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES flag needed
        pCtx->CommitShaderResources(m_SRB[CurrInstData.TextureInd], 0);

        {
            // Map the buffer and write current world-view-projection matrix
            MapHelper<float4x4> InstData(pCtx, m_InstanceConstants, MAP_WRITE, MAP_FLAG_DISCARD);
            *InstData = transposeMatrix(CurrInstData.Matrix);
        }

        pCtx->Draw(DrawAttrs);
    }
}

// Render a frame
void Tutorial06_Multithreading::Render()
{
    // Clear the back buffer 
    const float ClearColor[] = {  0.350f,  0.350f,  0.350f, 1.0f }; 
    m_pImmediateContext->ClearRenderTarget(nullptr, ClearColor);
    m_pImmediateContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG, 1.f);

    // Transition all shader resource bindings
    for(size_t i=0; i < _countof(m_SRB); ++i)
        m_pImmediateContext->TransitionShaderResources(m_pPSO, m_SRB[i]);

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
        }

        m_NumThreadsReady = 0;
        m_GotoNextFrameSignal.Trigger(true);
    }
}

// Callback function called by AntTweakBar to set the grid size
void Tutorial06_Multithreading::SetGridSize(const void *value, void * clientData)
{
    Tutorial06_Multithreading *pTheTutorial = reinterpret_cast<Tutorial06_Multithreading*>( clientData );
    pTheTutorial->m_GridSize = *static_cast<const int *>(value);
    pTheTutorial->PopulateInstanceData();
}

// Callback function called by AntTweakBar to get the grid size
void Tutorial06_Multithreading::GetGridSize(void *value, void * clientData)
{
    Tutorial06_Multithreading *pTheTutorial = reinterpret_cast<Tutorial06_Multithreading*>( clientData );
    *static_cast<int*>(value) = pTheTutorial->m_GridSize;
}

void Tutorial06_Multithreading::SetWorkerThreadCount(const void *value, void * clientData)
{
    Tutorial06_Multithreading *pTheTutorial = reinterpret_cast<Tutorial06_Multithreading*>( clientData );
    pTheTutorial->StopWorkerThreads();
    pTheTutorial->m_NumWorkerThreads = *static_cast<const int *>(value);
    pTheTutorial->StartWorkerThreads();
}

void Tutorial06_Multithreading::GetWorkerThreadCount(void *value, void * clientData)
{
    Tutorial06_Multithreading *pTheTutorial = reinterpret_cast<Tutorial06_Multithreading*>( clientData );
    *static_cast<int*>(value) = pTheTutorial->m_NumWorkerThreads;
}


void Tutorial06_Multithreading::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    bool IsDX = m_pDevice->GetDeviceCaps().DevType == DeviceType::D3D11 || m_pDevice->GetDeviceCaps().DevType == DeviceType::D3D12;

    // Set cube view matrix
    float4x4 View = rotationX(+0.6f) * translationMatrix(0.f, 0.f, 4.0f);

    float NearPlane = 0.1f;
    float FarPlane = 100.f;
    float aspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);
    // Projection matrix differs between DX and OpenGL
    auto Proj = Projection(PI_F / 4.f, aspectRatio, NearPlane, FarPlane, IsDX);
    // Compute view-projection matrix
    m_ViewProjMatrix = View * Proj;

    // Global rotation matrix
    m_RotationMatrix = rotationY( -static_cast<float>(CurrTime) * 1.0f) * rotationX(static_cast<float>(CurrTime)*0.25f);
}
