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

#include "Tutorial02_Cube.h"
#include "MapHelper.h"
#include "BasicShaderSourceStreamFactory.h"
#include "GraphicsUtilities.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial02_Cube();
}


void Tutorial02_Cube::Initialize(IRenderDevice*    pDevice,
                                 IDeviceContext**  ppContexts,
                                 Uint32            NumDeferredCtx,
                                 ISwapChain*       pSwapChain)
{
    SampleBase::Initialize(pDevice, ppContexts, NumDeferredCtx, pSwapChain);

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
        PSODesc.GraphicsPipeline.RTVFormats[0]                = pSwapChain->GetDesc().ColorBufferFormat;
        // Set depth buffer format which is the format of the swap chain's back buffer
        PSODesc.GraphicsPipeline.DSVFormat                    = pSwapChain->GetDesc().DepthBufferFormat;
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
        BasicShaderSourceStreamFactory BasicSSSFactory;
        ShaderCI.pShaderSourceStreamFactory = &BasicSSSFactory;
        // Create vertex shader
        RefCntAutoPtr<IShader> pVS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "Cube VS";
            ShaderCI.FilePath        = "cube.vsh";
            pDevice->CreateShader(ShaderCI, &pVS);
            // Create dynamic uniform buffer that will store our transformation matrix
            // Dynamic buffers can be frequently updated by the CPU
            BufferDesc CBDesc;
            CBDesc.Name           = "VS constants CB";
            CBDesc.uiSizeInBytes  = sizeof(float4x4);
            CBDesc.Usage          = USAGE_DYNAMIC;
            CBDesc.BindFlags      = BIND_UNIFORM_BUFFER;
            CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            pDevice->CreateBuffer(CBDesc, nullptr, &m_VSConstants);
        }

        // Create pixel shader
        RefCntAutoPtr<IShader> pPS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "Cube PS";
            ShaderCI.FilePath        = "cube.psh";
            pDevice->CreateShader(ShaderCI, &pPS);
        }

        // Define vertex shader input layout
        LayoutElement LayoutElems[] =
        {
            // Attribute 0 - vertex position
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            // Attribute 1 - vertex color
            LayoutElement{1, 0, 4, VT_FLOAT32, False}
        };
        PSODesc.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
        PSODesc.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

        PSODesc.GraphicsPipeline.pVS = pVS;
        PSODesc.GraphicsPipeline.pPS = pPS;

        // Define variable type that will be used by default
        PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

        pDevice->CreatePipelineState(PSODesc, &m_pPSO);

        // Since we did not explcitly specify the type for Constants, default type
        // (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never change and are bound directly
        // through the pipeline state object.
        m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);
        
        // Create shader resource binding object and bind all static resources in it
        m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);
    }

    {
        // Layout of this structure matches the one we defined in the pipeline state
        struct Vertex
        {
            float3 pos;
            float4 color;
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

        Vertex CubeVerts[8] =
        {
            {float3(-1,-1,-1), float4(1,0,0,1)},
            {float3(-1,+1,-1), float4(0,1,0,1)},
            {float3(+1,+1,-1), float4(0,0,1,1)},
            {float3(+1,-1,-1), float4(1,1,1,1)},

            {float3(-1,-1,+1), float4(1,1,0,1)},
            {float3(-1,+1,+1), float4(0,1,1,1)},
            {float3(+1,+1,+1), float4(1,0,1,1)},
            {float3(+1,-1,+1), float4(0.2f,0.2f,0.2f,1)},
        };
        // Create vertex buffer that stores cube vertices
        BufferDesc VertBuffDesc;
        VertBuffDesc.Name          = "Cube vertex buffer";
        VertBuffDesc.Usage         = USAGE_STATIC;
        VertBuffDesc.BindFlags     = BIND_VERTEX_BUFFER;
        VertBuffDesc.uiSizeInBytes = sizeof(CubeVerts);
        BufferData VBData;
        VBData.pData    = CubeVerts;
        VBData.DataSize = sizeof(CubeVerts);
        pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_CubeVertexBuffer);
    }

    {
        // Indices
        Uint32 Indices[] =
        {
            2,0,1, 2,3,0,
            4,6,5, 4,7,6,
            0,7,4, 0,3,7,
            1,0,4, 1,4,5,
            1,5,2, 5,6,2,
            3,6,7, 3,2,6
        };
        // Create index buffer
        BufferDesc IndBuffDesc;
        IndBuffDesc.Name          = "Cube index buffer";
        IndBuffDesc.Usage         = USAGE_STATIC;
        IndBuffDesc.BindFlags     = BIND_INDEX_BUFFER;
        IndBuffDesc.uiSizeInBytes = sizeof(Indices);
        BufferData IBData;
        IBData.pData    = Indices;
        IBData.DataSize = sizeof(Indices);
        pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_CubeIndexBuffer);
    }
}

// Render a frame
void Tutorial02_Cube::Render()
{
    // Clear the back buffer 
    const float ClearColor[] = {  0.350f,  0.350f,  0.350f, 1.0f }; 
    m_pImmediateContext->ClearRenderTarget(nullptr, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        *CBConstants = transposeMatrix(m_WorldViewProjMatrix);
    }

    // Bind vertex buffer
    Uint32 offset = 0;
    IBuffer *pBuffs[] = {m_CubeVertexBuffer};
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set pipeline state
    m_pImmediateContext->SetPipelineState(m_pPSO);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode 
    // makes sure that resources are transitioned to required states.
    m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    DrawAttribs DrawAttrs;
    DrawAttrs.IsIndexed  = true;      // This is an indexed draw call
    DrawAttrs.IndexType  = VT_UINT32; // Index type
    DrawAttrs.NumIndices = 36;
    // Verify the state of vertex and index buffers
    DrawAttrs.Flags      = DRAW_FLAG_VERIFY_STATES;
    m_pImmediateContext->Draw(DrawAttrs);
}

void Tutorial02_Cube::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    const bool IsGL = m_pDevice->GetDeviceCaps().IsGLDevice();

    // Set cube world view matrix
    float4x4 CubeWorldView = rotationY( static_cast<float>(CurrTime) * 1.0f) * rotationX(-PI_F*0.1f) * 
        translationMatrix(0.f, 0.0f, 5.0f);
    float NearPlane = 0.1f;
    float FarPlane = 100.f;
    float aspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);
    // Projection matrix differs between DX and OpenGL
    auto Proj = Projection(PI_F / 4.f, aspectRatio, NearPlane, FarPlane, IsGL);
    // Compute world-view-projection matrix
    m_WorldViewProjMatrix = CubeWorldView * Proj;
}

}
