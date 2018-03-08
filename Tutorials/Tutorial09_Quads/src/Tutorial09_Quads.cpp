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
#include <cmath>

#include "Tutorial09_Quads.h"
#include "MapHelper.h"
#include "BasicShaderSourceStreamFactory.h"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "AntTweakBar.h"

using namespace Diligent;

SampleBase* CreateSample()
{
    return new Tutorial09_Quads();
}

Tutorial09_Quads::~Tutorial09_Quads()
{
    StopWorkerThreads();
}

void Tutorial09_Quads::GetEngineInitializationAttribs(DeviceType DevType, EngineCreationAttribs &Attribs, Uint32 &NumDeferredContexts)
{
    SampleBase::GetEngineInitializationAttribs(DevType, Attribs, NumDeferredContexts);
    NumDeferredContexts = std::max(std::thread::hardware_concurrency()-1, 2u);
}

void Tutorial09_Quads::Initialize(IRenderDevice *pDevice, IDeviceContext **ppContexts, Uint32 NumDeferredCtx, ISwapChain *pSwapChain)
{
    SampleBase::Initialize(pDevice, ppContexts, NumDeferredCtx, pSwapChain);

    m_MaxThreads = static_cast<int>(m_pDeferredContexts.size());

    {
        // Pipeline state object encompasses configuration of all GPU stages

        PipelineStateDesc PSODesc;
        // Pipeline state name is used by the engine to report issues
        // It is always a good idea to give objects descriptive names
        PSODesc.Name = "Quad PSO"; 

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
        // Disable back face culling
        PSODesc.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
        // Disable depth testing
        PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

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
            CreationAttribs.Desc.Name = "Quad VS";
            CreationAttribs.FilePath = "quad.vsh";
            pDevice->CreateShader(CreationAttribs, &pVS);
            // Create dynamic uniform buffer that will store our transformation matrix
            // Dynamic buffers can be frequently updated by the CPU
            CreateUniformBuffer(pDevice, sizeof(float4x4), "Instance constants CB", &m_InstanceConstants);
            
            // Since we did not explcitly specify the type for Constants, default type 
            // (SHADER_VARIABLE_TYPE_STATIC) will be used. Static variables never change and are bound directly
            // through the shader (http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/)
            pVS->GetShaderVariable("InstanceData")->Set(m_InstanceConstants);
        }

        // Create pixel shader
        RefCntAutoPtr<IShader> pPS;
        {
            CreationAttribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
            CreationAttribs.EntryPoint = "main";
            CreationAttribs.Desc.Name = "Quad PS";
            CreationAttribs.FilePath = "quad.psh";
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

        PSODesc.GraphicsPipeline.pVS = pVS;
        PSODesc.GraphicsPipeline.pPS = pPS;

        pDevice->CreatePipelineState(PSODesc, &m_pPSO);
    }

    InitializeInstanceData();

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

    // Add num quads control
    TwAddVarCB(bar, "Num Quads", TW_TYPE_INT32, SetNumQuads, GetNumQuads, this, "min=1 max=100000 step=20");
    std::stringstream def;
    def << "min=0 max=" << m_MaxThreads;
    TwAddVarCB(bar, "Worker Threads", TW_TYPE_INT32, SetWorkerThreadCount, GetWorkerThreadCount, this, def.str().c_str());
    m_NumWorkerThreads = std::min(4, m_MaxThreads);

    StartWorkerThreads();
}

void Tutorial09_Quads::InitializeInstanceData()
{
    m_InstanceData.resize(m_NumQuads);

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(0); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<float> scale_distr(0.01f, 0.05f);
    std::uniform_real_distribution<float> pos_distr(-0.95f, +0.95f);
    std::uniform_real_distribution<float> move_dir_distr(-0.1f, +0.1f);
    std::uniform_real_distribution<float> angle_distr(-static_cast<float>(M_PI), +static_cast<float>(M_PI));
    std::uniform_real_distribution<float> rot_distr(-static_cast<float>(M_PI)*0.5f, +static_cast<float>(M_PI)*0.5f);
    std::uniform_int_distribution<Int32> tex_distr(0, NumTextures-1);

    for (int quad = 0; quad < m_NumQuads; ++quad)
    {
        auto &CurrInst = m_InstanceData[quad];
        CurrInst.Size = scale_distr(gen);
        CurrInst.Angle = angle_distr(gen);
        CurrInst.Pos.x = pos_distr(gen);
        CurrInst.Pos.y = pos_distr(gen);
        CurrInst.MoveDir.x = move_dir_distr(gen);
        CurrInst.MoveDir.y = move_dir_distr(gen);
        CurrInst.RotSpeed = rot_distr(gen);
        // Texture array index
        CurrInst.TextureInd = tex_distr(gen);
    }
}

void Tutorial09_Quads::UpdateInstanceData(float elapsedTime)
{
    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()

    std::uniform_real_distribution<float> rot_distr(-static_cast<float>(M_PI)*0.5f, +static_cast<float>(M_PI)*0.5f);
    for (int quad = 0; quad < m_NumQuads; ++quad)
    {
        auto &CurrInst = m_InstanceData[quad];
        CurrInst.Angle += CurrInst.RotSpeed * elapsedTime;
        if (std::abs(CurrInst.Pos.x + CurrInst.MoveDir.x * elapsedTime) > 0.95)
        {
            CurrInst.MoveDir.x *= -1.f;
            CurrInst.RotSpeed = rot_distr(gen);
        }
        CurrInst.Pos.x += CurrInst.MoveDir.x * elapsedTime;
        if (std::abs(CurrInst.Pos.y + CurrInst.MoveDir.y * elapsedTime) > 0.95)
        {
            CurrInst.MoveDir.y *= -1.f;
            CurrInst.RotSpeed = rot_distr(gen);
        }
        CurrInst.Pos.y += CurrInst.MoveDir.y * elapsedTime;
    }
}

void Tutorial09_Quads::StartWorkerThreads()
{
    m_WorkerThreads.resize(m_NumWorkerThreads);
    for(Uint32 t=0; t < m_WorkerThreads.size(); ++t)
    {
        m_WorkerThreads[t] = std::thread(WorkerThreadFunc, this, t );
    }
    m_CmdLists.resize(m_NumWorkerThreads);
}

void Tutorial09_Quads::StopWorkerThreads()
{
    m_RenderSubsetSignal.Trigger(true, -1);

    for(auto &thread : m_WorkerThreads)
    {
        thread.join();
    }
    m_RenderSubsetSignal.Reset();
}

void Tutorial09_Quads::WorkerThreadFunc(Tutorial09_Quads *pThis, Uint32 ThreadNum)
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

void Tutorial09_Quads::RenderSubset(IDeviceContext *pCtx, Uint32 Subset)
{
    // Deferred contexts start in default state. We must bind everything to the context
    pCtx->SetRenderTargets(0, nullptr, nullptr);

    DrawAttribs DrawAttrs;
    DrawAttrs.NumIndices = 4;
    DrawAttrs.Topology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;

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
            struct VSInstanceData
            {
                float4 g_QuadRotationAndScale;
                float4 g_QuadCenter;
            };

            // Map the buffer and write current world-view-projection matrix
            MapHelper<VSInstanceData> InstData(pCtx, m_InstanceConstants, MAP_WRITE, MAP_FLAG_DISCARD);

            float2x2 ScaleMatr = 
                float2x2(CurrInstData.Size, 0.f,
                         0.f, CurrInstData.Size);
            float sinAngle = sinf(CurrInstData.Angle);
            float cosAngle = cosf(CurrInstData.Angle);
            float2x2 RotMatr(cosAngle, -sinAngle,
                             sinAngle, cosAngle);
            auto Matr = ScaleMatr * RotMatr;
            InstData->g_QuadRotationAndScale.x = Matr._m00;
            InstData->g_QuadRotationAndScale.y = Matr._m10;
            InstData->g_QuadRotationAndScale.z = Matr._m01;
            InstData->g_QuadRotationAndScale.w = Matr._m11;
            InstData->g_QuadCenter.x = CurrInstData.Pos.x;
            InstData->g_QuadCenter.y = CurrInstData.Pos.y;
        }

        pCtx->Draw(DrawAttrs);
    }
}

// Render a frame
void Tutorial09_Quads::Render()
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
void Tutorial09_Quads::SetNumQuads(const void *value, void * clientData)
{
    Tutorial09_Quads *pTheTutorial = reinterpret_cast<Tutorial09_Quads*>( clientData );
    pTheTutorial->m_NumQuads = *static_cast<const int *>(value);
    pTheTutorial->InitializeInstanceData();
}

// Callback function called by AntTweakBar to get the grid size
void Tutorial09_Quads::GetNumQuads(void *value, void * clientData)
{
    Tutorial09_Quads *pTheTutorial = reinterpret_cast<Tutorial09_Quads*>( clientData );
    *static_cast<int*>(value) = pTheTutorial->m_NumQuads;
}

void Tutorial09_Quads::SetWorkerThreadCount(const void *value, void * clientData)
{
    Tutorial09_Quads *pTheTutorial = reinterpret_cast<Tutorial09_Quads*>( clientData );
    pTheTutorial->StopWorkerThreads();
    pTheTutorial->m_NumWorkerThreads = *static_cast<const int *>(value);
    pTheTutorial->StartWorkerThreads();
}

void Tutorial09_Quads::GetWorkerThreadCount(void *value, void * clientData)
{
    Tutorial09_Quads *pTheTutorial = reinterpret_cast<Tutorial09_Quads*>( clientData );
    *static_cast<int*>(value) = pTheTutorial->m_NumWorkerThreads;
}


void Tutorial09_Quads::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateInstanceData(static_cast<float>(ElapsedTime));
}
