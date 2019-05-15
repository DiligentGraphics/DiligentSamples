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

#include <cmath>
#include "GLTFViewer.h"
#include "MapHelper.h"
#include "BasicMath.h"
#include "GraphicsUtilities.h"
#include "AntTweakBar.h"
#include "TextureUtilities.h"
#include "CommonlyUsedStates.h"
#include "ShaderMacroHelper.h"
#include "FileSystem.h"

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
        float MipLevel;
        float Unusued1;
        float Unusued2;
    };
}

const std::pair<const char*, const char*> GLTFViewer::GLTFModels[] =
{
    {"Damaged Helmet",      "models/DamagedHelmet/DamagedHelmet.gltf"},
    {"Metal Rough Spheres", "models/MetalRoughSpheres/MetalRoughSpheres.gltf"},
    {"Flight Helmet",       "models/FlightHelmet/FlightHelmet.gltf"},
    {"Cesium Man",          "models/CesiumMan/CesiumMan.gltf"},
    {"Boom Box",            "models/BoomBoxWithAxes/BoomBoxWithAxes.gltf"},
    {"Normal Tangent Test", "models/NormalTangentTest/NormalTangentTest.gltf"}
};

void GLTFViewer::LoadModel(const char* Path)
{
    TwBar* bar = TwGetBarByName("Settings");

    if (m_Model)
    {
        if (!m_Model->Animations.empty())
        {
            TwRemoveVar(bar, "Play");
            TwRemoveVar(bar, "Active Animation");
        }

        m_GLTFRenderer->ReleaseResourceBindings(*m_Model);
        m_PlayAnimation  = false;
        m_AnimationIndex = 0;
        m_AnimationTimers.clear();
    }

    m_Model.reset(new GLTF::Model(m_pDevice, m_pImmediateContext, Path));
    m_GLTFRenderer->InitializeResourceBindings(*m_Model, m_CameraAttribsCB, m_LightAttribsCB);

    // Center and scale model
    float3 ModelDim{m_Model->aabb[0][0], m_Model->aabb[1][1], m_Model->aabb[2][2]};
    float Scale = (1.0f / std::max(std::max(ModelDim.x, ModelDim.y), ModelDim.z)) * 0.5f;
    auto Translate = -float3(m_Model->aabb[3][0], m_Model->aabb[3][1], m_Model->aabb[3][2]);
    Translate += -0.5f * ModelDim;
    float4x4 InvYAxis = float4x4::Identity();
    InvYAxis._22 = -1;
    m_ModelTransform = float4x4::Translation(Translate) * float4x4::Scale(Scale) * InvYAxis;

    if (!m_Model->Animations.empty())
    {
        TwAddVarRW(bar, "Play", TW_TYPE_BOOLCPP, &m_PlayAnimation, "group='Animation'");
        std::vector<TwEnumVal> AnimationEnumVals(m_Model->Animations.size());
        for (int i=0; i < m_Model->Animations.size(); ++i)
            AnimationEnumVals.emplace_back(TwEnumVal{i, m_Model->Animations[i].Name.c_str()});
        TwType AnimationsEnumTwType = TwDefineEnum( "AnimationsEnum", AnimationEnumVals.data(), static_cast<unsigned int>(AnimationEnumVals.size()));
        TwAddVarRW( bar, "Active Animation", AnimationsEnumTwType, &m_AnimationIndex, "group='Animation'");
        m_AnimationTimers.resize(m_Model->Animations.size());
        m_AnimationIndex = 0;
        m_PlayAnimation  = true;
    }
}

void GLTFViewer::ResetView()
{
    m_CameraYaw   = 0;
    m_CameraPitch = 0;
    m_ModelRotation = Quaternion::RotationFromAxisAngle(float3{0.f, 1.0f, 0.0f}, -PI_F / 2.f);
}


void GLTFViewer::Initialize(IEngineFactory* pEngineFactory, IRenderDevice* pDevice, IDeviceContext** ppContexts, Uint32 NumDeferredCtx, ISwapChain* pSwapChain)
{
    SampleBase::Initialize(pEngineFactory, pDevice, ppContexts, NumDeferredCtx, pSwapChain);

    ResetView();

    RefCntAutoPtr<ITexture> EnvironmentMap;
    CreateTextureFromFile("textures/papermill.ktx", TextureLoadInfo{"Environment map"}, m_pDevice, &EnvironmentMap);
    m_EnvironmentMapSRV = EnvironmentMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    auto BackBufferFmt  = m_pSwapChain->GetDesc().ColorBufferFormat;
    auto DepthBufferFmt = m_pSwapChain->GetDesc().DepthBufferFormat;
    GLTF_PBR_Renderer::CreateInfo RendererCI;
    RendererCI.RTVFmt         = BackBufferFmt;
    RendererCI.DSVFmt         = DepthBufferFmt;
    RendererCI.AllowDebugView = true;
    RendererCI.UseIBL         = true;
    RendererCI.FrontCCW       = true;
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

    m_GLTFRenderer->PrecomputeCubemaps(m_pDevice, m_pImmediateContext, m_EnvironmentMapSRV);

    CreateEnvMapPSO();

    m_LightDirection  = normalize(float3(0.5f, -0.6f, -0.2f));

    // Create a tweak bar
    TwBar *bar = TwNewBar("Settings");
    int barSize[2] = {250 * m_UIScale, 600 * m_UIScale};
    TwSetParam(bar, NULL, "size", TW_PARAM_INT32, 2, barSize);

    {
        TwEnumVal ModelEnumVals[_countof(GLTFModels)];
        for(int i=0; i < _countof(GLTFModels); ++i)
            ModelEnumVals[i] = TwEnumVal{i, GLTFModels[i].first};
        TwType ModelEnumTwType = TwDefineEnum( "Model", ModelEnumVals, _countof(ModelEnumVals) );
        TwAddVarCB( bar, "Model", ModelEnumTwType,
            []( const void *value, void * clientData )
            {
                GLTFViewer *pViewer = reinterpret_cast<GLTFViewer*>( clientData );
                pViewer->m_SelectedModel = *reinterpret_cast<const int*>(value);
                pViewer->LoadModel(pViewer->GLTFModels[pViewer->m_SelectedModel].second);
            },
            [](void *value, void * clientData)
            {
                GLTFViewer *pViewer = reinterpret_cast<GLTFViewer*>( clientData );
                *reinterpret_cast<int*>(value) = static_cast<int>(pViewer->m_SelectedModel);
            },
            this, "");
    }
#ifdef PLATFORM_WIN32
    TwAddButton(bar, "Load model", 
                [](void *clientData)
                {
                    auto FileName = FileSystem::OpenFileDialog("Select GLTF file", "glTF files\0*.gltf;*.glb\0");
                    if (!FileName.empty())
                    {
                        auto* pViewer = reinterpret_cast<GLTFViewer*>( clientData );
                        pViewer->LoadModel(FileName.c_str());
                    }
                }, 
                this, "");
#endif

    TwAddVarRO(bar, "Camera Rotation",    TW_TYPE_QUAT4F,  &m_CameraRotation,  "opened=true axisz=-z");
    TwAddVarRO(bar, "Model Rotation",     TW_TYPE_QUAT4F,  &m_ModelRotation,   "opened=true axisz=-z");
    TwAddButton(bar, "Reset view", 
                [](void *clientData)
                {
                    auto* pViewer = reinterpret_cast<GLTFViewer*>( clientData );
                    pViewer->ResetView();
                }, 
                this, "");
    TwAddVarRW(bar, "Light direction",    TW_TYPE_DIR3F,   &m_LightDirection,  "opened=true axisz=-z");
    TwAddVarRW(bar, "Camera dist",        TW_TYPE_FLOAT,   &m_CameraDist,      "min=0.1 max=5.0 step=0.1");
    TwAddVarRW(bar, "Light Color",        TW_TYPE_COLOR4F, &m_LightColor,      "group='Lighting' opened=false");
    TwAddVarRW(bar, "Light Intensity",    TW_TYPE_FLOAT,   &m_LightIntensity,  "group='Lighting' min=0.0 max=50.0 step=0.1");
    TwAddVarRW(bar, "Occlusion strength", TW_TYPE_FLOAT,   &m_RenderParams.OcclusionStrength, "group='Lighting' min=0.0 max=1.0 step=0.01");
    TwAddVarRW(bar, "Emission scale",     TW_TYPE_FLOAT,   &m_RenderParams.EmissionScale,     "group='Lighting' min=0.0 max=1.0 step=0.01");
    TwAddVarRW(bar, "IBL scale",          TW_TYPE_FLOAT,   &m_RenderParams.IBLScale,          "group='Lighting' min=0.0 max=1.0 step=0.01");
    TwAddVarRW(bar, "Average log lum",    TW_TYPE_FLOAT,   &m_RenderParams.AverageLogLum,     "group='Tone mapping' min=0.01 max=10.0 step=0.01");
    TwAddVarRW(bar, "Middle gray",        TW_TYPE_FLOAT,   &m_RenderParams.MiddleGray,        "group='Tone mapping' min=0.01 max=1.0 step=0.01");
    TwAddVarRW(bar, "White point",        TW_TYPE_FLOAT,   &m_RenderParams.WhitePoint,        "group='Tone mapping' min=0.1  max=20.0 step=0.1");
    {
        TwEnumVal BackgroundModeEnumVals[] =
        {
            {static_cast<int>(BackgroundMode::None),              "None"             },
            {static_cast<int>(BackgroundMode::EnvironmentMap),    "Environmen Map"   },
            {static_cast<int>(BackgroundMode::Irradiance),        "Irradiance"       },
            {static_cast<int>(BackgroundMode::PrefilteredEnvMap), "PrefilteredEnvMap"}
        };
        TwType BackgroundModeEnumTwType = TwDefineEnum( "Background mode", BackgroundModeEnumVals, _countof(BackgroundModeEnumVals) );
        TwAddVarCB( bar, "Background mode", BackgroundModeEnumTwType,
            []( const void *value, void * clientData )
            {
                GLTFViewer *pViewer = reinterpret_cast<GLTFViewer*>( clientData );
                pViewer->m_BackgroundMode = static_cast<GLTFViewer::BackgroundMode>(*reinterpret_cast<const int*>(value));
                pViewer->CreateEnvMapSRB();
            },
            [](void *value, void * clientData)
            {
                GLTFViewer *pViewer = reinterpret_cast<GLTFViewer*>( clientData );
                *reinterpret_cast<int*>(value) = static_cast<int>(pViewer->m_BackgroundMode);
            },
            this, "");
    }
    TwAddVarRW(bar, "Env map mip", TW_TYPE_FLOAT, &m_EnvMapMipLevel, "min=0.0 max=7.0 step=0.1");
    
    {
        TwEnumVal DebugViewType[] = // array used to describe the shadow map resolution
        {
            {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::None),            "None"       },
            {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::BaseColor),       "Base Color" },
            {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Transparency),    "Transparency"},
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
            {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::NdotV),           "n*v"},
            {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::DiffuseIBL),      "Diffuse IBL"},
            {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::SpecularIBL),     "Specular IBL"}
        };
        TwType DebugViewTwType = TwDefineEnum( "Debug view", DebugViewType, _countof( DebugViewType ) );
        TwAddVarRW( bar, "Debug view", DebugViewTwType, &m_RenderParams.DebugView, "" );
    }

    LoadModel(GLTFModels[m_SelectedModel].second);
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
    CreateEnvMapSRB();
}

void GLTFViewer::CreateEnvMapSRB()
{
    if(m_BackgroundMode != BackgroundMode::None)
    {
        m_EnvMapSRB.Release();
        m_EnvMapPSO->CreateShaderResourceBinding(&m_EnvMapSRB, true);
        ITextureView* pEnvMapSRV = nullptr;
        switch(m_BackgroundMode)
        {
            case BackgroundMode::EnvironmentMap:
                pEnvMapSRV = m_EnvironmentMapSRV;
            break;

            case BackgroundMode::Irradiance:
                pEnvMapSRV = m_GLTFRenderer->GetIrradianceCubeSRV();
            break;

            case BackgroundMode::PrefilteredEnvMap:
                pEnvMapSRV = m_GLTFRenderer->GetPrefilteredEnvMapSRV();
            break;

            default:
                UNEXPECTED("Unexpected background mode");
        }
        m_EnvMapSRB->GetVariableByName(SHADER_TYPE_PIXEL, "EnvMap")->Set(pEnvMapSRV);
    }
}

GLTFViewer::~GLTFViewer()
{
}

// Render a frame
void GLTFViewer::Render()
{
    // Clear the back buffer 
    const float ClearColor[] = { 0.032f,  0.032f,  0.032f, 1.0f }; 
    m_pImmediateContext->ClearRenderTarget(nullptr, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(nullptr, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    
    float4x4 CameraView = m_CameraRotation.ToMatrix() * float4x4::Translation(0.f, 0.0f, m_CameraDist);
    float4x4 CameraWorld = CameraView.Inverse();
    float3 CameraWorldPos = float3::MakeVector(CameraWorld[3]);
    float NearPlane = 0.1f;
    float FarPlane = 100.f;
    float aspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);
    // Projection matrix differs between DX and OpenGL
    auto Proj = float4x4::Projection(PI_F / 4.f, aspectRatio, NearPlane, FarPlane, m_pDevice->GetDeviceCaps().IsGLDevice());
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

    m_RenderParams.ModelTransform = m_ModelTransform * m_ModelRotation.ToMatrix();
    m_GLTFRenderer->Render(m_pImmediateContext, *m_Model, m_RenderParams);

    if (m_BackgroundMode != BackgroundMode::None)
    {
        {
            MapHelper<EnvMapRenderAttribs> EnvMapAttribs(m_pImmediateContext, m_EnvMapRenderAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
            EnvMapAttribs->TMAttribs.iToneMappingMode     = TONE_MAPPING_MODE_UNCHARTED2;
            EnvMapAttribs->TMAttribs.bAutoExposure        = 0;
            EnvMapAttribs->TMAttribs.fMiddleGray          = m_RenderParams.MiddleGray;
            EnvMapAttribs->TMAttribs.bLightAdaptation     = 0;
            EnvMapAttribs->TMAttribs.fWhitePoint          = m_RenderParams.WhitePoint;
            EnvMapAttribs->TMAttribs.fLuminanceSaturation = 1.0;
            EnvMapAttribs->AverageLogLum = m_RenderParams.AverageLogLum;
            EnvMapAttribs->MipLevel      =  m_EnvMapMipLevel;
        }
        m_pImmediateContext->SetPipelineState(m_EnvMapPSO);
        m_pImmediateContext->CommitShaderResources(m_EnvMapSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
        DrawAttribs drawAttribs(3, DRAW_FLAG_VERIFY_ALL);
        m_pImmediateContext->Draw(drawAttribs);
    }
}


void GLTFViewer::Update(double CurrTime, double ElapsedTime)
{
    {
        const auto& mouseState = m_InputController.GetMouseState();
        constexpr float RotationSpeed = 0.005f;
        float fYawDelta   = mouseState.DeltaX * RotationSpeed;
        float fPitchDelta = mouseState.DeltaY * RotationSpeed;
        if (mouseState.ButtonFlags & MouseState::BUTTON_FLAG_LEFT)
        {
            m_CameraYaw   += fYawDelta;
            m_CameraPitch += fPitchDelta;
        }

        // Apply extra rotations to adjust the view to match Khronos GLTF viewer
        m_CameraRotation =
            Quaternion::RotationFromAxisAngle(float3{1,0,0}, -m_CameraPitch) *
            Quaternion::RotationFromAxisAngle(float3{0,1,0}, -m_CameraYaw)   *
            Quaternion::RotationFromAxisAngle(float3{0.75f, 0.0f, 0.75f}, PI_F);

        if (mouseState.ButtonFlags & MouseState::BUTTON_FLAG_RIGHT)
        {
            auto CameraView = m_CameraRotation.ToMatrix();
            auto CameraWorld = CameraView.Transpose();

            float3 CameraRight = float3::MakeVector(CameraWorld[0]);
            float3 CameraUp    = float3::MakeVector(CameraWorld[1]);
            m_ModelRotation = 
                Quaternion::RotationFromAxisAngle(CameraRight, -fPitchDelta) *
                Quaternion::RotationFromAxisAngle(CameraUp,    -fYawDelta)   *
                m_ModelRotation;
        }

        m_CameraDist -= mouseState.WheelDelta * 0.25f;
    }

    SampleBase::Update(CurrTime, ElapsedTime);

    if (!m_Model->Animations.empty() && m_PlayAnimation)
    {
        float& AnimationTimer = m_AnimationTimers[m_AnimationIndex];
        AnimationTimer += static_cast<float>(ElapsedTime);
        AnimationTimer = std::fmod(AnimationTimer, m_Model->Animations[m_AnimationIndex].End);
        m_Model->UpdateAnimation(m_AnimationIndex, AnimationTimer);
    }
}

}
