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

// This file is derived from the open source project provided by Intel Corportaion that
// requires the following notice to be kept:
//--------------------------------------------------------------------------------------
// Copyright 2013 Intel Corporation
// All Rights Reserved
//
// Permission is granted to use, copy, distribute and prepare derivative works of this
// software for any purpose and without fee, provided, that the above copyright notice
// and this statement appear in all copies.  Intel makes no representations about the
// suitability of this software for any purpose.  THIS SOFTWARE IS PROVIDED "AS IS."
// INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES, EXPRESS OR IMPLIED, AND ALL LIABILITY,
// INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES, FOR THE USE OF THIS SOFTWARE,
// INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY RIGHTS, AND INCLUDING THE
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  Intel does not
// assume any responsibility for any errors which may appear in this software nor any
// responsibility to update it.
//--------------------------------------------------------------------------------------

#pragma once

#include "Structures.fxh"
#include "ScriptParser.h"
#include "ShaderMacroHelper.h"

struct FrameAttribs
{
    IRenderDevice *pDevice;
    IDeviceContext *pDeviceContext;
    
    double dElapsedTime;

    LightAttribs *pLightAttribs;
    IBuffer *pcbLightAttribs;
    IBuffer *pcbCameraAttribs;

    //CameraAttribs CameraAttribs;
    
    ITextureView *ptex2DSrcColorBufferSRV;
    ITextureView *ptex2DSrcColorBufferRTV;
    ITextureView *ptex2DSrcDepthBufferDSV;
    ITextureView *ptex2DSrcDepthBufferSRV;
    ITextureView *ptex2DShadowMapSRV;
    ITextureView *pDstRTV;
};

class LightSctrPostProcess
{
public:
    LightSctrPostProcess(IRenderDevice* in_pDevice, 
                         IDeviceContext *in_pContext,
                         TEXTURE_FORMAT BackBufferFmt,
                         TEXTURE_FORMAT DepthBufferFmt,
                         TEXTURE_FORMAT OffscreenBackBuffer);
    ~LightSctrPostProcess();


    void OnWindowResize(IRenderDevice* pDevice, Uint32 uiBackBufferWidth, Uint32 uiBackBufferHeight);

    void PerformPostProcessing(FrameAttribs &FrameAttribs,
                               PostProcessingAttribs &PPAttribs);

    void ComputeSunColor(const float3 &vDirectionOnSun,
                         const float4 &f4ExtraterrestrialSunColor,
                         float4 &f4SunColorAtGround,
                         float4 &f4AmbientLight);
    
    void RenderSun(FrameAttribs &FrameAttribs);
    IBuffer *GetMediaAttribsCB(){return m_pcbMediaAttribs;}
    ITextureView* GetPrecomputedNetDensitySRV(){return m_ptex2DOccludedNetDensityToAtmTopSRV;}
    ITextureView* GetAmbientSkyLightSRV(IRenderDevice *pDevice, IDeviceContext *pContext);

private:
    void ReconstructCameraSpaceZ(FrameAttribs &FrameAttribs);
    void RenderSliceEndpoints(FrameAttribs &FrameAttribs);
    void RenderCoordinateTexture(FrameAttribs &FrameAttribs);
    void RenderCoarseUnshadowedInctr(FrameAttribs &FrameAttribs);
    void RefineSampleLocations(FrameAttribs &FrameAttribs);
    void MarkRayMarchingSamples(FrameAttribs &FrameAttribs);
    void RenderSliceUVDirAndOrig(FrameAttribs &FrameAttribs);
    void Build1DMinMaxMipMap(FrameAttribs &FrameAttribs, int iCascadeIndex);
    void DoRayMarching(FrameAttribs &FrameAttribs, Uint32 uiMaxStepsAlongRay, int iCascadeIndex);
    void InterpolateInsctrIrradiance(FrameAttribs &FrameAttribs);
    void UnwarpEpipolarScattering(FrameAttribs &FrameAttribs, bool bRenderLuminance);
    void UpdateAverageLuminance(FrameAttribs &FrameAttribs);
    enum class EFixInscatteringMode
    {
        LuminanceOnly = 0,
        FixInscattering = 1,
        FullScreenRayMarching = 2
    };
    void FixInscatteringAtDepthBreaks(FrameAttribs &FrameAttribs, Uint32 uiMaxStepsAlongRay, EFixInscatteringMode Mode);
    void RenderSampleLocations(FrameAttribs &FrameAttribs);
    void CreatePrecomputedOpticalDepthTexture(IRenderDevice *pDevice, IDeviceContext *pContext);
    void CreatePrecomputedScatteringLUT(IRenderDevice *pDevice, IDeviceContext *pContext);
    void CreateRandomSphereSamplingTexture(IRenderDevice *pDevice);
    void ComputeAmbientSkyLightTexture(IRenderDevice *pDevice, IDeviceContext *pContext);
    void ComputeScatteringCoefficients(IDeviceContext *pDeviceCtx = NULL);

    void DefineMacros(Diligent::ShaderMacroHelper &Macros);
    
    PostProcessingAttribs m_PostProcessingAttribs;
    bool m_bUseCombinedMinMaxTexture;
    Uint32 m_uiSampleRefinementCSThreadGroupSize;
    Uint32 m_uiSampleRefinementCSMinimumThreadGroupSize;

    RefCntAutoPtr<Diligent::ScriptParser> m_pRenderScript;

    Diligent::RefCntAutoPtr<ITextureView> m_ptex2DMinMaxShadowMapSRV[2];
    Diligent::RefCntAutoPtr<ITextureView> m_ptex2DMinMaxShadowMapRTV[2];

    static const int sm_iNumPrecomputedHeights = 1024;
    static const int sm_iNumPrecomputedAngles = 1024;

    
    static const int sm_iPrecomputedSctrUDim = 32;
    static const int sm_iPrecomputedSctrVDim = 128;
    static const int sm_iPrecomputedSctrWDim = 64;
    static const int sm_iPrecomputedSctrQDim = 16;
    Diligent::RefCntAutoPtr<ITextureView> m_ptex3DSingleScatteringSRV;
    Diligent::RefCntAutoPtr<ITextureView> m_ptex3DHighOrderScatteringSRV;
    Diligent::RefCntAutoPtr<ITextureView> m_ptex3DMultipleScatteringSRV;
    
    static const int sm_iNumRandomSamplesOnSphere = 128;
    Diligent::RefCntAutoPtr<ITextureView> m_ptex2DSphereRandomSamplingSRV;

    void CreateMinMaxShadowMap(IRenderDevice* pDevice);

    static const int sm_iLowResLuminanceMips = 7; // 64x64
    Diligent::RefCntAutoPtr<ITextureView> m_ptex2DLowResLuminanceRTV, m_ptex2DLowResLuminanceSRV;
    
    static const int sm_iAmbientSkyLightTexDim = 1024;
    Diligent::RefCntAutoPtr<ITextureView> m_ptex2DAmbientSkyLightSRV;
    Diligent::RefCntAutoPtr<ITextureView> m_ptex2DOccludedNetDensityToAtmTopSRV;

    Diligent::RefCntAutoPtr<IShader> m_pReconstrCamSpaceZPS;
    Diligent::RefCntAutoPtr<IShader> m_pRendedSliceEndpointsPS;
    Diligent::RefCntAutoPtr<IShader> m_pRendedCoordTexPS;
    Diligent::RefCntAutoPtr<IShader> m_pRefineSampleLocationsCS;
    Diligent::RefCntAutoPtr<IPipelineState> m_pRefineSampleLocationsPSO;
    Diligent::RefCntAutoPtr<IShaderResourceBinding> m_pRefineSampleLocationsSRB;
    Diligent::RefCntAutoPtr<IShader> m_pRenderCoarseUnshadowedInsctrPS;
    Diligent::RefCntAutoPtr<IShader> m_pMarkRayMarchingSamplesInStencilPS;
    Diligent::RefCntAutoPtr<IShader> m_pRenderSliceUVDirInSMPS;
    Diligent::RefCntAutoPtr<IShader> m_pInitializeMinMaxShadowMapPS;
    Diligent::RefCntAutoPtr<IShader> m_pComputeMinMaxSMLevelPS;
    Diligent::RefCntAutoPtr<IShaderResourceBinding> m_pComputeMinMaxSMLevelSRB[2];
    Diligent::RefCntAutoPtr<IShader> m_pDoRayMarchPS[2]; // 0 - min/max optimization disabled; 1 - min/max optimization enabled
    Diligent::RefCntAutoPtr<IShader> m_pInterpolateIrradiancePS;
    Diligent::RefCntAutoPtr<IShader> m_pUnwarpEpipolarSctrImgPS;
    Diligent::RefCntAutoPtr<IShader> m_pUnwarpAndRenderLuminancePS;
    Diligent::RefCntAutoPtr<IShader> m_pUpdateAverageLuminancePS;
    Diligent::RefCntAutoPtr<IShader> m_pFixInsctrAtDepthBreaksPS[2];// 0 - perform tone mapping, 1 - render luminance only

    Diligent::RefCntAutoPtr<IResourceMapping> m_pResMapping;

    Diligent::RefCntAutoPtr<ITextureView> m_ptex2DEpipolarInscatteringRTV;
    Diligent::RefCntAutoPtr<ITextureView> m_ptex2DEpipolarExtinctionRTV;
    Diligent::RefCntAutoPtr<ITextureView> m_ptex2DEpipolarImageDSV;

    Diligent::RefCntAutoPtr<IShader> m_pRenderSampleLocationsVS, m_pRenderSampleLocationsPS;
    Diligent::RefCntAutoPtr<ISampler> m_pPointClampSampler, m_pLinearClampSampler;

    Diligent::RefCntAutoPtr<IShader> m_pPrecomputeSingleSctrCS;
    Diligent::RefCntAutoPtr<IShader> m_pComputeSctrRadianceCS;
    Diligent::RefCntAutoPtr<IShader> m_pComputeScatteringOrderCS;
    Diligent::RefCntAutoPtr<IShader> m_pInitHighOrderScatteringCS, m_pUpdateHighOrderScatteringCS;
    Diligent::RefCntAutoPtr<IShader> m_pCombineScatteringOrdersCS;
    Diligent::RefCntAutoPtr<IShader> m_pPrecomputeAmbientSkyLightPS;

    Diligent::RefCntAutoPtr<IPipelineState> m_pPrecomputeSingleSctrPSO;
    Diligent::RefCntAutoPtr<IShaderResourceBinding> m_pPrecomputeSingleSctrSRB;
    Diligent::RefCntAutoPtr<IPipelineState> m_pComputeSctrRadiancePSO;
    Diligent::RefCntAutoPtr<IShaderResourceBinding> m_pComputeSctrRadianceSRB;
    Diligent::RefCntAutoPtr<IPipelineState> m_pComputeScatteringOrderPSO;
    Diligent::RefCntAutoPtr<IShaderResourceBinding> m_pComputeScatteringOrderSRB;
    Diligent::RefCntAutoPtr<IPipelineState> m_pInitHighOrderScatteringPSO, m_pUpdateHighOrderScatteringPSO;
    Diligent::RefCntAutoPtr<IShaderResourceBinding> m_pInitHighOrderScatteringSRB, m_pUpdateHighOrderScatteringSRB;
    Diligent::RefCntAutoPtr<IPipelineState> m_pCombineScatteringOrdersPSO;
    Diligent::RefCntAutoPtr<IShaderResourceBinding> m_pCombineScatteringOrdersSRB;

    Diligent::RefCntAutoPtr<ITexture> m_ptex3DHighOrderSctr, m_ptex3DHighOrderSctr2;

    Diligent::RefCntAutoPtr<IBuffer> m_pcbPostProcessingAttribs;
    Diligent::RefCntAutoPtr<IBuffer> m_pcbMediaAttribs;
    Diligent::RefCntAutoPtr<IBuffer> m_pcbMiscParams;

    Uint32 m_uiBackBufferWidth, m_uiBackBufferHeight;
    
    const float m_fTurbidity = 1.02f;
    AirScatteringAttribs m_MediaParams;

    enum UpToDateResourceFlags
    {
        AuxTextures                 = 0x001,
        ExtinctionTexture           = 0x002,
        SliceUVDirAndOriginTex      = 0x004,
        PrecomputedOpticaLDepthTex  = 0x008,
        LowResLuminamceTex          = 0x010,
        AmbientSkyLightTex          = 0x020,
        PrecomputedIntegralsTex     = 0x040
    };
    Uint32 m_uiUpToDateResourceFlags;
    Diligent::RefCntAutoPtr<ITextureView> m_ptex2DShadowMapSRV;
};
