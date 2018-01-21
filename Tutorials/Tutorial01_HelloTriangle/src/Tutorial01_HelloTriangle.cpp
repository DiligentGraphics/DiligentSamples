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

#include "Tutorial01_HelloTriangle.h"

using namespace Diligent;

SampleBase* CreateSample()
{
    return new Tutorial01_HelloTriangle();
}

// For this tutorial, we will use simple vertex shader
// that creates a procedural triangle

// Diligent Engine can use HLSL source for all supported platforms
// It will convert HLSL to GLSL for OpenGL/Vulkan

static const char* VSSource = R"(
struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float3 Color : COLOR; 
};

PSInput main(uint VertId : SV_VertexID) 
{
    float4 Pos[3];
    Pos[0] = float4(-0.5, -0.5, 0.0, 1.0);
    Pos[1] = float4( 0.0, +0.5, 0.0, 1.0);
    Pos[2] = float4(+0.5, -0.5, 0.0, 1.0);

    float3 Col[3];
    Col[0] = float3(1.0, 0.0, 0.0); // red
    Col[1] = float3(0.0, 1.0, 0.0); // green
    Col[2] = float3(0.0, 0.0, 1.0); // blue

    PSInput ps; 
    ps.Pos = Pos[VertId];
    ps.Color = Col[VertId];
    return ps;
}
)";

// Pixel shader will simply output interpolated vertex color
static const char* PSSource = R"(
struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float3 Color : COLOR; 
};

float4 main(PSInput In) : SV_Target
{
    return float4(In.Color.rgb, 1.0);
}
)";


void Tutorial01_HelloTriangle::Initialize(IRenderDevice *pDevice, IDeviceContext **ppContexts, Uint32 NumDeferredCtx, ISwapChain *pSwapChain)
{
    SampleBase::Initialize(pDevice, ppContexts, NumDeferredCtx, pSwapChain);

    // Pipeline state object encompasses configuration of all GPU stages

    PipelineStateDesc PSODesc;
    // Pipeline state name is used by the engine to report issues
    // It is always a good idea to give objects descriptive names
    PSODesc.Name = "Simple triangle PSO"; 

    // This is a graphics pipeline
    PSODesc.IsComputePipeline = false; 

    // This tutorial will render to a single render target
    PSODesc.GraphicsPipeline.NumRenderTargets = 1;
    // Set render target format which is the format of the swap chain's color buffer
    PSODesc.GraphicsPipeline.RTVFormats[0] = pSwapChain->GetDesc().ColorBufferFormat;
    // This tutorial will not use depth buffer
    PSODesc.GraphicsPipeline.DSVFormat = TEX_FORMAT_UNKNOWN;
    // Primitive topology type defines what kind of primitives will be rendered by this pipeline state
    PSODesc.GraphicsPipeline.PrimitiveTopologyType = PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    // No back face culling for this tutorial
    PSODesc.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
    // Disable depth testing
    PSODesc.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

    ShaderCreationAttribs CreationAttribs;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL behind the scene
    CreationAttribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    // Create vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        CreationAttribs.Desc.ShaderType = SHADER_TYPE_VERTEX;
        CreationAttribs.EntryPoint = "main";
        CreationAttribs.Desc.Name = "Triangle vertex shader";
        CreationAttribs.Source = VSSource;
        pDevice->CreateShader(CreationAttribs, &pVS);
    }

    // Create pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        CreationAttribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
        CreationAttribs.EntryPoint = "main";
        CreationAttribs.Desc.Name = "Triangle pixel shader";
        CreationAttribs.Source = PSSource;
        pDevice->CreateShader(CreationAttribs, &pPS);
    }

    // Finally, create the pipeline state
    PSODesc.GraphicsPipeline.pVS = pVS;
    PSODesc.GraphicsPipeline.pPS = pPS;
    pDevice->CreatePipelineState(PSODesc, &m_pPSO);
}

// Render a frame
void Tutorial01_HelloTriangle::Render()
{
    // Clear the back buffer 
    const float ClearColor[] = {  0.350f,  0.350f,  0.350f, 1.0f }; 
    m_pImmediateContext->ClearRenderTarget(nullptr, ClearColor);
    m_pImmediateContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG, 1.f);

    // Set pipeline state in the immediate context
    m_pImmediateContext->SetPipelineState(m_pPSO);
    // We need to commit shader resource. Even though in this example
    // we don't really have any resources, this call also sets the shaders
    m_pImmediateContext->CommitShaderResources(nullptr, COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES);
    DrawAttribs drawAttrs;
    drawAttrs.NumVertices = 3; // We will render 3 vertices
    drawAttrs.Topology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // Primitive topology must be specified
    m_pImmediateContext->Draw(drawAttrs);
}

void Tutorial01_HelloTriangle::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
}
