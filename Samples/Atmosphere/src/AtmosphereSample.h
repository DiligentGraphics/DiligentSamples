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

#pragma once 

#include "SampleBase.h"
#include "ScriptParser.h"
#include "BasicMath.h"
#include "EarthHemisphere.h"
#include "ElevationDataSource.h"

using namespace Diligent;

class AtmosphereSample : public SampleBase
{
public:
    AtmosphereSample(IRenderDevice *pDevice, IDeviceContext *pImmediateContext, ISwapChain *pSwapChain);
    ~AtmosphereSample();
    virtual void Render()override;
    virtual void Update(double CurrTime, double ElapsedTime)override;
    virtual void WindowResize( Uint32 Width, Uint32 Height )override;
    virtual const Diligent::Char* GetSampleName()const override{return "Atmosphere Sample";}
    
private:
    void CreateShadowMap();
    void ReleaseShadowMap();
    void RenderShadowMap( IDeviceContext *pContext, LightAttribs &LightAttribs, const float4x4 &mCameraView, const float4x4 &mCameraProj );

    static void SetNumCascadesCB( const void *value, void * clientData );
    static void GetNumCascadesCB( void *value, void * clientData );
    static void SetShadowMapResCB( const void *value, void * clientData );
    static void GetShadowMapResCB( void *value, void * clientData );

    void UpdateGUI();

#if 0
    Quaternion m_SpongeRotation; // model rotation
#endif

    float3 m_f3LightDir;          // light direction vector
    float3 m_f3CameraDir;      // tmp camera view direction vector
    float3 m_f3CameraPos;
    float4x4 m_mCameraView;
    float4x4 m_mCameraProj;

    RefCntAutoPtr<Diligent::IBuffer> m_pcbCameraAttribs;
    RefCntAutoPtr<Diligent::IBuffer> m_pcbLightAttribs;
    std::vector< RefCntAutoPtr<Diligent::ITextureView> > m_pShadowMapDSVs;
    RefCntAutoPtr<Diligent::ITextureView> m_pShadowMapSRV;

    Diligent::Uint32 m_uiShadowMapResolution;
    float m_fCascadePartitioningFactor;
    bool m_bVisualizeCascades;

    RenderingParams m_TerrainRenderParams;
    PostProcessingAttribs m_PPAttribs;
	Diligent::String m_strRawDEMDataFile;
	Diligent::String m_strMtrlMaskFile;
    Diligent::String m_strTileTexPaths[EarthHemsiphere::NUM_TILE_TEXTURES];
    Diligent::String m_strNormalMapTexPaths[EarthHemsiphere::NUM_TILE_TEXTURES];

    float m_fMinElevation, m_fMaxElevation;
	std::unique_ptr<ElevationDataSource> m_pElevDataSource;
    EarthHemsiphere m_EarthHemisphere;
    bool m_bIsDXDevice;

    std::unique_ptr<class LightSctrPostProcess> m_pLightSctrPP;

    bool m_bEnableLightScattering;
    float m_fScatteringScale;
    float m_fElapsedTime;
    float4 m_f4CustomRlghBeta, m_f4CustomMieBeta;

    RefCntAutoPtr<ITexture>  m_pOffscreenColorBuffer;
    RefCntAutoPtr<ITexture>  m_pOffscreenDepthBuffer;
};
