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

#include "GLTFViewer.h"
#include "MapHelper.h"
#include "BasicMath.h"
#include "GraphicsUtilities.h"
#include "AntTweakBar.h"
#include "TextureUtilities.h"
#include "CommonlyUsedStates.h"
#include "ShaderMacroHelper.h"

namespace Diligent
{

#include "BasicStructures.fxh"
#include "ToneMappingStructures.fxh"

SampleBase* CreateSample()
{
    return new GLTFViewer();
}

namespace
{
    struct EnvMapRenderAttribs
    {
        ToneMappingAttribs TMAttribs;

        float AverageLogLum;
        float Unusued0;
        float Unusued1;
        float Unusued2;
    };
}
void GLTFViewer::Initialize(IEngineFactory* pEngineFactory, IRenderDevice* pDevice, IDeviceContext** ppContexts, Uint32 NumDeferredCtx, ISwapChain* pSwapChain)
{
    SampleBase::Initialize(pEngineFactory, pDevice, ppContexts, NumDeferredCtx, pSwapChain);

    RefCntAutoPtr<ITexture> EnvironmentMap;
    CreateTextureFromFile("textures/papermill.ktx", TextureLoadInfo{}, m_pDevice, &EnvironmentMap);
    m_EnvironmentMapSRV = EnvironmentMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    auto BackBufferFmt  = m_pSwapChain->GetDesc().ColorBufferFormat;
    auto DepthBufferFmt = m_pSwapChain->GetDesc().DepthBufferFormat;
    GLTF_PBR_Renderer::CreateInfo RendererCI;
    RendererCI.RTVFmt = BackBufferFmt;
    RendererCI.DSVFmt = DepthBufferFmt;
    RendererCI.AllowDebugView = true;
    m_GLTFRenderer.reset(new GLTF_PBR_Renderer(m_pDevice, m_pImmediateContext, RendererCI));

    CreateUniformBuffer(m_pDevice, sizeof(CameraAttribs),       "Camera attribs buffer",         &m_CameraAttribsCB);
    CreateUniformBuffer(m_pDevice, sizeof(LightAttribs),        "Light attribs buffer",          &m_LightAttribsCB);
    CreateUniformBuffer(m_pDevice, sizeof(EnvMapRenderAttribs), "Env map render attribs buffer", &m_EnvMapRenderAttribsCB);
    StateTransitionDesc Barriers [] =
    {
        {m_CameraAttribsCB,        RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, true},
        {m_LightAttribsCB,         RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, true},
        {m_EnvMapRenderAttribsCB,  RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, true},
        {EnvironmentMap,           RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, true}
    };
    m_pImmediateContext->TransitionResourceStates(_countof(Barriers), Barriers);


    m_Model.reset(new GLTF::Model(m_pDevice, m_pImmediateContext, "models/DamagedHelmet.gltf"));
    m_GLTFRenderer->InitializeResourceBindings(*m_Model, m_CameraAttribsCB, m_LightAttribsCB);

    auto Rotation = float4x4::RotationX_GL(PI_F);
    for(auto& Node : m_Model->Nodes)
        Node->Matrix *= Rotation;

    CreateEnvMapPSO();

    float3 axis(0, 0.15f, 1.f);
    m_Rotation = Quaternion::RotationFromAxisAngle(axis, PI_F);
    m_LightDirection  = normalize(float3(0.5f, -0.6f, -0.2f));

    // Create a tweak bar
    TwBar *bar = TwNewBar("Settings");
    int barSize[2] = {224 * m_UIScale, 500 * m_UIScale};
    TwSetParam(bar, NULL, "size", TW_PARAM_INT32, 2, barSize);

    TwAddVarRW(bar, "Rotation",           TW_TYPE_QUAT4F,  &m_Rotation,        "opened=true axisz=-z");
    TwAddVarRW(bar, "Light direction",    TW_TYPE_DIR3F,   &m_LightDirection,  "opened=true axisz=-z");
    TwAddVarRW(bar, "Light Color",        TW_TYPE_COLOR4F, &m_LightColor,      "opened=false");
    TwAddVarRW(bar, "Light Intensity",    TW_TYPE_FLOAT,   &m_LightIntensity,  "min=0.0 max=50.0 step=0.1");
    TwAddVarRW(bar, "Occlusion strength", TW_TYPE_FLOAT,   &m_RenderParams.OcclusionStrength, "min=0.0 max=1.0 step=0.01");
    TwAddVarRW(bar, "Emission scale",     TW_TYPE_FLOAT,   &m_RenderParams.EmissionScale,     "min=0.0 max=1.0 step=0.01");
    TwAddVarRW(bar, "Average log lum",    TW_TYPE_FLOAT,   &m_RenderParams.AverageLogLum,     "group='Tone mapping' min=0.01 max=10.0 step=0.01");
    TwAddVarRW(bar, "Middle gray",        TW_TYPE_FLOAT,   &m_RenderParams.MiddleGray,        "group='Tone mapping' min=0.01 max=1.0 step=0.01");
    TwAddVarRW(bar, "White point",        TW_TYPE_FLOAT,   &m_RenderParams.WhitePoint,        "group='Tone mapping' min=0.1  max=20.0 step=0.1");
    TwAddVarRW(bar, "Show environment",   TW_TYPE_BOOLCPP, &m_ShowEnvironment,                "");
    

    TwEnumVal DebugViewType[] = // array used to describe the shadow map resolution
    {
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::None),            "None"       },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::BaseColor),       "Base Color" },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::NormalMap),       "Normal Map" },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Occlusion),       "Occlusion"  },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Emissive),        "Emissive"   },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Metallic),        "Metallic"   },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Roughness),       "Roughness"  },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::DiffuseColor),    "Diffuse color"},
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::SpecularColor),   "Specular color (R0)"},
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Reflectance90),   "Reflectance90"},
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::MeshNormal),      "Mesh normal"},
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::PerturbedNormal), "Perturbed normal"},
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::NdotV),           "n*v"}
    };
    TwType DebugViewTwType = TwDefineEnum( "Debug view", DebugViewType, _countof( DebugViewType ) );
    TwAddVarRW( bar, "Debug view", DebugViewTwType, &m_RenderParams.DebugView, "" );
}

void GLTFViewer::CreateEnvMapPSO()
{
    ShaderCreateInfo ShaderCI;
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory("shaders", &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.UseCombinedTextureSamplers = true;

    ShaderMacroHelper Macros;
    Macros.AddShaderMacro("TONE_MAPPING_MODE", "TONE_MAPPING_MODE_UNCHARTED2");
    ShaderCI.Macros = Macros;

    ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
    ShaderCI.Desc.Name       = "Environment map VS";
    ShaderCI.EntryPoint      = "main";
    ShaderCI.FilePath        = "env_map.vsh";
    RefCntAutoPtr<IShader> pVS;
    m_pDevice->CreateShader(ShaderCI, &pVS);

    ShaderCI.Desc.Name   = "Environment map PS";
    ShaderCI.EntryPoint  = "main";
    ShaderCI.FilePath    = "env_map.psh";
    ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
    RefCntAutoPtr<IShader> pPS;
    m_pDevice->CreateShader(ShaderCI, &pPS);

    PipelineStateDesc PSODesc;
    PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
        
    StaticSamplerDesc StaticSamplers[] =
    {
        {SHADER_TYPE_PIXEL, "EnvMap", Sam_LinearClamp}
    };
    PSODesc.ResourceLayout.StaticSamplers    = StaticSamplers;
    PSODesc.ResourceLayout.NumStaticSamplers = _countof(StaticSamplers);

    ShaderResourceVariableDesc Vars[] = 
    {
        {SHADER_TYPE_PIXEL, "cbCameraAttribs",       SHADER_RESOURCE_VARIABLE_TYPE_STATIC},
        {SHADER_TYPE_PIXEL, "cbEnvMapRenderAttribs", SHADER_RESOURCE_VARIABLE_TYPE_STATIC}
    };
    PSODesc.ResourceLayout.Variables    = Vars;
    PSODesc.ResourceLayout.NumVariables = _countof(Vars);

    PSODesc.Name = "EnvMap PSO";
    PSODesc.GraphicsPipeline.pVS = pVS;
    PSODesc.GraphicsPipeline.pPS = pPS;

    PSODesc.GraphicsPipeline.RTVFormats[0] = m_pSwapChain->GetDesc().ColorBufferFormat;
    PSODesc.GraphicsPipeline.NumRenderTargets = 1;
    PSODesc.GraphicsPipeline.DSVFormat = m_pSwapChain->GetDesc().DepthBufferFormat;
    PSODesc.GraphicsPipeline.PrimitiveTopology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSODesc.GraphicsPipeline.DepthStencilDesc.DepthFunc = COMPARISON_FUNC_LESS_EQUAL;

    m_pDevice->CreatePipelineState(PSODesc, &m_EnvMapPSO);
    m_EnvMapPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "cbCameraAttribs")->Set(m_CameraAttribsCB);
    m_EnvMapPSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "cbEnvMapRenderAttribs")->Set(m_EnvMapRenderAttribsCB);
    m_EnvMapPSO->CreateShaderResourceBinding(&m_EnvMapSRB, true);
    m_EnvMapSRB->GetVariableByName(SHADER_TYPE_PIXEL, "EnvMap")->Set(m_EnvironmentMapSRV);
}

GLTFViewer::~GLTFViewer()
{
}

// Render a frame
void GLTFViewer::Render()
{
    // Clear the back buffer 
    const float ClearColor[] = { 0.350f,  0.350f,  0.350f, 1.0f }; 
    m_pImmediateContext->ClearRenderTarget(nullptr, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    {
        float4x4 CameraView = m_Rotation.ToMatrix() * float4x4::TranslationD3D(0.f, 0.0f, 4.0f);
        float4x4 CameraWorld = CameraView.Inverse();
        float4 CamWorld = float4(0,0,0,1) * CameraWorld;
        float3 CameraWorldPos = float3::MakeVector(CameraWorld[3]);
        float NearPlane = 0.1f;
        float FarPlane = 100.f;
        float aspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);
        // Projection matrix differs between DX and OpenGL
        auto Proj = float4x4::ProjectionD3D(PI_F / 4.f, aspectRatio, NearPlane, FarPlane, m_pDevice->GetDeviceCaps().IsGLDevice());
        // Compute world-view-projection matrix
        auto CameraViewProj = CameraView * Proj;

        {
            MapHelper<CameraAttribs> CamAttribs(m_pImmediateContext, m_CameraAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
            CamAttribs->mProjT        = Proj.Transpose();
            CamAttribs->mViewProjT    = CameraViewProj.Transpose();
            CamAttribs->mViewProjInvT = CameraViewProj.Inverse().Transpose();
            CamAttribs->f4Position = float4(CameraWorldPos, 1);
        }

        {
            MapHelper<LightAttribs> lightAttribs(m_pImmediateContext, m_LightAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
            lightAttribs->f4Direction = m_LightDirection;
            lightAttribs->f4Intensity = m_LightColor * m_LightIntensity;
        }
    }

    m_GLTFRenderer->Render(m_pImmediateContext, *m_Model, m_RenderParams);

    if (m_ShowEnvironment)
    {
        {
            MapHelper<EnvMapRenderAttribs> EnvMapAttribs(m_pImmediateContext, m_EnvMapRenderAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
            EnvMapAttribs->TMAttribs.uiToneMappingMode    = TONE_MAPPING_MODE_UNCHARTED2;
            EnvMapAttribs->TMAttribs.bAutoExposure        = 0;
            EnvMapAttribs->TMAttribs.fMiddleGray          = m_RenderParams.MiddleGray;
            EnvMapAttribs->TMAttribs.bLightAdaptation     = 0;
            EnvMapAttribs->TMAttribs.fWhitePoint          = m_RenderParams.WhitePoint;
            EnvMapAttribs->TMAttribs.fLuminanceSaturation = 1.0;
            EnvMapAttribs->AverageLogLum = m_RenderParams.AverageLogLum;
        }
        m_pImmediateContext->SetPipelineState(m_EnvMapPSO);
        m_pImmediateContext->CommitShaderResources(m_EnvMapSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
        DrawAttribs drawAttribs(3, DRAW_FLAG_VERIFY_ALL);
        m_pImmediateContext->Draw(drawAttribs);
    }
}


// Rotating sponge
void GLTFViewer::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
}

}
