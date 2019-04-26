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
    m_GLTFRenderer.reset(new GLTF_PBR_Renderer(m_pDevice, m_pImmediateContext, BackBufferFmt, DepthBufferFmt));

    CreateUniformBuffer(m_pDevice, sizeof(CameraAttribs), "Camera attribs buffer", &m_CameraAttribs);
    CreateUniformBuffer(m_pDevice, sizeof(LightAttribs),  "Light attribs buffer", &m_LightAttribs);

    m_Model.reset(new GLTF::Model(m_pDevice, m_pImmediateContext, "models/DamagedHelmet.gltf"));
    m_GLTFRenderer->InitializeResourceBindings(*m_Model, m_CameraAttribs, m_LightAttribs);

    // Create a tweak bar
    TwBar *bar = TwNewBar("Settings");
    int barSize[2] = {224 * m_UIScale, 120 * m_UIScale};
    TwSetParam(bar, NULL, "size", TW_PARAM_INT32, 2, barSize);

    TwEnumVal DebugViewType[] = // array used to describe the shadow map resolution
    {
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::None),        "None"       },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::BaseColor),   "Base Color" },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::NormalMap),   "Normal Map" },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Occlusion),   "Occlusion"  },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Emissive),    "Emissive"   },
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Metallicity), "Metallicity"},
        {static_cast<int>(GLTF_PBR_Renderer::RenderInfo::DebugViewType::Roughness),   "Roughness"  },
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
        float4x4 CameraView = //float4x4::RotationY_D3D( static_cast<float>(TMP_CurrTime) * 1.0f) * float4x4::RotationX_D3D(-PI_F*0.1f) * 
            float4x4::TranslationD3D(0.f, 0.0f, 5.0f);
        float NearPlane = 0.1f;
        float FarPlane = 100.f;
        float aspectRatio = static_cast<float>(m_pSwapChain->GetDesc().Width) / static_cast<float>(m_pSwapChain->GetDesc().Height);
        // Projection matrix differs between DX and OpenGL
        auto Proj = float4x4::ProjectionD3D(PI_F / 4.f, aspectRatio, NearPlane, FarPlane, m_pDevice->GetDeviceCaps().IsGLDevice());
        // Compute world-view-projection matrix
        auto CameraViewProj = CameraView * Proj;

        MapHelper<CameraAttribs> CamAttribs(m_pImmediateContext, m_CameraAttribs, MAP_WRITE, MAP_FLAG_DISCARD);
        CamAttribs->mViewProjT = CameraViewProj.Transpose();
    }

    m_GLTFRenderer->Render(m_pImmediateContext, *m_Model, m_RenderParams);
}


// Rotating sponge
void GLTFViewer::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    TMP_CurrTime = CurrTime;
}

}
