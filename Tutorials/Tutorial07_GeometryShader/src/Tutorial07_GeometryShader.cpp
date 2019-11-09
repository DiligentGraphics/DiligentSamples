/*     Copyright 2019 Diligent Graphics LLC
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
#include "../../Common/src/TexturedCube.h"
#include "imgui.h"

#ifdef HLSL2GLSL_CONVERTER_SUPPORTED
#   include "HLSL2GLSLConverterImpl.h"
#endif

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
                                                               EngineCreateInfo&  Attribs,
                                                               SwapChainDesc&     SCDesc)
{
    SampleBase::GetEngineInitializationAttribs(DevType, Attribs, SCDesc);
#if VULKAN_SUPPORTED
    if(DevType == DeviceType::Vulkan)
    {
        auto& VkAttrs = static_cast<EngineVkCreateInfo&>(Attribs);
        VkAttrs.EnabledFeatures.geometryShader = true;
    }
#endif
}

static RefCntAutoPtr<IShader> CreateShader(IRenderDevice*          pDevice,
                                           const ShaderCreateInfo& ShaderCI)
{
    RefCntAutoPtr<IShader> pShader;

#ifdef HLSL2GLSL_CONVERTER_SUPPORTED
    if (pDevice->GetDeviceCaps().IsVulkanDevice())
    {
        // glslang currently does not produce geometry shader bytecode that can be
        // properly linked with other shader stages. So we will manually convert HLSL to GLSL
        // and compile GLSL

        const auto& Converter = HLSL2GLSLConverterImpl::GetInstance();
        HLSL2GLSLConverterImpl::ConversionAttribs Attribs;
        Attribs.pSourceStreamFactory       = ShaderCI.pShaderSourceStreamFactory;
        Attribs.ppConversionStream         = nullptr;
        Attribs.EntryPoint                 = ShaderCI.EntryPoint;
        Attribs.ShaderType                 = ShaderCI.Desc.ShaderType;
        Attribs.IncludeDefinitions         = true;
        Attribs.InputFileName              = ShaderCI.FilePath;
        Attribs.SamplerSuffix              = ShaderCI.CombinedSamplerSuffix;
        // Separate shader objects extension is required to allow input/output layout qualifiers
        Attribs.UseInOutLocationQualifiers = true;
        auto ConvertedSource = Converter.Convert(Attribs);
        
        ShaderCreateInfo ConvertedShaderCI = ShaderCI;
        ConvertedShaderCI.pShaderSourceStreamFactory = nullptr;
        ConvertedShaderCI.Source                     = ConvertedSource.c_str();
        ConvertedShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_GLSL;

        pDevice->CreateShader(ConvertedShaderCI, &pShader);
    }
#endif

    if (!pShader)
    {
        pDevice->CreateShader(ShaderCI, &pShader);
    }
    return pShader;
}

void Tutorial07_GeometryShader::CreatePipelineState()
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

    // Create dynamic uniform buffer that will store shader constants
    CreateUniformBuffer(m_pDevice, sizeof(Constants), "Shader constants CB", &m_ShaderConstants);

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

    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube VS";
        ShaderCI.FilePath        = "cube.vsh";
        pVS = CreateShader(m_pDevice, ShaderCI);
    }

    // Create a geometry shader
    RefCntAutoPtr<IShader> pGS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_GEOMETRY;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube GS";
        ShaderCI.FilePath        = "cube.gsh";
        pGS = CreateShader(m_pDevice, ShaderCI);
    }

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube PS";
        ShaderCI.FilePath        = "cube.psh";
        pPS = CreateShader(m_pDevice, ShaderCI);
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
    SamplerDesc SamLinearClampDesc
    {
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, 
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
    };
    StaticSamplerDesc StaticSamplers[] = 
    {
        {SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}
    };
    PSODesc.ResourceLayout.StaticSamplers    = StaticSamplers;
    PSODesc.ResourceLayout.NumStaticSamplers = _countof(StaticSamplers);

    m_pDevice->CreatePipelineState(PSODesc, &m_pPSO);

    // Since we did not explcitly specify the type for 'VSConstants', 'GSConstants', 
    // and 'PSConstants' variables, default type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used.
    // Static variables never change and are bound directly to the pipeline state object.
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX,   "VSConstants")->Set(m_ShaderConstants);
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_GEOMETRY, "GSConstants")->Set(m_ShaderConstants);
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL,    "PSConstants")->Set(m_ShaderConstants);

    // Since we are using mutable variable, we must create a shader resource binding object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    m_pPSO->CreateShaderResourceBinding(&m_SRB, true);
}

void Tutorial07_GeometryShader::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::SliderFloat("Line Width", &m_LineWidth, 1.f, 10.f);
    }
    ImGui::End();
}

void Tutorial07_GeometryShader::Initialize(IEngineFactory*   pEngineFactory,
                                           IRenderDevice*    pDevice,
                                           IDeviceContext**  ppContexts,
                                           Uint32            NumDeferredCtx,
                                           ISwapChain*       pSwapChain)
{
    const auto& deviceCaps = pDevice->GetDeviceCaps();
    if (!deviceCaps.bGeometryShadersSupported)
    {
        throw std::runtime_error("Geometry shaders are not supported");
    }

    SampleBase::Initialize(pEngineFactory, pDevice, ppContexts, NumDeferredCtx, pSwapChain);

    CreatePipelineState();

    // Load textured cube
    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(pDevice);
    m_CubeIndexBuffer  = TexturedCube::CreateIndexBuffer(pDevice);
    m_TextureSRV = TexturedCube::LoadTexture(pDevice, "DGLogo.png")->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);
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

    // Bind vertex and index buffers
    Uint32 offset = 0;
    IBuffer* pBuffs[] = {m_CubeVertexBuffer};
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, &offset, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set the pipeline state
    m_pImmediateContext->SetPipelineState(m_pPSO);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode 
    // makes sure that resources are transitioned to required states.
    m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs DrawAttrs;
    DrawAttrs.IndexType  = VT_UINT32; // Index type
    DrawAttrs.NumIndices = 36;
    // Verify the state of vertex and index buffers
    DrawAttrs.Flags      = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);
}

void Tutorial07_GeometryShader::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    const bool IsGL = m_pDevice->GetDeviceCaps().IsGLDevice();

    // Set cube world view matrix
    float4x4 CubeWorldView = float4x4::RotationY( static_cast<float>(CurrTime) * 1.0f) * float4x4::RotationX(-PI_F*0.1f) * 
        float4x4::Translation(0.f, 0.0f, 5.0f);
    float NearPlane = 0.1f;
    float FarPlane = 100.f;
    float aspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);
    // Projection matrix differs between DX and OpenGL
    auto Proj = float4x4::Projection(PI_F / 4.f, aspectRatio, NearPlane, FarPlane, IsGL);
    // Compute world-view-projection matrix
    m_WorldViewProjMatrix = CubeWorldView * Proj;
}

}
