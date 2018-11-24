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

#include "Tutorial05_TextureArray.h"
#include "MapHelper.h"
#include "BasicShaderSourceStreamFactory.h"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "AntTweakBar.h"

using namespace Diligent;

SampleBase* CreateSample()
{
    return new Tutorial05_TextureArray();
}

namespace
{

struct InstanceData
{
    float4x4 Matrix;
    float TextureInd;
};

}

void Tutorial05_TextureArray::Initialize(IRenderDevice *pDevice, IDeviceContext **ppContexts, Uint32 NumDeferredCtx, ISwapChain *pSwapChain)
{
    SampleBase::Initialize(pDevice, ppContexts, NumDeferredCtx, pSwapChain);

    {
        // Pipeline state object encompasses configuration of all GPU stages

        PipelineStateDesc PSODesc;
        // This is a graphics pipeline
        PSODesc.IsComputePipeline = false; 

        // Pipeline state name is used by the engine to report issues
        // It is always a good idea to give objects descriptive names
        PSODesc.Name = "Cube PSO"; 

        // This tutorial will render to a single render target
        PSODesc.GraphicsPipeline.NumRenderTargets = 1;
        // Set render target format which is the format of the swap chain's color buffer
        PSODesc.GraphicsPipeline.RTVFormats[0] = pSwapChain->GetDesc().ColorBufferFormat;
        // Set depth buffer format which is the format of the swap chain's back buffer
        PSODesc.GraphicsPipeline.DSVFormat = pSwapChain->GetDesc().DepthBufferFormat;
        // Primitive topology defines what kind of primitives will be rendered by this pipeline state
        PSODesc.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        // Cull back faces
        PSODesc.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
        // Enable depth testing
        PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

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
        RefCntAutoPtr<IShader> pVS;
        {
            CreationAttribs.Desc.ShaderType = SHADER_TYPE_VERTEX;
            CreationAttribs.EntryPoint = "main";
            CreationAttribs.Desc.Name = "Cube VS";
            CreationAttribs.FilePath = "cube_inst.vsh";
            pDevice->CreateShader(CreationAttribs, &pVS);
            // Create dynamic uniform buffer that will store our transformation matrix
            // Dynamic buffers can be frequently updated by the CPU
            CreateUniformBuffer(pDevice, sizeof(float4x4)*2, "VS constants CB", &m_VSConstants);
            // Since we did not explcitly specify the type for Constants, default type 
            // (SHADER_VARIABLE_TYPE_STATIC) will be used. Static variables never change and are bound directly
            // through the shader (http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/)
            pVS->GetShaderVariable("Constants")->Set(m_VSConstants);
        }

        // Create pixel shader
        RefCntAutoPtr<IShader> pPS;
        {
            CreationAttribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
            CreationAttribs.EntryPoint = "main";
            CreationAttribs.Desc.Name = "Cube PS";
            CreationAttribs.FilePath = "cube_inst.psh";
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
            LayoutElement(1, 0, 2, VT_FLOAT32, False),
            
            // Per-instance data - second buffer slot
            // We will use four attributes to encode instance-specific 4x4 transformation matrix
            // Attribute 2 - first row
            LayoutElement(2, 1, 4, VT_FLOAT32, False, 0, 0, LayoutElement::FREQUENCY_PER_INSTANCE),
            // Attribute 3 - second row
            LayoutElement(3, 1, 4, VT_FLOAT32, False, 0, 0, LayoutElement::FREQUENCY_PER_INSTANCE),
            // Attribute 4 - third row
            LayoutElement(4, 1, 4, VT_FLOAT32, False, 0, 0, LayoutElement::FREQUENCY_PER_INSTANCE),
            // Attribute 5 - fourth row
            LayoutElement(5, 1, 4, VT_FLOAT32, False, 0, 0, LayoutElement::FREQUENCY_PER_INSTANCE),
            // Attribute 6 - texture array index
            LayoutElement(6, 1, 1, VT_FLOAT32, False, 0, 0, LayoutElement::FREQUENCY_PER_INSTANCE),
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
        // Create instance data buffer that will store transformation matrices
        BufferDesc InstBuffDesc;
        InstBuffDesc.Name = "Instance data buffer";
        // Use default usage as this buffer will only be updated when grid size changes
        InstBuffDesc.Usage = USAGE_DEFAULT; 
        InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        InstBuffDesc.uiSizeInBytes = sizeof(InstanceData) * MaxInstances;
        pDevice->CreateBuffer(InstBuffDesc, BufferData(), &m_InstanceBuffer);
        PopulateInstanceBuffer();
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

    // Load texture array
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
        for(Uint32 mip=0; mip < TexDesc.MipLevels; ++mip)
        {
            pTexArray->CopyData(m_pImmediateContext, SrcTex, mip, 0, nullptr, mip, tex, 0, 0, 0);
        }
    }
    // Get shader resource view from the texture array
    m_TextureSRV = pTexArray->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    // Since we are using mutable variable, we must create shader resource binding object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    m_pPSO->CreateShaderResourceBinding(&m_SRB, true);
    // Set texture SRV in the SRB
    m_SRB->GetVariable(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);

    // Create a tweak bar
    TwBar *bar = TwNewBar("Settings");
    int barSize[2] = {224 * m_UIScale, 120 * m_UIScale};
    TwSetParam(bar, NULL, "size", TW_PARAM_INT32, 2, barSize);

    // Add grid size control
    TwAddVarCB(bar, "Grid Size", TW_TYPE_INT32, SetGridSize, GetGridSize, this, "min=1 max=32");
}

void Tutorial05_TextureArray::PopulateInstanceBuffer()
{
    // Populate instance data buffer
    std::vector<InstanceData> InstanceData(m_GridSize*m_GridSize*m_GridSize);
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
                auto &CurrInst = InstanceData[instId++];
                CurrInst.Matrix = matrix;
                // Texture array index
                CurrInst.TextureInd = static_cast<float>(tex_distr(gen));
            }
        }
    }
    // Update instance data buffer
    Uint32 DataSize = static_cast<Uint32>(sizeof(InstanceData[0]) * InstanceData.size());
    m_InstanceBuffer->UpdateData(m_pImmediateContext, 0, DataSize, InstanceData.data());
}


// Render a frame
void Tutorial05_TextureArray::Render()
{
    // Clear the back buffer 
    const float ClearColor[] = {  0.350f,  0.350f,  0.350f, 1.0f }; 
    m_pImmediateContext->ClearRenderTarget(nullptr, ClearColor);
    m_pImmediateContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG, 1.f);

    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants[0] = transposeMatrix(m_ViewProjMatrix);
        CBConstants[1] = transposeMatrix(m_RotationMatrix);
    }

    // Bind vertex & instance buffers
    Uint32 offsets[] = {0, 0};
    IBuffer *pBuffs[] = {m_CubeVertexBuffer, m_InstanceBuffer};
    m_pImmediateContext->SetVertexBuffers(0, _countof(pBuffs), pBuffs, offsets, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0);
    
    // Set pipeline state
    m_pImmediateContext->SetPipelineState(m_pPSO);
    // Commit shader resources. Pass pointer to the shader resource binding object
    // COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES flag needs to be specified to make sure
    // that resources are transitioned to proper states
    m_pImmediateContext->CommitShaderResources(m_SRB, COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES);

    DrawAttribs DrawAttrs;
    DrawAttrs.IsIndexed = true; // This is indexed draw call
    DrawAttrs.IndexType = VT_UINT32; // Index type
    DrawAttrs.NumIndices = 36;
    DrawAttrs.NumInstances = m_GridSize*m_GridSize*m_GridSize; // Specify number of instances
    // Transition vertex and index buffer to required states
    DrawAttrs.Flags = DRAW_FLAG_TRANSITION_INDEX_BUFFER | DRAW_FLAG_TRANSITION_VERTEX_BUFFERS;
    m_pImmediateContext->Draw(DrawAttrs);
}

// Callback function called by AntTweakBar to set the grid size
void Tutorial05_TextureArray::SetGridSize(const void *value, void * clientData)
{
    Tutorial05_TextureArray *pTheTutorial = reinterpret_cast<Tutorial05_TextureArray*>( clientData );
    pTheTutorial->m_GridSize = *static_cast<const int *>(value);
    pTheTutorial->PopulateInstanceBuffer();
}

// Callback function called by AntTweakBar to get the grid size
void Tutorial05_TextureArray::GetGridSize(void *value, void * clientData)
{
    Tutorial05_TextureArray *pTheTutorial = reinterpret_cast<Tutorial05_TextureArray*>( clientData );
    *static_cast<int*>(value) = pTheTutorial->m_GridSize;
}


void Tutorial05_TextureArray::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    const bool IsGL = m_pDevice->GetDeviceCaps().IsGLDevice();

    // Set cube view matrix
    float4x4 View = rotationX(+0.6f) * translationMatrix(0.f, 0.f, 4.0f);

    float NearPlane = 0.1f;
    float FarPlane = 100.f;
    float aspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);
    // Projection matrix differs between DX and OpenGL
    auto Proj = Projection(PI_F / 4.f, aspectRatio, NearPlane, FarPlane, IsGL);
    // Compute view-projection matrix
    m_ViewProjMatrix = View * Proj;

    // Global rotation matrix
    m_RotationMatrix = rotationY( -static_cast<float>(CurrTime) * 1.0f) * rotationX(static_cast<float>(CurrTime)*0.25f);
}
