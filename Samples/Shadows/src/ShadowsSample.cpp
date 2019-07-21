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

#include "ShadowsSample.h"
#include "MapHelper.h"
#include "FileSystem.h"
#include "ShaderMacroHelper.h"
#include "CommonlyUsedStates.h"
#include "StringTools.h"
#include "GraphicsUtilities.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new ShadowsSample();
}


void ShadowsSample::Initialize(IEngineFactory* pEngineFactory, IRenderDevice *pDevice, IDeviceContext **ppContexts, Uint32 NumDeferredCtx, ISwapChain *pSwapChain)
{
    SampleBase::Initialize(pEngineFactory, pDevice, ppContexts, NumDeferredCtx, pSwapChain);
    std::string MeshFileName = "Powerplant/Powerplant.sdkmesh";
    m_Mesh.Create(MeshFileName.c_str());
    std::string Directory;
    FileSystem::SplitFilePath(MeshFileName, &Directory, nullptr);
    m_Mesh.LoadGPUResources(Directory.c_str(), pDevice, m_pImmediateContext);

    CreateUniformBuffer(pDevice, sizeof(CameraAttribs), "Camera attribs buffer", &m_CameraAttribsCB);
    CreateUniformBuffer(pDevice, sizeof(LightAttribs), "Light attribs buffer", &m_LightAttribsCB);
    CreatePipelineStates(pDevice);

    m_LightAttribs.f4Direction = float3(-0.803294539f, -0.128133044f, -0.581647396f);

    m_Camera.SetPos(float3(70, 10, 0.f));
    m_Camera.SetRotation(-PI_F/2.f, 0);
    m_Camera.SetRotationSpeed(0.005f);
    m_Camera.SetMoveSpeed(5.f);
    m_Camera.SetSpeedUpScales(5.f, 10.f);
}



void ShadowsSample::DXSDKMESH_VERTEX_ELEMENTtoInputLayoutDesc(const DXSDKMESH_VERTEX_ELEMENT*  VertexElement,
                                                              Uint32                           Stride,
                                                              InputLayoutDesc&                 Layout,
                                                              std::vector<LayoutElement>&      Elements)
{
    Elements.clear();
    for (Uint32 input_elem = 0; VertexElement[input_elem].Stream != 0xFF; ++input_elem)
    {
        const auto& SrcElem = VertexElement[input_elem];
        Int32 InputIndex = -1;
        switch(SrcElem.Usage)
        {
            case DXSDKMESH_VERTEX_SEMANTIC_POSITION:
                InputIndex = 0;
            break;

            case DXSDKMESH_VERTEX_SEMANTIC_NORMAL:
                InputIndex = 1;
            break;

            case DXSDKMESH_VERTEX_SEMANTIC_TEXCOORD:
                InputIndex = 2;
            break;
        }

        if (InputIndex >= 0)
        {
            Uint32     NumComponents = 0;
            VALUE_TYPE ValueType     = VT_UNDEFINED;
            Bool       IsNormalized  = False;
            switch(SrcElem.Type)
            {
                case DXSDKMESH_VERTEX_DATA_TYPE_FLOAT2:
                    NumComponents = 2;
                    ValueType     = VT_FLOAT32;
                    IsNormalized  = False;
                break;

                case DXSDKMESH_VERTEX_DATA_TYPE_FLOAT3:
                    NumComponents = 3;
                    ValueType     = VT_FLOAT32;
                    IsNormalized  = False;
                break;

                default:
                    UNEXPECTED("Unsupported data type. Please add appropriate case statement.");
            }
            Elements.emplace_back(InputIndex, SrcElem.Stream, NumComponents, ValueType, IsNormalized, SrcElem.Offset, Stride);
        }
    }
    Layout.LayoutElements = Elements.data();
    Layout.NumElements    = static_cast<Uint32>(Elements.size());
}

void ShadowsSample::CreatePipelineStates(IRenderDevice* pDevice)
{
    ShaderCreateInfo ShaderCI;
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory("shaders", &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.UseCombinedTextureSamplers = true;

    ShaderMacroHelper Macros;
    //Macros.AddShaderMacro("TONE_MAPPING_MODE", "TONE_MAPPING_MODE_UNCHARTED2");
    ShaderCI.Macros = Macros;

    ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
    ShaderCI.Desc.Name       = "Mesh VS";
    ShaderCI.EntryPoint      = "MeshVS";
    ShaderCI.FilePath        = "MeshVS.vsh";
    RefCntAutoPtr<IShader> pVS;
    m_pDevice->CreateShader(ShaderCI, &pVS);

    ShaderCI.Desc.Name   = "Mesh PS";
    ShaderCI.EntryPoint  = "MeshPS";
    ShaderCI.FilePath    = "MeshPS.psh";
    ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
    RefCntAutoPtr<IShader> pPS;
    m_pDevice->CreateShader(ShaderCI, &pPS);

    m_PSOIndex.resize(m_Mesh.GetNumVBs());
    m_RenderMeshPSO.clear();
    m_RenderMeshShadowPSO.clear();
    for(Uint32 vb = 0; vb < m_Mesh.GetNumVBs(); ++vb)
    {
        PipelineStateDesc PSODesc;
        std::vector<LayoutElement> Elements;
        auto& InputLayout = PSODesc.GraphicsPipeline.InputLayout;
        DXSDKMESH_VERTEX_ELEMENTtoInputLayoutDesc(m_Mesh.VBElements(vb), m_Mesh.GetVertexStride(vb), InputLayout, Elements);
        
        //  Try to find PSO with the same layout
        Uint32 pso;
        for (pso=0; pso < m_RenderMeshPSO.size(); ++pso)
        {
            const auto& PSOLayout = m_RenderMeshPSO[pso]->GetDesc().GraphicsPipeline.InputLayout;
            bool IsSameLayout =
                PSOLayout.NumElements == InputLayout.NumElements && 
                memcmp(PSOLayout.LayoutElements, InputLayout.LayoutElements, sizeof(LayoutElement) * InputLayout.NumElements) == 0;
            
            if (IsSameLayout)
                break;
        }
        
        m_PSOIndex[vb] = pso;
        if (pso < static_cast<Uint32>(m_RenderMeshPSO.size()))
            continue;

        StaticSamplerDesc StaticSamplers[] =
        {
            {SHADER_TYPE_PIXEL, "g_tex2DDiffuse", Sam_Aniso4xWrap}
        };
        PSODesc.ResourceLayout.StaticSamplers    = StaticSamplers;
        PSODesc.ResourceLayout.NumStaticSamplers = _countof(StaticSamplers);

        ShaderResourceVariableDesc Vars[] = 
        {
            {SHADER_TYPE_PIXEL, "g_tex2DDiffuse",   SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
        };
        PSODesc.ResourceLayout.Variables    = Vars;
        PSODesc.ResourceLayout.NumVariables = _countof(Vars);

        PSODesc.Name = "Mesh PSO";
        PSODesc.GraphicsPipeline.pVS = pVS;
        PSODesc.GraphicsPipeline.pPS = pPS;

        PSODesc.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
        PSODesc.GraphicsPipeline.NumRenderTargets = 1;
        PSODesc.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
        PSODesc.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        PSODesc.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS_EQUAL;

        RefCntAutoPtr<IPipelineState> pRenderMeshPSO;
        m_pDevice->CreatePipelineState(PSODesc, &pRenderMeshPSO);
        pRenderMeshPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbCameraAttribs")->Set(m_CameraAttribsCB);
        pRenderMeshPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL,  "cbLightAttribs")->Set(m_LightAttribsCB);

        PSODesc.Name = "Mesh Shadow PSO";
        PSODesc.GraphicsPipeline.pPS        = nullptr;
        PSODesc.ResourceLayout.StaticSamplers    = nullptr;
        PSODesc.ResourceLayout.NumStaticSamplers = 0;
        PSODesc.ResourceLayout.Variables         = nullptr;
        PSODesc.ResourceLayout.NumVariables      = 0;
        RefCntAutoPtr<IPipelineState> pRenderMeshShadowPSO;
        m_pDevice->CreatePipelineState(PSODesc, &pRenderMeshShadowPSO);
        pRenderMeshShadowPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbCameraAttribs")->Set(m_CameraAttribsCB);

        m_RenderMeshPSO.emplace_back(std::move(pRenderMeshPSO));
        m_RenderMeshShadowPSO.emplace_back(std::move(pRenderMeshShadowPSO));
    }
    
    m_SRBs.clear();
    m_SRBs.resize(m_Mesh.GetNumMaterials());
    for(Uint32 mat = 0; mat < m_Mesh.GetNumMaterials(); ++mat)
    {
        const auto& Mat = m_Mesh.GetMaterial(mat);
        RefCntAutoPtr<IShaderResourceBinding> pSRB;
        m_RenderMeshPSO[0]->CreateShaderResourceBinding(&pSRB, true);
        VERIFY(Mat.pDiffuseRV != nullptr, "Material must have diffuse color texture");
        pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_tex2DDiffuse")->Set(Mat.pDiffuseRV);
        m_SRBs[mat] = std::move(pSRB);
    }
}

ShadowsSample::~ShadowsSample()
{
}


// Render a frame
void ShadowsSample::Render()
{
    // Clear the back buffer 
    const float ClearColor[] = { 0.032f,  0.032f,  0.032f, 1.0f }; 
    m_pImmediateContext->ClearRenderTarget(nullptr, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        MapHelper<LightAttribs> LightData(m_pImmediateContext, m_LightAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
        *LightData = m_LightAttribs;
    }

    {
        const auto& CameraView  = m_Camera.GetViewMatrix();
        const auto& CameraWorld = m_Camera.GetWorldMatrix();
        float3 CameraWorldPos = float3::MakeVector(CameraWorld[3]);
        const auto& Proj = m_Camera.GetProjMatrix();
        auto CameraViewProj = CameraView * Proj;

        MapHelper<CameraAttribs> CamAttribs(m_pImmediateContext, m_CameraAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
        CamAttribs->mProjT        = Proj.Transpose();
        CamAttribs->mViewProjT    = CameraViewProj.Transpose();
        CamAttribs->mViewProjInvT = CameraViewProj.Inverse().Transpose();
        CamAttribs->f4Position = float4(CameraWorldPos, 1);
    }

    DrawMesh(m_pImmediateContext, false);
}


void ShadowsSample::DrawMesh(IDeviceContext* pCtx, bool bIsShadowPass)
{
    //uint32 partCount = 0;
    for (Uint32 meshIdx = 0; meshIdx < m_Mesh.GetNumMeshes(); ++meshIdx)
    {
        const auto& SubMesh = m_Mesh.GetMesh(meshIdx);

        IBuffer* pVBs[]  = {m_Mesh.GetMeshVertexBuffer(meshIdx, 0)};
        Uint32 Offsets[] = {0};
        pCtx->SetVertexBuffers(0, 1, pVBs, Offsets, RESOURCE_STATE_TRANSITION_MODE_VERIFY, SET_VERTEX_BUFFERS_FLAG_RESET);

        auto* pIB = m_Mesh.GetMeshIndexBuffer(meshIdx);
        auto IBFormat = m_Mesh.GetIBFormat(meshIdx);
        
        pCtx->SetIndexBuffer(pIB, 0, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

        auto PSOIndex = m_PSOIndex[SubMesh.VertexBuffers[0]];
        auto& pPSO = (bIsShadowPass ? m_RenderMeshShadowPSO : m_RenderMeshPSO)[PSOIndex];
        pCtx->SetPipelineState(pPSO);

        // Draw all subsets
        for(Uint32 subsetIdx = 0; subsetIdx < SubMesh.NumSubsets; ++subsetIdx)
        {
            //if(meshData.FrustumTests[partCount++])
            {
                const auto& Subset = m_Mesh.GetSubset(meshIdx, subsetIdx);
                pCtx->CommitShaderResources(m_SRBs[Subset.MaterialID], RESOURCE_STATE_TRANSITION_MODE_VERIFY);

                DrawAttribs drawAttrs(Subset.IndexCount, IBFormat, DRAW_FLAG_VERIFY_ALL);
                drawAttrs.FirstIndexLocation = Subset.IndexStart;
                pCtx->Draw(drawAttrs);
            }
        }
    }
}

void ShadowsSample::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);

    m_Camera.Update(m_InputController, static_cast<float>(ElapsedTime));
    {
        const auto& mouseState = m_InputController.GetMouseState();
        if (m_LastMouseState.PosX >= 0 &&
            m_LastMouseState.PosY >= 0 &&
           (m_LastMouseState.ButtonFlags & MouseState::BUTTON_FLAG_RIGHT) != 0)
        {
            constexpr float LightRotationSpeed = 0.001f;
            float fYawDelta   = (mouseState.PosX - m_LastMouseState.PosX) * LightRotationSpeed;
            float fPitchDelta = (mouseState.PosY - m_LastMouseState.PosY) * LightRotationSpeed;
            float3& f3LightDir = reinterpret_cast<float3&>(m_LightAttribs.f4Direction);
            f3LightDir = float4(f3LightDir, 0) *
                           float4x4::RotationArbitrary(m_Camera.GetWorldUp(),    fYawDelta) * 
                           float4x4::RotationArbitrary(m_Camera.GetWorldRight(), fPitchDelta);
        }

        m_LastMouseState = mouseState;
    }
}

void ShadowsSample::WindowResize(Uint32 Width, Uint32 Height)
{
    float NearPlane = 0.1f;
    float FarPlane = 200.f;
    float AspectRatio = static_cast<float>(Width) / static_cast<float>(Height);
    m_Camera.SetProjAttribs(NearPlane, FarPlane, AspectRatio, PI_F / 4.f, m_pDevice->GetDeviceCaps().IsGLDevice());
}

}
