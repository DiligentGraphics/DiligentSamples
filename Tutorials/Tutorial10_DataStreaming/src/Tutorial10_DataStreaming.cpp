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
#include <math.h>
#include <algorithm>

#include "Tutorial10_DataStreaming.h"
#include "MapHelper.h"
#include "BasicShaderSourceStreamFactory.h"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "AntTweakBar.h"

using namespace Diligent;

class StreamingBuffer
{
public:
    StreamingBuffer(IRenderDevice* pDevice, BIND_FLAGS BindFlags, Uint32 Size, size_t NumContexts, const Char* Name) : 
        m_BufferSize            (Size),
        m_MapInfo               (NumContexts)
    {
        BufferDesc BuffDesc;
        BuffDesc.Name           = Name;
        BuffDesc.Usage          = USAGE_DYNAMIC;
        BuffDesc.BindFlags      = BindFlags;
        BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        BuffDesc.uiSizeInBytes  = Size;
        pDevice->CreateBuffer(BuffDesc, BufferData(), &m_pBuffer);
    }

    // Returns offset of the allocated region
    Uint32 Allocate(IDeviceContext* pCtx, Uint32 Size, size_t CtxNum)
    {
        auto& MapInfo = m_MapInfo[CtxNum];
        // Check if there is enough space in the buffer
        if (MapInfo.m_CurrOffset + Size > m_BufferSize)
        {
            // Unmap the buffer
            Flush(CtxNum);
        }
        
        if (MapInfo.m_MappedData == nullptr)
        {
            // If current offset is zero, we are mapping the buffer for the first time after it has been flushed. Use MAP_FLAG_DISCARD flag.
            // Otherwise use MAP_FLAG_DO_NOT_SYNCHRONIZE flag.
            MapInfo.m_MappedData.Map(pCtx, m_pBuffer, MAP_WRITE, MapInfo.m_CurrOffset == 0 ? MAP_FLAG_DISCARD : MAP_FLAG_DO_NOT_SYNCHRONIZE);
        }

        auto Offset = MapInfo.m_CurrOffset;
        // Update offset
        MapInfo.m_CurrOffset += Size;
        return Offset;
    }

    void Release(size_t CtxNum)
    {
        if (!m_AllowPersistentMap)
        {
            m_MapInfo[CtxNum].m_MappedData.Unmap();
        }
    }
    
    void Flush(size_t CtxNum)
    {
        m_MapInfo[CtxNum].m_MappedData.Unmap();
        m_MapInfo[CtxNum].m_CurrOffset = 0;
    }

    IBuffer* GetBuffer(){return m_pBuffer;}
    void* GetMappedCPUAddress(size_t CtxNum)
    {
        return m_MapInfo[CtxNum].m_MappedData;
    }

    void AllowPersistentMapping(bool AllowMapping)
    {
        m_AllowPersistentMap = AllowMapping;
    }

private:
    RefCntAutoPtr<IBuffer> m_pBuffer;
    const Uint32 m_BufferSize;

    bool m_AllowPersistentMap = false;
    
    struct MapInfo
    {
        MapHelper<Uint8> m_MappedData;
        Uint32 m_CurrOffset = 0;
    };
    // We need to keep track of mapped data for every context
    std::vector<MapInfo> m_MapInfo;
};

SampleBase* CreateSample()
{
    return new Tutorial10_DataStreaming();
}

Tutorial10_DataStreaming::~Tutorial10_DataStreaming()
{
    StopWorkerThreads();
}

void Tutorial10_DataStreaming::GetEngineInitializationAttribs(DeviceType DevType, EngineCreationAttribs &Attribs, Uint32 &NumDeferredContexts)
{
    SampleBase::GetEngineInitializationAttribs(DevType, Attribs, NumDeferredContexts);
    NumDeferredContexts = std::max(std::thread::hardware_concurrency()-1, 2u);
#if D3D12_SUPPORTED
    if (DevType == DeviceType::D3D12)
    {
        EngineD3D12Attribs &EngD3D12Attribs = static_cast<EngineD3D12Attribs &>(Attribs);
        EngD3D12Attribs.NumCommandsToFlushCmdList = 8192;
    }
#endif
#if VULKAN_SUPPORTED
    if(DevType == DeviceType::Vulkan)
    {
        auto& VkAttrs = static_cast<EngineVkAttribs&>(Attribs);
        VkAttrs.DynamicHeapSize = 128 << 20;
        VkAttrs.DynamicHeapPageSize = 2 << 20;
        VkAttrs.NumCommandsToFlushCmdBuffer = 8192;
    }
#endif
}

void Tutorial10_DataStreaming::Initialize(IRenderDevice *pDevice, IDeviceContext **ppContexts, Uint32 NumDeferredCtx, ISwapChain *pSwapChain)
{
    SampleBase::Initialize(pDevice, ppContexts, NumDeferredCtx, pSwapChain);

    m_MaxThreads = static_cast<int>(m_pDeferredContexts.size());

    BlendStateDesc BlendState[NumStates];
    BlendState[1].RenderTargets[0].BlendEnable = true;
    BlendState[1].RenderTargets[0].SrcBlend = BLEND_FACTOR_SRC_ALPHA;
    BlendState[1].RenderTargets[0].DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;

    BlendState[2].RenderTargets[0].BlendEnable = true;
    BlendState[2].RenderTargets[0].SrcBlend = BLEND_FACTOR_INV_SRC_ALPHA;
    BlendState[2].RenderTargets[0].DestBlend = BLEND_FACTOR_SRC_ALPHA;

    BlendState[3].RenderTargets[0].BlendEnable = true;
    BlendState[3].RenderTargets[0].SrcBlend = BLEND_FACTOR_SRC_COLOR;
    BlendState[3].RenderTargets[0].DestBlend = BLEND_FACTOR_INV_SRC_COLOR;

    BlendState[4].RenderTargets[0].BlendEnable = true;
    BlendState[4].RenderTargets[0].SrcBlend = BLEND_FACTOR_INV_SRC_COLOR;
    BlendState[4].RenderTargets[0].DestBlend = BLEND_FACTOR_SRC_COLOR;

    std::vector<StateTransitionDesc> Barriers;
    {
        // Pipeline state object encompasses configuration of all GPU stages

        PipelineStateDesc PSODesc;
        // Pipeline state name is used by the engine to report issues
        // It is always a good idea to give objects descriptive names
        PSODesc.Name = "Polygon PSO"; 

        // This is a graphics pipeline
        PSODesc.IsComputePipeline = false; 

        // This tutorial will render to a single render target
        PSODesc.GraphicsPipeline.NumRenderTargets = 1;
        // Set render target format which is the format of the swap chain's color buffer
        PSODesc.GraphicsPipeline.RTVFormats[0] = pSwapChain->GetDesc().ColorBufferFormat;
        // Set depth buffer format which is the format of the swap chain's back buffer
        PSODesc.GraphicsPipeline.DSVFormat = pSwapChain->GetDesc().DepthBufferFormat;
        // Primitive topology defines what kind of primitives will be rendered by this pipeline state
        PSODesc.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // Disable back face culling
        PSODesc.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
        // Disable depth testing
        PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

        ShaderCreationAttribs CreationAttribs;
        // Tell the system that the shader source code is in HLSL.
        // For OpenGL, the engine will convert this into GLSL behind the scene
        CreationAttribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

        // We will be using combined texture samplers
        CreationAttribs.UseCombinedTextureSamplers = true;

        // In this tutorial, we will load shaders from file. To be able to do that,
        // we need to create a shader source stream factory
        BasicShaderSourceStreamFactory BasicSSSFactory;
        CreationAttribs.pShaderSourceStreamFactory = &BasicSSSFactory;
        // Define variable type that will be used by default
        CreationAttribs.Desc.DefaultVariableType = SHADER_VARIABLE_TYPE_STATIC;
        // Create vertex shader
        RefCntAutoPtr<IShader> pVS, pVSBatched;
        {
            CreationAttribs.Desc.ShaderType = SHADER_TYPE_VERTEX;
            CreationAttribs.EntryPoint = "main";
            CreationAttribs.Desc.Name = "Polygon VS";
            CreationAttribs.FilePath = "polygon.vsh";
            pDevice->CreateShader(CreationAttribs, &pVS);
            // Create dynamic uniform buffer that will store our transformation matrix
            // Dynamic buffers can be frequently updated by the CPU
            CreateUniformBuffer(pDevice, sizeof(float4x4), "Instance constants CB", &m_PolygonAttribsCB);
            // Transition the buffer to RESOURCE_STATE_CONSTANT_BUFFER state
            Barriers.emplace_back(m_PolygonAttribsCB, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, true);

            // Since we did not explcitly specify the type for Constants, default type 
            // (SHADER_VARIABLE_TYPE_STATIC) will be used. Static variables never change and are bound directly
            // through the shader (http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/)
            pVS->GetShaderVariable("PolygonAttribs")->Set(m_PolygonAttribsCB);

            CreationAttribs.Desc.Name = "Polygon VS Batched";
            CreationAttribs.FilePath = "polygon_batch.vsh";
            pDevice->CreateShader(CreationAttribs, &pVSBatched);
        }

        // Create pixel shader
        RefCntAutoPtr<IShader> pPS, pPSBatched;
        {
            CreationAttribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
            CreationAttribs.EntryPoint = "main";
            CreationAttribs.Desc.Name = "Polygon PS";
            CreationAttribs.FilePath = "polygon.psh";
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

            CreationAttribs.Desc.Name = "Polygon PS Batched";
            CreationAttribs.FilePath = "polygon_batch.psh";
            pDevice->CreateShader(CreationAttribs, &pPSBatched);
        }

        LayoutElement LayoutElem[] =
        {
            // Attribute 0 - PolygonXY
            LayoutElement(0, 0, 2, VT_FLOAT32, False, 0, 0, LayoutElement::FREQUENCY_PER_VERTEX)
        };
        PSODesc.GraphicsPipeline.InputLayout.LayoutElements = LayoutElem;
        PSODesc.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElem);

        PSODesc.GraphicsPipeline.pVS = pVS;
        PSODesc.GraphicsPipeline.pPS = pPS;

        for (int state = 0; state < NumStates; ++state)
        {
            PSODesc.GraphicsPipeline.BlendDesc = BlendState[state];
            pDevice->CreatePipelineState(PSODesc, &m_pPSO[0][state]);
            if (state > 0)
                VERIFY(m_pPSO[0][state]->IsCompatibleWith(m_pPSO[0][0]), "PSOs are expected to be compatible");
        }


        PSODesc.Name = "Batched Polygon PSO";
        // Define vertex shader input layout
        // This tutorial uses two types of input: per-vertex data and per-instance data.
        LayoutElement BatchLayoutElems[] =
        {
            // Attribute 0 - PolygonXY
            LayoutElement(0, 0, 2, VT_FLOAT32, False, 0, 0, LayoutElement::FREQUENCY_PER_VERTEX),
            // Attribute 1 - PolygonRotationAndScale
            LayoutElement(1, 1, 4, VT_FLOAT32, False, 0, 0, LayoutElement::FREQUENCY_PER_INSTANCE),
            // Attribute 2 - PolygonCenter
            LayoutElement(2, 1, 2, VT_FLOAT32, False, 0, 0, LayoutElement::FREQUENCY_PER_INSTANCE),
            // Attribute 3 - TexArrInd
            LayoutElement(3, 1, 1, VT_FLOAT32, False, 0, 0, LayoutElement::FREQUENCY_PER_INSTANCE)
        };
        PSODesc.GraphicsPipeline.InputLayout.LayoutElements = BatchLayoutElems;
        PSODesc.GraphicsPipeline.InputLayout.NumElements = _countof(BatchLayoutElems);
        
        PSODesc.GraphicsPipeline.pVS = pVSBatched;
        PSODesc.GraphicsPipeline.pPS = pPSBatched;

        for (int state = 0; state < NumStates; ++state)
        {
            PSODesc.GraphicsPipeline.BlendDesc = BlendState[state];
            pDevice->CreatePipelineState(PSODesc, &m_pPSO[1][state]);
            if (state > 0)
                VERIFY(m_pPSO[1][state]->IsCompatibleWith(m_pPSO[1][0]), "PSOs are expected to be compatible");
        }
    }

    m_StreamingVB.reset(new StreamingBuffer(pDevice, BIND_VERTEX_BUFFER, MaxVertsInStreamingBuffer * sizeof(float2),     1+NumDeferredCtx, "Streaming vertex buffer"));
    m_StreamingIB.reset(new StreamingBuffer(pDevice, BIND_INDEX_BUFFER,  MaxVertsInStreamingBuffer * 3 * sizeof(Uint32), 1+NumDeferredCtx, "Streaming index buffer"));

    // Transition the buffers to required state
    Barriers.emplace_back(m_StreamingVB->GetBuffer(), RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_VERTEX_BUFFER, true);
    Barriers.emplace_back(m_StreamingIB->GetBuffer(), RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_INDEX_BUFFER, true);

    InitializePolygonGeometry();
    InitializePolygons();

    // Load textures
    RefCntAutoPtr<ITexture> pTexArray;
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

        const auto &TexDesc = SrcTex->GetDesc();
        if (pTexArray == nullptr)
        {
            //	Create texture array
            auto TexArrDesc = TexDesc;
            TexArrDesc.ArraySize = NumTextures;
            TexArrDesc.Type = RESOURCE_DIM_TEX_2D_ARRAY;
            TexArrDesc.Usage = USAGE_DEFAULT;
            TexArrDesc.BindFlags = BIND_SHADER_RESOURCE;
            m_pDevice->CreateTexture(TexArrDesc, TextureData(), &pTexArray);
        }
        // Copy current texture into the texture array
        for (Uint32 mip = 0; mip < TexDesc.MipLevels; ++mip)
        {
            CopyTextureAttribs CopyAttribs(SrcTex, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, pTexArray, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
            CopyAttribs.SrcMipLevel = mip;
            CopyAttribs.DstMipLevel = mip;
            CopyAttribs.DstSlice = tex;
            m_pImmediateContext->CopyTexture(CopyAttribs);
        }

        Barriers.emplace_back(SrcTex, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, true);
    }
    m_TexArraySRV = pTexArray->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    // Transition all textures to shader resource state
    Barriers.emplace_back(pTexArray, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, true);
    m_pImmediateContext->TransitionResourceStates(static_cast<Uint32>(Barriers.size()), Barriers.data());

    // Set texture SRV in the SRB
    for (int tex = 0; tex < NumTextures; ++tex)
    {
        // Create one Shader Resource Binding for every texture
        // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
        m_pPSO[0][0]->CreateShaderResourceBinding(&m_SRB[tex], true);
        m_SRB[tex]->GetVariable(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV[tex]);
    }

    m_pPSO[1][0]->CreateShaderResourceBinding(&m_BatchSRB, true);
    m_BatchSRB->GetVariable(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TexArraySRV);

    // Create a tweak bar
    TwBar *bar = TwNewBar("Settings");
    int barSize[2] = {224 * m_UIScale, 120 * m_UIScale};
    TwSetParam(bar, NULL, "size", TW_PARAM_INT32, 2, barSize);

    // Add num Polygons control
    TwAddVarCB(bar, "Num Polygons", TW_TYPE_INT32, SetNumPolygons, GetNumPolygons, this, "min=1 max=100000 step=20");
    TwAddVarCB(bar, "Batch Size", TW_TYPE_INT32, SetBatchSize, GetBatchSize, this, "min=1 max=100");
    std::stringstream def;
    def << "min=0 max=" << m_MaxThreads;
    TwAddVarCB(bar, "Worker Threads", TW_TYPE_INT32, SetWorkerThreadCount, GetWorkerThreadCount, this, def.str().c_str());
    m_NumWorkerThreads = std::min(4, m_MaxThreads);
    
    if (pDevice->GetDeviceCaps().DevType == DeviceType::D3D12 || pDevice->GetDeviceCaps().DevType == DeviceType::Vulkan)
    {
        TwAddVarRW(bar, "Persistent map", TW_TYPE_BOOLCPP, &m_bAllowPersistentMap, "");
    }

    if (m_BatchSize > 1)
        CreateInstanceBuffer();

    StartWorkerThreads();
}

void Tutorial10_DataStreaming::InitializePolygonGeometry()
{
    m_PolygonGeo.resize(MaxPolygonVerts+1);
    for(Uint32 NumVerts = MinPolygonVerts; NumVerts <= MaxPolygonVerts; ++NumVerts)
    {
        auto& PolygonGeo = m_PolygonGeo[NumVerts];
        PolygonGeo.Verts.reserve(NumVerts);
        PolygonGeo.Inds.reserve( (NumVerts-2)*3);
        float ArcLen = static_cast<float>(M_PI*2.0) / static_cast<float>(NumVerts);
        float Angle = ((NumVerts % 2) == 1) ? (float)M_PI/2.f : (float)M_PI/2.f - ArcLen/2.f;
        for(Uint32 v=0; v < NumVerts; ++v, Angle += ArcLen)
        {
            PolygonGeo.Verts.emplace_back( float2{cosf(Angle), sinf(Angle)} );

            if(v < NumVerts-2)
            {
                PolygonGeo.Inds.push_back(0);
                PolygonGeo.Inds.push_back(v+1);
                PolygonGeo.Inds.push_back(v+2);
            }
        }
    }
}

void Tutorial10_DataStreaming::InitializePolygons()
{
    m_Polygons.resize(m_NumPolygons);

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(0); //Standard mersenne_twister_engine seeded with rd()
    std::uniform_real_distribution<float> scale_distr(0.01f, 0.05f);
    std::uniform_real_distribution<float> pos_distr(-0.95f, +0.95f);
    std::uniform_real_distribution<float> move_dir_distr(-0.1f, +0.1f);
    std::uniform_real_distribution<float> angle_distr(-static_cast<float>(M_PI), +static_cast<float>(M_PI));
    std::uniform_real_distribution<float> rot_distr(-static_cast<float>(M_PI)*0.5f, +static_cast<float>(M_PI)*0.5f);
    std::uniform_int_distribution<Int32> tex_distr(0, NumTextures-1);
    std::uniform_int_distribution<Int32> state_distr(0, NumStates - 1);
    std::uniform_int_distribution<Int32> num_verts_distr(MinPolygonVerts, MaxPolygonVerts);

    for (int Polygon = 0; Polygon < m_NumPolygons; ++Polygon)
    {
        auto& CurrInst = m_Polygons[Polygon];
        CurrInst.Size = scale_distr(gen);
        CurrInst.Angle = angle_distr(gen);
        CurrInst.Pos.x = pos_distr(gen);
        CurrInst.Pos.y = pos_distr(gen);
        CurrInst.MoveDir.x = move_dir_distr(gen);
        CurrInst.MoveDir.y = move_dir_distr(gen);
        CurrInst.RotSpeed = rot_distr(gen);
        // Texture array index
        CurrInst.TextureInd = tex_distr(gen);
        CurrInst.StateInd = state_distr(gen);
        CurrInst.NumVerts = num_verts_distr(gen);
    }
}

std::pair<Diligent::Uint32, Diligent::Uint32> Tutorial10_DataStreaming::WritePolygon(const PolygonGeometry& PolygonGeo, IDeviceContext* pCtx, size_t CtxNum)
{
    // Request memory for vertices and indices
    auto VBOffset = m_StreamingVB->Allocate(pCtx, static_cast<Uint32>(PolygonGeo.Verts.size()) * sizeof(float2), CtxNum);
    auto IBOffset = m_StreamingIB->Allocate(pCtx, static_cast<Uint32>(PolygonGeo.Inds.size()) * sizeof(Uint32), CtxNum);
    auto* VertexData = reinterpret_cast<float2*>(reinterpret_cast<Uint8*>(m_StreamingVB->GetMappedCPUAddress(CtxNum)) + VBOffset);
    auto* IndexData  = reinterpret_cast<Uint32*>(reinterpret_cast<Uint8*>(m_StreamingIB->GetMappedCPUAddress(CtxNum)) + IBOffset);
    memcpy(VertexData, PolygonGeo.Verts.data(), PolygonGeo.Verts.size() * sizeof(float2));
    memcpy(IndexData, PolygonGeo.Inds.data(), PolygonGeo.Inds.size() * sizeof(Uint32));

    m_StreamingVB->Release(CtxNum);
    m_StreamingIB->Release(CtxNum);

    return {VBOffset, IBOffset};
}

void Tutorial10_DataStreaming::UpdatePolygons(float elapsedTime)
{
    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()

    std::uniform_real_distribution<float> rot_distr(-static_cast<float>(M_PI)*0.5f, +static_cast<float>(M_PI)*0.5f);
    for (int Polygon = 0; Polygon < m_NumPolygons; ++Polygon)
    {
        auto &CurrInst = m_Polygons[Polygon];
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

void Tutorial10_DataStreaming::StartWorkerThreads()
{
    m_WorkerThreads.resize(m_NumWorkerThreads);
    for(Uint32 t=0; t < m_WorkerThreads.size(); ++t)
    {
        m_WorkerThreads[t] = std::thread(WorkerThreadFunc, this, t );
    }
    m_CmdLists.resize(m_NumWorkerThreads);
}

void Tutorial10_DataStreaming::StopWorkerThreads()
{
    m_RenderSubsetSignal.Trigger(true, -1);

    for(auto &thread : m_WorkerThreads)
    {
        thread.join();
    }
    m_RenderSubsetSignal.Reset();
}

void Tutorial10_DataStreaming::WorkerThreadFunc(Tutorial10_DataStreaming *pThis, Uint32 ThreadNum)
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
        if(pThis->m_BatchSize > 1)
            pThis->RenderSubset<true>(pDeferredCtx, 1 + ThreadNum);
        else
            pThis->RenderSubset<false>(pDeferredCtx, 1 + ThreadNum);

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

template<bool UseBatch>
void Tutorial10_DataStreaming::RenderSubset(IDeviceContext *pCtx, Uint32 Subset)
{
    // Deferred contexts start in default state. We must bind everything to the context
    // Render targets are set and transitioned to correct states by the main thread, here we only verify states
    pCtx->SetRenderTargets(0, nullptr, nullptr, SET_RENDER_TARGETS_FLAG_VERIFY_STATES);

    DrawAttribs DrawAttrs;
    DrawAttrs.IsIndexed = true;
    DrawAttrs.IndexType = VT_UINT32;
    DrawAttrs.Flags = DRAW_FLAG_VERIFY_STATES;

    Uint32 NumSubsets = 1 + m_NumWorkerThreads;
    const Uint32 TotalPolygons = static_cast<Uint32>(m_Polygons.size());
    const Uint32 TotalBatches = (TotalPolygons + m_BatchSize - 1) / m_BatchSize;
    const Uint32 SusbsetSize = TotalBatches / NumSubsets;
    const Uint32 StartBatch = SusbsetSize * Subset;
    const Uint32 EndBatch = (Subset < NumSubsets-1) ? SusbsetSize * (Subset+1) : TotalBatches;
    for(Uint32 batch = StartBatch; batch < EndBatch; ++batch)
    {
        const Uint32 StartInst = batch * m_BatchSize;
        const Uint32 EndInst = std::min(StartInst + static_cast<Uint32>(m_BatchSize), static_cast<Uint32>(m_NumPolygons));

        // Set pipeline state
        auto StateInd = m_Polygons[StartInst].StateInd;
        pCtx->SetPipelineState(m_pPSO[UseBatch ? 1 : 0][StateInd]);

        const auto& PolygonGeo = m_PolygonGeo[m_Polygons[StartInst].NumVerts];
        auto Offsets = WritePolygon(PolygonGeo, pCtx, Subset);
        Uint32 offsets[] = { Offsets.first, 0 };
        IBuffer *pBuffs[] = { m_StreamingVB->GetBuffer(), m_BatchDataBuffer };
        pCtx->SetVertexBuffers(0, UseBatch ? 2 : 1, pBuffs, offsets, SET_VERTEX_BUFFERS_FLAG_RESET);

        pCtx->SetIndexBuffer(m_StreamingIB->GetBuffer(), Offsets.second);

        MapHelper<InstanceData> BatchData;
        if (UseBatch)
        {
            pCtx->CommitShaderResources(m_BatchSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
            BatchData.Map(pCtx, m_BatchDataBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
        }

        for (Uint32 inst = StartInst; inst < EndInst; ++inst)
        {
            const auto &CurrInstData = m_Polygons[inst];
            // Shader resources have been explicitly transitioned to correct states, so
            // RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode is not needed.
            // Instead, we use RESOURCE_STATE_TRANSITION_MODE_VERIFY mode to
            // verify that all resources are in correct states. This mode only has effect
            // in debug and development builds
            if(!UseBatch)
                pCtx->CommitShaderResources(m_SRB[CurrInstData.TextureInd], RESOURCE_STATE_TRANSITION_MODE_VERIFY);
            
            {
                float2x2 ScaleMatr =
                    float2x2(CurrInstData.Size, 0.f,
                        0.f, CurrInstData.Size);
                float sinAngle = sinf(CurrInstData.Angle);
                float cosAngle = cosf(CurrInstData.Angle);
                float2x2 RotMatr(cosAngle, -sinAngle,
                    sinAngle, cosAngle);
                auto Matr = ScaleMatr * RotMatr;
                float4 PolygonRotationAndScale(Matr._m00, Matr._m10, Matr._m01, Matr._m11);

                if (UseBatch)
                {
                    auto& CurrPolygon = BatchData[inst - StartInst];
                    CurrPolygon.PolygonRotationAndScale = PolygonRotationAndScale;
                    CurrPolygon.PolygonCenter = CurrInstData.Pos;
                    CurrPolygon.TexArrInd = static_cast<float>(CurrInstData.TextureInd);
                }
                else
                {
                    struct PolygonAttribs
                    {
                        float4 g_PolygonRotationAndScale;
                        float4 g_PolygonCenter;
                    };
                    
                    // Map the buffer and write current world-view-projection matrix
                    MapHelper<PolygonAttribs> InstData(pCtx, m_PolygonAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);

                    InstData->g_PolygonRotationAndScale = PolygonRotationAndScale;
                    InstData->g_PolygonCenter.x = CurrInstData.Pos.x;
                    InstData->g_PolygonCenter.y = CurrInstData.Pos.y;
                }
            }
        }
        
        if (UseBatch)
            BatchData.Unmap();

        // Note that since we transitioned vertex and index buffers to correct states, we do not 
        // use DRAW_FLAG_TRANSITION_INDEX_BUFFER and DRAW_FLAG_TRANSITION_VERTEX_BUFFERS
        // flags
        DrawAttrs.NumIndices = static_cast<Uint32>(PolygonGeo.Inds.size());
        DrawAttrs.NumInstances = EndInst - StartInst;
        pCtx->Draw(DrawAttrs);
    }

    m_StreamingVB->Flush(Subset);
    m_StreamingIB->Flush(Subset);
}

// Render a frame
void Tutorial10_DataStreaming::Render()
{
    // Clear the back buffer 
    const float ClearColor[] = {  0.350f,  0.350f,  0.350f, 1.0f }; 
    m_pImmediateContext->ClearRenderTarget(nullptr, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG | CLEAR_DEPTH_STENCIL_TRANSITION_STATE_FLAG, 1.f);

    m_StreamingIB->AllowPersistentMapping(m_bAllowPersistentMap);
    m_StreamingVB->AllowPersistentMapping(m_bAllowPersistentMap);

    if (m_NumWorkerThreads > 0)
    {
        m_NumThreadsCompleted = 0;
        m_RenderSubsetSignal.Trigger(true);
    }

    if(m_BatchSize>1)
        RenderSubset<true>(m_pImmediateContext, 0);
    else
        RenderSubset<false>(m_pImmediateContext, 0);

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

// Callback function called by AntTweakBar to set the grid size
void Tutorial10_DataStreaming::SetNumPolygons(const void *value, void * clientData)
{
    Tutorial10_DataStreaming *pTheTutorial = reinterpret_cast<Tutorial10_DataStreaming*>( clientData );
    pTheTutorial->m_NumPolygons = *static_cast<const int *>(value);
    pTheTutorial->InitializePolygons();
}

// Callback function called by AntTweakBar to get the grid size
void Tutorial10_DataStreaming::GetNumPolygons(void *value, void * clientData)
{
    Tutorial10_DataStreaming *pTheTutorial = reinterpret_cast<Tutorial10_DataStreaming*>( clientData );
    *static_cast<int*>(value) = pTheTutorial->m_NumPolygons;
}

void Tutorial10_DataStreaming::CreateInstanceBuffer()
{
    // Create instance data buffer that will store transformation matrices
    BufferDesc InstBuffDesc;
    InstBuffDesc.Name = "Batch data buffer";
    // Use default usage as this buffer will only be updated when grid size changes
    InstBuffDesc.Usage = USAGE_DYNAMIC;
    InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    InstBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
    InstBuffDesc.uiSizeInBytes = sizeof(InstanceData) * m_BatchSize;
    m_BatchDataBuffer.Release();
    m_pDevice->CreateBuffer(InstBuffDesc, BufferData(), &m_BatchDataBuffer);
}

void Tutorial10_DataStreaming::SetBatchSize(const void *value, void * clientData)
{
    Tutorial10_DataStreaming *pTheTutorial = reinterpret_cast<Tutorial10_DataStreaming*>(clientData);
    pTheTutorial->m_BatchSize = *static_cast<const int *>(value);
    pTheTutorial->CreateInstanceBuffer();
}
void Tutorial10_DataStreaming::GetBatchSize(void *value, void * clientData)
{
    Tutorial10_DataStreaming *pTheTutorial = reinterpret_cast<Tutorial10_DataStreaming*>(clientData);
    *static_cast<int*>(value) = pTheTutorial->m_BatchSize;
}

void Tutorial10_DataStreaming::SetWorkerThreadCount(const void *value, void * clientData)
{
    Tutorial10_DataStreaming *pTheTutorial = reinterpret_cast<Tutorial10_DataStreaming*>( clientData );
    pTheTutorial->StopWorkerThreads();
    pTheTutorial->m_NumWorkerThreads = *static_cast<const int *>(value);
    pTheTutorial->StartWorkerThreads();
}

void Tutorial10_DataStreaming::GetWorkerThreadCount(void *value, void * clientData)
{
    Tutorial10_DataStreaming *pTheTutorial = reinterpret_cast<Tutorial10_DataStreaming*>( clientData );
    *static_cast<int*>(value) = pTheTutorial->m_NumWorkerThreads;
}


void Tutorial10_DataStreaming::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdatePolygons(static_cast<float>(ElapsedTime));
}
