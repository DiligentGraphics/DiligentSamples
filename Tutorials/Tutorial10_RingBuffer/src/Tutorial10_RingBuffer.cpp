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

#include <algorithm>
#include <cmath>
#include "Tutorial10_RingBuffer.h"
#include "MapHelper.h"
#include "BasicShaderSourceStreamFactory.h"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "CommonlyUsedStates.h"

using namespace Diligent;

SampleBase* CreateSample()
{
    return new Tutorial10_RingBuffer();
}


void Tutorial10_RingBuffer::Initialize(IRenderDevice *pDevice, IDeviceContext **ppContexts, Uint32 NumDeferredCtx, ISwapChain *pSwapChain)
{
    SampleBase::Initialize(pDevice, ppContexts, NumDeferredCtx, pSwapChain);

    {
        // Pipeline state object encompasses configuration of all GPU stages

        PipelineStateDesc PSODesc;
        // Pipeline state name is used by the engine to report issues
        // It is always a good idea to give objects descriptive names
        PSODesc.Name = "Ripple PSO"; 

        // This is a graphics pipeline
        PSODesc.IsComputePipeline = false; 

        // This tutorial will render to a single render target
        PSODesc.GraphicsPipeline.NumRenderTargets = 1;
        // Set render target format which is the format of the swap chain's color buffer
        PSODesc.GraphicsPipeline.RTVFormats[0] = pSwapChain->GetDesc().ColorBufferFormat;
        // Set depth buffer format which is the format of the swap chain's back buffer
        PSODesc.GraphicsPipeline.DSVFormat = pSwapChain->GetDesc().DepthBufferFormat;
        // Primitive topology defines what kind of primitives will be rendered by this pipeline state
        PSODesc.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP;
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
            CreationAttribs.Desc.Name = "Ripples VS";
            CreationAttribs.FilePath = "ripples.vsh";
            pDevice->CreateShader(CreationAttribs, &pVS);
            // Create dynamic uniform buffer that will store our transformation matrix
            // Dynamic buffers can be frequently updated by the CPU
            BufferDesc CBDesc;
            CBDesc.Name = "VS constants CB";
            CBDesc.uiSizeInBytes = sizeof(float4x4);
            CBDesc.Usage = USAGE_DYNAMIC;
            CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
            CBDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
            pDevice->CreateBuffer( CBDesc, BufferData(), &m_VSConstants );

            // Since we did not explcitly specify the type for Constants, default type
            // (SHADER_VARIABLE_TYPE_STATIC) will be used. Static variables never change and are bound directly
            // through the shader (http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/)
            pVS->GetShaderVariable("Constants")->Set(m_VSConstants);
        }

        RefCntAutoPtr<ITextureView> pTextureSRV;
        {
            // Load texture
            TextureLoadInfo loadInfo;
            loadInfo.IsSRGB = true;
            loadInfo.MipLevels = 1;
            RefCntAutoPtr<ITexture> Tex;
            CreateTextureFromFile("colors.png", loadInfo, m_pDevice, &Tex);
            // Get shader resource view from the texture
            pTextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        }

        // Create pixel shader
        RefCntAutoPtr<IShader> pPS;
        {
            CreationAttribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
            CreationAttribs.EntryPoint = "main";
            CreationAttribs.Desc.Name = "Ripples PS";
            CreationAttribs.FilePath = "ripples.psh";
            StaticSamplerDesc StaticSamplers[] = 
            {
                StaticSamplerDesc{"g_Tex", Sam_LinearClamp}
            };
            CreationAttribs.Desc.StaticSamplers = StaticSamplers;
            CreationAttribs.Desc.NumStaticSamplers = _countof(StaticSamplers);
            pDevice->CreateShader(CreationAttribs, &pPS);
            pPS->GetShaderVariable("g_Tex")->Set(pTextureSRV);
        }

        // Define vertex shader input layout
        LayoutElement LayoutElems[] =
        {
            // Attribute 0 - vertex position
            LayoutElement(0, 0, 3, VT_FLOAT32, False),
            // Attribute 1 - tex coord
            LayoutElement(1, 0, 1, VT_FLOAT32, False)
        };
        PSODesc.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
        PSODesc.GraphicsPipeline.InputLayout.NumElements = _countof(LayoutElems);

        PSODesc.GraphicsPipeline.pVS = pVS;
        PSODesc.GraphicsPipeline.pPS = pPS;

        pDevice->CreatePipelineState(PSODesc, &m_pPSO);
    }

    {
        // Create vertex buffer that stores ripple vertices
        BufferDesc VertBuffDesc;
        VertBuffDesc.Name = "Ripple vertex buffer";
        VertBuffDesc.Usage = USAGE_DYNAMIC;
        VertBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        VertBuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        VertBuffDesc.uiSizeInBytes = sizeof(Vertex) * RippleGridSize * RippleGridSize;
        pDevice->CreateBuffer(VertBuffDesc, BufferData{}, &m_VertexBuffer);
    }

    {
        m_NumIndices = 
            (RippleGridSize-1) * RippleGridSize * 2 // RippleGridSize-1 rows, each having RippleGridSize * 2 vertices
            + (RippleGridSize-2) * 2; // plus 2 duplicate vertex for every transiton to next strip
        std::vector<Uint32> Indices;
        Indices.reserve(m_NumIndices);
        for(Uint32 row = 0; row < RippleGridSize-1; ++row)
        {
            if(row > 0)
            {
                Indices.push_back( (row+1)*RippleGridSize + 0);
            }

            for(Uint32 col = 0; col < RippleGridSize; ++col)
            {
                Indices.push_back((row+1)*RippleGridSize + col);
                Indices.push_back(row*RippleGridSize + col);
                
            }

            if(row < RippleGridSize-2)
            {
                Indices.push_back(Indices.back());
            }
        }
        VERIFY_EXPR(Indices.size() == m_NumIndices);

        // Create index buffer
        BufferDesc IndBuffDesc;
        IndBuffDesc.Name = "Ripple index buffer";
        IndBuffDesc.Usage = USAGE_STATIC;
        IndBuffDesc.BindFlags = BIND_INDEX_BUFFER;
        IndBuffDesc.uiSizeInBytes = static_cast<Uint32>(Indices.size() * sizeof(Indices[0]));
        BufferData IBData;
        IBData.pData = Indices.data();
        IBData.DataSize = IndBuffDesc.uiSizeInBytes;
        pDevice->CreateBuffer(IndBuffDesc, IBData, &m_IndexBuffer);
    }
}

// Render a frame
void Tutorial10_RingBuffer::Render()
{
    // Clear the back buffer 
    const float ClearColor[] = {  0.350f,  0.350f,  0.350f, 1.0f }; 
    m_pImmediateContext->ClearRenderTarget(nullptr, ClearColor);
    m_pImmediateContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG, 1.f);

    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        *CBConstants = transposeMatrix(m_WorldViewProjMatrix);
    }

    // Bind vertex buffer
    Uint32 offset = 0;
    IBuffer *pBuffs[] = {m_VertexBuffer};
    m_pImmediateContext->SetVertexBuffers(0, 1, pBuffs, &offset, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_IndexBuffer, 0);

    // Set pipeline state
    m_pImmediateContext->SetPipelineState(m_pPSO);
    // Commit shader resources
    // COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES flag needs to be specified to make sure
    // that resources are transitioned to proper states
    m_pImmediateContext->CommitShaderResources(nullptr, COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES);
    
    DrawAttribs DrawAttrs;
    DrawAttrs.IsIndexed = true; // This is an indexed draw call
    DrawAttrs.IndexType = VT_UINT32; // Index type
    DrawAttrs.NumIndices = m_NumIndices;
    m_pImmediateContext->Draw(DrawAttrs);
}

// Source: https://www.shadertoy.com/view/ldjcDK
float bipolarPattern(float2 p, float time)
{
    //rotate
    float rotationrate = 0.1f;
    float s = sin(rotationrate * time);
    float c = cos(rotationrate * time);
    p = float2(p.x*c + p.y*s, -p.x*s + p.y*c);
       
    //bipolar coordinates (sigma, tau)
    float a = std::min(10.0f, 2.0f*time);
    float alpha = a*a - dot(p, p);
    float beta = a*a + dot(p, p);
    float sigma = atan2(2.0f*a*p.y, alpha);
    float tau = 0.5f * log((beta + 2.0f*a*p.x) / (beta - 2.0f*a*p.x));

    //do something funky in bipolar corrdinates
    float freq = 10.0f;
    float rate = 3.0f;
    float2 osc;
    osc.x = 0.5f * (1.0f + cos(freq*sigma + rate*time));
    osc.y = 0.5f * (1.0f + cos(freq*tau + rate*time));
    
    //the factor of cosh suppreses the oscilations near the poles where they would otherwise go a bit crazy.
    float cosh = exp(-tau) + exp(tau);
    float bipolarOscillations  = (osc.x + osc.y)/cosh;
    
    return bipolarOscillations;
}

void Tutorial10_RingBuffer::UpdateRippleBuffer(float CurrTime)
{
    // Map the buffer and write current world-view-projection matrix
    MapHelper<Vertex> Data(m_pImmediateContext, m_VertexBuffer, MAP_WRITE, MAP_FLAG_DISCARD);
    for(int row=0; row < RippleGridSize; ++row)
    {
        for(int col=0; col < RippleGridSize; ++col)
        {
            auto& vert = Data[row * RippleGridSize + col];
            float x = static_cast<float>(col) / static_cast<float>(RippleGridSize-1);
            float y = static_cast<float>(row) / static_cast<float>(RippleGridSize-1);
            x = (x-0.5f) * 2.f;
            y = (y-0.5f) * 2.f;
            float r = std::max(abs(x), abs(y));
            float r0 = sqrt(x*x + y*y);
            x = x / r0 * r;
            y = y / r0 * r;
            float z = bipolarPattern(float2(x, y)*25.0f, CurrTime);
            vert.texcoord = z;
            vert.pos.x = x * 2.f;
            vert.pos.y = y * 2.f;
            vert.pos.z = z * 0.2f;
        }
    }
}

void Tutorial10_RingBuffer::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    const bool IsGL = m_pDevice->GetDeviceCaps().IsGLDevice();
    
    UpdateRippleBuffer(static_cast<float>(CurrTime));

    // Set world view matrix
    float4x4 WorldView = rotationX(PI_F / 1.5f) * translationMatrix(0.f, 0.0f, 5.0f);
    float NearPlane = 0.1f;
    float FarPlane = 100.f;
    float aspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);
    // Projection matrix differs between DX and OpenGL
    auto Proj = Projection(PI_F / 4.f, aspectRatio, NearPlane, FarPlane, IsGL);
    // Compute world-view-projection matrix
    m_WorldViewProjMatrix = WorldView * Proj;
}
