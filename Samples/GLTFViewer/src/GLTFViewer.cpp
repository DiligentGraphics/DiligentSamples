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

namespace Diligent
{

#include "BasicStructures.fxh"

SampleBase* CreateSample()
{
    return new GLTFViewer();
}

void GLTFViewer::Initialize(IEngineFactory* pEngineFactory, IRenderDevice* pDevice, IDeviceContext** ppContexts, Uint32 NumDeferredCtx, ISwapChain* pSwapChain)
{
    SampleBase::Initialize(pEngineFactory, pDevice, ppContexts, NumDeferredCtx, pSwapChain);
    auto BackBufferFmt  = m_pSwapChain->GetDesc().ColorBufferFormat;
    auto DepthBufferFmt = m_pSwapChain->GetDesc().DepthBufferFormat;
    GLTF_PBR_Renderer::CreateInfo RendererCI;
    RendererCI.RTVFmt = BackBufferFmt;
    RendererCI.DSVFmt = DepthBufferFmt;
    RendererCI.AllowDebugView = true;
    m_GLTFRenderer.reset(new GLTF_PBR_Renderer(m_pDevice, m_pImmediateContext, RendererCI));

    CreateUniformBuffer(m_pDevice, sizeof(CameraAttribs), "Camera attribs buffer", &m_CameraAttribsCB);
    CreateUniformBuffer(m_pDevice, sizeof(LightAttribs),  "Light attribs buffer",  &m_LightAttribsCB);
    StateTransitionDesc Barriers [] =
    {
        {m_CameraAttribsCB, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, true},
        {m_LightAttribsCB,  RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, true},
    };
    m_pImmediateContext->TransitionResourceStates(_countof(Barriers), Barriers);


    m_Model.reset(new GLTF::Model(m_pDevice, m_pImmediateContext, "models/DamagedHelmet.gltf"));
    m_GLTFRenderer->InitializeResourceBindings(*m_Model, m_CameraAttribsCB, m_LightAttribsCB);

    float3 axis(-1, 1, 0);
    m_Rotation = Quaternion::RotationFromAxisAngle(axis, PI_F/4.f);
    m_LightDirection  = normalize(float3(0.5f, -0.6f, -0.2f));

    // Create a tweak bar
    TwBar *bar = TwNewBar("Settings");
    int barSize[2] = {224 * m_UIScale, 400 * m_UIScale};
    TwSetParam(bar, NULL, "size", TW_PARAM_INT32, 2, barSize);

    TwAddVarRW(bar, "Rotation",           TW_TYPE_QUAT4F,  &m_Rotation,        "opened=true axisz=-z");
    TwAddVarRW(bar, "Light direction",    TW_TYPE_DIR3F,   &m_LightDirection,  "opened=true axisz=-z");
    TwAddVarRW(bar, "Light Color",        TW_TYPE_COLOR4F, &m_LightColor,      "opened=false");
    TwAddVarRW(bar, "Light Intensity",    TW_TYPE_FLOAT,   &m_LightIntensity,  "min=0.1 max=5.0 step=0.1");
    TwAddVarRW(bar, "Occlusion strength", TW_TYPE_FLOAT,   &m_RenderParams.OcclusionStrength, "min=0.0 max=1.0 step=0.01");
    TwAddVarRW(bar, "Emission scale",     TW_TYPE_FLOAT,   &m_RenderParams.EmissionScale,     "min=0.0 max=1.0 step=0.01");


    TwEnumVal DebugViewType[] = // array used to describe the shadow map resolution
    {
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::None),         "None"       },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::BaseColor),    "Base Color" },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::NormalMap),    "Normal Map" },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Occlusion),    "Occlusion"  },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Emissive),     "Emissive"   },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Metallic),     "Metallic"   },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Roughness),    "Roughness"  },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::DiffuseColor), "Diffuse color"},
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::SpecularColor),"Specular color (R0)"},
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Reflectance90),"Reflectance90"}
    };
    TwType DebugViewTwType = TwDefineEnum( "Debug view", DebugViewType, _countof( DebugViewType ) );
    TwAddVarRW( bar, "Debug view", DebugViewTwType, &m_RenderParams.DebugView, "" );
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
        float4x4 CameraView = m_Rotation.ToMatrix() * float4x4::TranslationD3D(0.f, 0.0f, 5.0f);
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
            CamAttribs->mViewProjT = CameraViewProj.Transpose();
            CamAttribs->f4Position = float4(CameraWorldPos, 1);
        }

        {
            MapHelper<LightAttribs> lightAttribs(m_pImmediateContext, m_LightAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD);
            lightAttribs->f4Direction = m_LightDirection;
            lightAttribs->f4Intensity = m_LightColor * m_LightIntensity;
        }
    }

    m_GLTFRenderer->Render(m_pImmediateContext, *m_Model, m_RenderParams);
}


// Rotating sponge
void GLTFViewer::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
}

}
