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

#include "Tutorial12_RenderTarget.h"
#include "MapHelper.h"
#include "BasicShaderSourceStreamFactory.h"
#include "GraphicsUtilities.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial12_RenderTarget();
}

void Tutorial12_RenderTarget::Initialize(IRenderDevice*    pDevice,
                                         IDeviceContext**  ppContexts,
                                         Uint32            NumDeferredCtx,
                                         ISwapChain*       pSwapChain)
{
    SampleBase::Initialize(pDevice, ppContexts, NumDeferredCtx, pSwapChain);
    
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
        // Set render target format which is the format of the render target's color buffer
        PSODesc.GraphicsPipeline.RTVFormats[0] = TEX_FORMAT_RGBA8_UNORM;
        // Set depth buffer format which is the format of the render target's depth buffer
        PSODesc.GraphicsPipeline.DSVFormat = TEX_FORMAT_D32_FLOAT;
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
            CreationAttribs.FilePath = "cube.vsh";
            pDevice->CreateShader(CreationAttribs, &pVS);

            // Create dynamic uniform buffer that will store our transformation matrix
            // Dynamic buffers can be frequently updated by the CPU
            BufferDesc CBDesc;
            CBDesc.Name = "VS constants CB";
            CBDesc.uiSizeInBytes = sizeof(float4x4);
            CBDesc.Usage = USAGE_DYNAMIC;
            CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
            CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            pDevice->CreateBuffer( CBDesc, nullptr, &m_VSConstants );

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
            CreationAttribs.FilePath = "cube.psh";
            pDevice->CreateShader(CreationAttribs, &pPS);
        }

        PSODesc.GraphicsPipeline.pVS = pVS;
        PSODesc.GraphicsPipeline.pPS = pPS;
        pDevice->CreatePipelineState(PSODesc, &m_pPSO);

        // Create shader resource binding object and bind all static resources in it
        m_pPSO->CreateShaderResourceBinding(&m_pSRB, true);
    }

    {
        // Render target color attachment
        RefCntAutoPtr<ITexture> pRTColor;
        // Render target color attachment descriptor
        TextureDesc RTColorDesc;
        // The render target's attachments are 2D textures
        RTColorDesc.Type = RESOURCE_DIM_TEX_2D;
        // The render target's attachments' width is that of the swap chain
        RTColorDesc.Width = pSwapChain->GetDesc().Width;
        // The render target's attachments' height is that of the swap chain
        RTColorDesc.Height = pSwapChain->GetDesc().Height;
        // The render target's attachments only have one mipmap
        RTColorDesc.MipLevels = 1;
        // The render target's color buffer format is 8 bits RGBA
        RTColorDesc.Format = TEX_FORMAT_RGBA8_UNORM;
        // The render target's color buffer can be bound as a shader resource and as a render target
        RTColorDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
        // Let's create the render target's color buffer
        pDevice->CreateTexture(RTColorDesc, nullptr, &pRTColor);
        // Store the render target color attachment
        m_pRTColorAttachment = pRTColor->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);

        // Render target depth attachment
        RefCntAutoPtr<ITexture> pRTDepth;
        // Render target depth attachment descriptor
        TextureDesc RTDepthDesc = RTColorDesc;
        // The render target's depth buffer format is 8 bits RGBA
        RTDepthDesc.Format = TEX_FORMAT_D32_FLOAT;
        // The render target's depth buffer can be bound as a shader resource and as a render target
        RTDepthDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
        // Let's create the render target's depth buffer
        pDevice->CreateTexture(RTDepthDesc, nullptr, &pRTDepth);
        // Store the render target color attachment
        m_pRTDepthAttachment = pRTDepth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);

        // Pipeline state object encompasses configuration of all GPU stages
        PipelineStateDesc RTPSODesc;
        // Pipeline state name is used by the engine to report issues
        // It is always a good idea to give objects descriptive names
        RTPSODesc.Name = "Render Target PSO";
        // This is a graphics pipeline
        RTPSODesc.IsComputePipeline = false;
        // This tutorial will render to a single render target
        RTPSODesc.GraphicsPipeline.NumRenderTargets = 1;
        // Set render target format which is the format of the swap chain's color buffer
        RTPSODesc.GraphicsPipeline.RTVFormats[0] = pSwapChain->GetDesc().ColorBufferFormat;
        // Set depth buffer format which is the format of the swap chain's back buffer
        RTPSODesc.GraphicsPipeline.DSVFormat = pSwapChain->GetDesc().DepthBufferFormat;
        // Primitive topology defines what kind of primitives will be rendered by this pipeline state
        RTPSODesc.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
        // Cull back faces
        RTPSODesc.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_BACK;
        // Enable depth testing
        RTPSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

        ShaderCreationAttribs CreationAttribs;
        // Tell the system that the shader source code is in HLSL.
        // For OpenGL, the engine will convert this into GLSL behind the scene
        CreationAttribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
        // Enforce the use of combined texture samplers for OpenGL backends
        CreationAttribs.UseCombinedTextureSamplers = true;
        // In this tutorial, we will load shaders from file. To be able to do that,
        // we need to create a shader source stream factory
        BasicShaderSourceStreamFactory BasicSSSFactory;
        CreationAttribs.pShaderSourceStreamFactory = &BasicSSSFactory;
        // Define variable type that will be used by default
        CreationAttribs.Desc.DefaultVariableType = SHADER_VARIABLE_TYPE_STATIC;

        // Create vertex shader
        RefCntAutoPtr<IShader> pRTVS;
        {
            CreationAttribs.Desc.ShaderType = SHADER_TYPE_VERTEX;
            CreationAttribs.EntryPoint = "main";
            CreationAttribs.Desc.Name = "Render Target VS";
            CreationAttribs.FilePath = "rendertarget.vsh";
            pDevice->CreateShader(CreationAttribs, &pRTVS);
        }

        // Create pixel shader
        RefCntAutoPtr<IShader> pRTPS;
        {
            CreationAttribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
            CreationAttribs.EntryPoint = "main";
            CreationAttribs.Desc.Name = "Render Target PS";
            CreationAttribs.FilePath = "rendertarget.psh";

            // Shader variables should typically be mutable, which means they are expected
            // to change on a per-instance basis
            ShaderVariableDesc Vars[] =
            {
                { "g_Texture", SHADER_VARIABLE_TYPE_MUTABLE }
            };
            CreationAttribs.Desc.VariableDesc = Vars;
            CreationAttribs.Desc.NumVariables = _countof(Vars);

            // Define static sampler for g_Texture. Static samplers should be used whenever possible
            StaticSamplerDesc StaticSamplers[] =
            {
                { "g_Texture", { FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP } }
            };
            CreationAttribs.Desc.StaticSamplers = StaticSamplers;
            CreationAttribs.Desc.NumStaticSamplers = _countof(StaticSamplers);

            pDevice->CreateShader(CreationAttribs, &pRTPS);
        }

        RTPSODesc.GraphicsPipeline.pVS = pRTVS;
        RTPSODesc.GraphicsPipeline.pPS = pRTPS;
        pDevice->CreatePipelineState(RTPSODesc, &m_pRTPSO);

        // Since we are using mutable variable, we must create shader resource binding object
        // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
        m_pRTPSO->CreateShaderResourceBinding(&m_pRTSRB, true);

        // Set render target color texture SRV in the SRB
        m_pRTSRB->GetVariable(SHADER_TYPE_PIXEL, "g_Texture")->Set(pRTColor->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    }
}

// Render a frame
void Tutorial12_RenderTarget::Render()
{
    // Clear the render target's buffers
    const float ClearColor[] = { 0.350f,  0.350f,  0.350f, 1.0f };
    m_pImmediateContext->SetRenderTargets(1, &m_pRTColorAttachment, m_pRTDepthAttachment, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearRenderTarget(m_pRTColorAttachment, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(m_pRTDepthAttachment, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        // Map the cube's constant buffer and fill it in with its model-view-projection matrix
        MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        *CBConstants = transposeMatrix(m_WorldViewProjMatrix);
    }

    // Set the cube's pipeline state
    m_pImmediateContext->SetPipelineState(m_pPSO);

    // Commit the cube shader's resources
    m_pImmediateContext->CommitShaderResources(m_pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Draw the cube's vertices
    DrawAttribs DrawAttrs;
    DrawAttrs.NumVertices = 36;
    DrawAttrs.Flags = DRAW_FLAG_VERIFY_STATES; // Verify the state of vertex and index buffers
    m_pImmediateContext->Draw(DrawAttrs);

    // Clear the default render target's buffers
    const float Zero[] = { 0.0f,  0.0f,  0.0f, 1.0f };
    m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearRenderTarget(nullptr, Zero, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set render target pipeline state
    m_pImmediateContext->SetPipelineState(m_pRTPSO);

    // Commit the render target shader's resources
    m_pImmediateContext->CommitShaderResources(m_pRTSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Draw the render target's vertices
    DrawAttribs RTDrawAttrs;
    RTDrawAttrs.NumVertices = 4;
    RTDrawAttrs.Flags = DRAW_FLAG_VERIFY_STATES; // Verify the state of vertex and index buffers
    m_pImmediateContext->Draw(RTDrawAttrs);
}

void Tutorial12_RenderTarget::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    // Set cube world view matrix
    float4x4 CubeWorldView = rotationY(-static_cast<float>(CurrTime)) * rotationX(PI_F * 0.1f) *  translationMatrix(0.0f, 0.0f, 5.0f);
    float NearPlane = 0.1f;
    float FarPlane = 100.f;
    float aspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);

    // Projection matrix differs between DX and OpenGL
    auto Proj = Projection(PI_F / 4.0f, aspectRatio, NearPlane, FarPlane, m_pDevice->GetDeviceCaps().IsGLDevice());

    // Compute world-view-projection matrix
    m_WorldViewProjMatrix = CubeWorldView * Proj;
}

}
