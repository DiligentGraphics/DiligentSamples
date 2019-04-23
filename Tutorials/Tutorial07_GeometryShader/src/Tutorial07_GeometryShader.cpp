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

#include "Tutorial07_GeometryShader.h"
#include "MapHelper.h"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "AntTweakBar.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial07_GeometryShader();
}

namespace
{
struct Constants
{
    float4x4 WorldViewProj;
    float4 ViewportSize;
    float LineWidth;
};
}

void Tutorial07_GeometryShader::GetEngineInitializationAttribs(DeviceType         DevType,
                                                               EngineCreateInfo&  Attribs)
{
    SampleBase::GetEngineInitializationAttribs(DevType, Attribs);
#if VULKAN_SUPPORTED
    if(DevType == DeviceType::Vulkan)
    {
        auto& VkAttrs = static_cast<EngineVkCreateInfo&>(Attribs);
        VkAttrs.EnabledFeatures.geometryShader = true;
    }
#endif
}

void Tutorial07_GeometryShader::Initialize(IEngineFactory*   pEngineFactory,
                                           IRenderDevice*    pDevice,
                                           IDeviceContext**  ppContexts,
                                           Uint32            NumDeferredCtx,
                                           ISwapChain*       pSwapChain)
{
    const auto& deviceCaps = pDevice->GetDeviceCaps();
    if(!deviceCaps.bGeometryShadersSupported)
    {
        throw std::runtime_error("Geometry shaders are not supported");
    }

    SampleBase::Initialize(pEngineFactory, pDevice, ppContexts, NumDeferredCtx, pSwapChain);

    {
        // Pipeline state object encompasses configuration of all GPU stages

        PipelineStateDesc PSODesc;
        // Pipeline state name is used by the engine to report issues
        // It is always a good idea to give objects descriptive names
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

        // Create dynamic uniform buffer that will store shader constants
        CreateUniformBuffer(pDevice, sizeof(Constants), "Shader constants CB", &m_ShaderConstants);

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
            pDevice->CreateShader(ShaderCI, &pVS);
        }

        // Create geometry shader
        RefCntAutoPtr<IShader> pGS;
        {
            ShaderCI.Desc.ShaderType = SHADER_TYPE_GEOMETRY;
            ShaderCI.EntryPoint      = "main";
            ShaderCI.Desc.Name       = "Cube GS";
            ShaderCI.FilePath        = "cube.gsh";
            pDevice->CreateShader(ShaderCI, &pGS);
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
            // Attribute 1 - texture coordinates
            LayoutElement{1, 0, 2, VT_FLOAT32, False}
        };

        PSODesc.GraphicsPipeline.pVS = pVS;
        PSODesc.GraphicsPipeline.pGS = pGS;
        PSODesc.GraphicsPipeline.pPS = pPS;
        PSODesc.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
        PSODesc.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

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

        pDevice->CreatePipelineState(PSODesc, &m_pPSO);

        // Since we did not explcitly specify the type for Constants, default type
        // (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables never change and are bound directly
        // to the pipeline state object.
        m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX,   "VSConstants")->Set(m_ShaderConstants);
        m_pPSO->GetStaticVariableByName(SHADER_TYPE_GEOMETRY, "GSConstants")->Set(m_ShaderConstants);
        m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL,    "PSConstants")->Set(m_ShaderConstants);
    }

    {
        // Layout of this structure matches the one we defined in pipeline state
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

        // This time we have to duplicate verices because texture coordinates cannot
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
        VBData.pData    = CubeVerts;
        VBData.DataSize = sizeof(CubeVerts);
        pDevice->CreateBuffer(VertBuffDesc, &VBData, &m_CubeVertexBuffer);
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
        IndBuffDesc.Name          = "Cube index buffer";
        IndBuffDesc.Usage         = USAGE_STATIC;
        IndBuffDesc.BindFlags     = BIND_INDEX_BUFFER;
        IndBuffDesc.uiSizeInBytes = sizeof(Indices);
        BufferData IBData;
        IBData.pData    = Indices;
        IBData.DataSize = sizeof(Indices);
        pDevice->CreateBuffer(IndBuffDesc, &IBData, &m_CubeIndexBuffer);
    }

    {
        // Load texture
        TextureLoadInfo loadInfo;
        loadInfo.IsSRGB = true;
        RefCntAutoPtr<ITexture> Tex;
        CreateTextureFromFile("DGLogo.png", loadInfo, m_pDevice, &Tex);
        // Get shader resource view from the texture
        m_TextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    }

    // Since we are using mutable variable, we must create shader resource binding object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    m_pPSO->CreateShaderResourceBinding(&m_SRB, true);
    // Set texture SRV in the SRB
    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);

    // Create a tweak bar
    TwBar *bar = TwNewBar("Settings");
    int barSize[2] = {224 * m_UIScale, 120 * m_UIScale};
    TwSetParam(bar, NULL, "size", TW_PARAM_INT32, 2, barSize);

    // Add grid size control
    TwAddVarRW(bar, "Line Width", TW_TYPE_FLOAT, &m_LineWidth, "min=1 max=10");
}

// Render a frame
void Tutorial07_GeometryShader::Render()
{
    // Clear the back buffer 
    const float ClearColor[] = { 0.350f,  0.350f,  0.350f, 1.0f }; 
    m_pImmediateContext->ClearRenderTarget(nullptr, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<Constants> Consts(m_pImmediateContext, m_ShaderConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        Consts->WorldViewProj = m_WorldViewProjMatrix.Transpose();
        
        const auto &SCDesc = m_pSwapChain->GetDesc();
        Consts->ViewportSize = float4(static_cast<float>(SCDesc.Width), static_cast<float>(SCDesc.Height), 1.f/static_cast<float>(SCDesc.Width), 1.f/static_cast<float>(SCDesc.Height));
        
        Consts->LineWidth = m_LineWidth;
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
    m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawAttribs DrawAttrs;
    DrawAttrs.IsIndexed  = true; // This is indexed draw call
    DrawAttrs.IndexType  = VT_UINT32; // Index type
    DrawAttrs.NumIndices = 36;
    // Verify the state of vertex and index buffers
    DrawAttrs.Flags      = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->Draw(DrawAttrs);
}

void Tutorial07_GeometryShader::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    const bool IsGL = m_pDevice->GetDeviceCaps().IsGLDevice();

    // Set cube world view matrix
    float4x4 CubeWorldView = float4x4::RotationY_D3D( static_cast<float>(CurrTime) * 1.0f) * float4x4::RotationX_D3D(-PI_F*0.1f) * 
        float4x4::TranslationD3D(0.f, 0.0f, 5.0f);
    float NearPlane = 0.1f;
    float FarPlane = 100.f;
    float aspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);
    // Projection matrix differs between DX and OpenGL
    auto Proj = float4x4::ProjectionD3D(PI_F / 4.f, aspectRatio, NearPlane, FarPlane, IsGL);
    // Compute world-view-projection matrix
    m_WorldViewProjMatrix = CubeWorldView * Proj;
}

}
