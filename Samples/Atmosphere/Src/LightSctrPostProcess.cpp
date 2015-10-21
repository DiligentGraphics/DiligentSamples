/*     Copyright 2015 Egor Yusov
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

#include "pch.h"

#include "LightSctrPostProcess.h"
#include "ConvenienceFunctions.h"
#include "GraphicsUtilities.h"
#include "BasicShaderSourceStreamFactory.h"
#include "MapHelper.h"

using namespace Diligent;
using namespace Diligent;


#define _USE_MATH_DEFINES
#include <math.h>

void CreateShader(IRenderDevice *pDevice, const Char* FileName, const Char* EntryPoint, SHADER_TYPE Type, const ShaderMacro *Macros, IShader **ppShader)
{
    ShaderCreationAttribs Attribs;
    Attribs.EntryPoint = EntryPoint;
    Attribs.FilePath = FileName;
    Attribs.Macros = Macros;
    Attribs.SearchDirectories = "shaders;shaders\\atmosphere";
    Attribs.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
    Attribs.Desc.ShaderType = Type;
    Attribs.Desc.Name = EntryPoint;
    BasicShaderSourceStreamFactory BasicSSSFactory(Attribs.SearchDirectories);
    Attribs.pShaderSourceStreamFactory = &BasicSSSFactory;
    pDevice->CreateShader( Attribs, ppShader );
}

LightSctrPostProcess :: LightSctrPostProcess(IRenderDevice* pDevice, 
                                               IDeviceContext *pContext) : 
    m_uiSampleRefinementCSThreadGroupSize(0),
    // Using small group size is inefficient because a lot of SIMD lanes become idle
    m_uiSampleRefinementCSMinimumThreadGroupSize(128),// Must be greater than 32
    m_fTurbidity(1.02f),
    m_bUseCombinedMinMaxTexture(false),
    m_uiUpToDateResourceFlags(0)
{
    pDevice->CreateResourceMapping(ResourceMappingDesc(), &m_pResMapping);
    
    m_pRenderScript = CreateRenderScriptFromFile( "shaders\\LightScattering.lua", pDevice, pContext, [&]( ScriptParser *pScriptParser )
    {
        pScriptParser->SetGlobalVariable( "extResourceMapping", m_pResMapping );
    } );
    m_pRenderScript->GetSamplerByName( "PointClampSampler", &m_pPointClampSampler );
    m_pRenderScript->GetSamplerByName( "LinearClampSampler", &m_pLinearClampSampler );

#if 0
    
    // Create samplers

    D3D11_SAMPLER_DESC SamLinearBorder0Desc = 
    {
        D3D11_FILTER_MIN_MAG_MIP_LINEAR,
        D3D11_TEXTURE_ADDRESS_BORDER,
        D3D11_TEXTURE_ADDRESS_BORDER,
        D3D11_TEXTURE_ADDRESS_BORDER,
        0, //FLOAT MipLODBias;
        0, //UINT MaxAnisotropy;
        D3D11_COMPARISON_NEVER, // D3D11_COMPARISON_FUNC ComparisonFunc;
        {0.f, 0.f, 0.f, 0.f}, //FLOAT BorderColor[ 4 ];
        -FLT_MAX, //FLOAT MinLOD;
        +FLT_MAX //FLOAT MaxLOD;
    };
    V_RETURN( in_pd3dDevice->CreateSamplerState( &SamLinearBorder0Desc, &m_psamLinearBorder0) );

#endif

    CreateUniformBuffer( pDevice, sizeof( PostProcessingAttribs ), &m_pcbPostProcessingAttribs);
    CreateUniformBuffer( pDevice, sizeof( MiscDynamicParams ), &m_pcbMiscParams);

    BufferDesc CBDesc;
    CBDesc.Usage = USAGE_DEFAULT;
    CBDesc.BindFlags = BIND_UNIFORM_BUFFER;
    CBDesc.uiSizeInBytes = sizeof(AirScatteringAttribs);

    BufferData InitData;
    InitData.DataSize = CBDesc.uiSizeInBytes;
    InitData.pData = &m_MediaParams;
    pDevice->CreateBuffer( CBDesc, InitData,  &m_pcbMediaAttribs);

    m_pResMapping->AddResource( "cbPostProcessingAttribs", m_pcbPostProcessingAttribs, false );
    m_pResMapping->AddResource( "cbParticipatingMediaScatteringParams", m_pcbMediaAttribs, false );
    m_pResMapping->AddResource( "cbMiscDynamicParams", m_pcbMiscParams, false );

    ComputeScatteringCoefficients(pContext);

    CreatePrecomputedOpticalDepthTexture(pDevice, pContext);

    // static_cast<int>() is required because Run() gets its arguments by reference
    // and gcc will try to find reference to sm_iAmbientSkyLightTexDim, which does not exist
    m_pRenderScript->Run( "CreateAmbientSkyLightTexture", static_cast<int>(sm_iAmbientSkyLightTexDim) );
    m_pRenderScript->GetTextureViewByName( "tex2DAmbientSkyLightSRV", &m_ptex2DAmbientSkyLightSRV );
}

LightSctrPostProcess :: ~LightSctrPostProcess()
{

}


void LightSctrPostProcess :: OnWindowResize(IRenderDevice* pd3dDevice, Uint32 uiBackBufferWidth, Uint32 uiBackBufferHeight)
{
    m_pRenderScript->Run( "WindowResize", uiBackBufferWidth, uiBackBufferHeight );
    m_uiBackBufferWidth = uiBackBufferWidth;
    m_uiBackBufferHeight = uiBackBufferHeight;

    m_pRendedSliceEndpointsPS.Release();
    m_pRendedCoordTexPS.Release();
    m_pRenderSliceUVDirInSMPS.Release();
    m_pRenderSampleLocationsVS.Release();
    m_pRenderSampleLocationsPS.Release();
    m_pUnwarpEpipolarSctrImgPS.Release();
    m_pUnwarpAndRenderLuminancePS.Release();
}

void LightSctrPostProcess :: DefineMacros(ShaderMacroHelper &Macros)
{
    Macros.AddShaderMacro("NUM_EPIPOLAR_SLICES", m_PostProcessingAttribs.m_uiNumEpipolarSlices);
    Macros.AddShaderMacro("MAX_SAMPLES_IN_SLICE", m_PostProcessingAttribs.m_uiMaxSamplesInSlice);
    Macros.AddShaderMacro("OPTIMIZE_SAMPLE_LOCATIONS", m_PostProcessingAttribs.m_bOptimizeSampleLocations);
    Macros.AddShaderMacro("USE_COMBINED_MIN_MAX_TEXTURE", m_bUseCombinedMinMaxTexture );
    Macros.AddShaderMacro("EXTINCTION_EVAL_MODE", m_PostProcessingAttribs.m_uiExtinctionEvalMode );
    Macros.AddShaderMacro("ENABLE_LIGHT_SHAFTS", m_PostProcessingAttribs.m_bEnableLightShafts);
    Macros.AddShaderMacro("MULTIPLE_SCATTERING_MODE", m_PostProcessingAttribs.m_uiMultipleScatteringMode);
    Macros.AddShaderMacro("SINGLE_SCATTERING_MODE", m_PostProcessingAttribs.m_uiSingleScatteringMode);
    
    {
        std::stringstream ss;
        ss<<"float2("<<m_uiBackBufferWidth<<".0,"<<m_uiBackBufferHeight<<".0)";
        Macros.AddShaderMacro("SCREEN_RESLOUTION", ss.str());
    }

    {
        std::stringstream ss;
        ss<<"float4("<<sm_iPrecomputedSctrUDim<<".0,"
                     <<sm_iPrecomputedSctrVDim<<".0,"
                     <<sm_iPrecomputedSctrWDim<<".0,"
                     <<sm_iPrecomputedSctrQDim<<".0)";
        Macros.AddShaderMacro("PRECOMPUTED_SCTR_LUT_DIM", ss.str());
    }

    Macros.AddShaderMacro("EARTH_RADIUS",   m_MediaParams.fEarthRadius);
    Macros.AddShaderMacro("ATM_TOP_HEIGHT", m_MediaParams.fAtmTopHeight);
    Macros.AddShaderMacro("ATM_TOP_RADIUS", m_MediaParams.fAtmTopRadius);
    
    {
        std::stringstream ss;
        ss<<"float2("<<m_MediaParams.f2ParticleScaleHeight.x<<".0,"<<m_MediaParams.f2ParticleScaleHeight.y<<".0)";
        Macros.AddShaderMacro("PARTICLE_SCALE_HEIGHT", ss.str());
    }
}


void LightSctrPostProcess::CreatePrecomputedOpticalDepthTexture(IRenderDevice* pDevice, 
                                                                 IDeviceContext *pDeviceContext)
{
    // static_cast<int>() is required because Run() gets its arguments by reference
    // and gcc will try to find references to sm_iNumPrecomputedHeights and 
    // sm_iNumPrecomputedAngles, which do not exist
    m_pRenderScript->Run(pDeviceContext, "PrecomputeNetDensityToAtmTop", 
                         static_cast<int>(sm_iNumPrecomputedHeights), 
                         static_cast<int>(sm_iNumPrecomputedAngles) );
    m_uiUpToDateResourceFlags |= UpToDateResourceFlags::PrecomputedOpticaLDepthTex;
    m_ptex2DOccludedNetDensityToAtmTopSRV.Release();
    m_pRenderScript->GetTextureViewByName( "tex2DOccludedNetDensityToAtmTopSRV", &m_ptex2DOccludedNetDensityToAtmTopSRV );
}



void LightSctrPostProcess :: CreateRandomSphereSamplingTexture(IRenderDevice *pDevice)
{
    TextureDesc RandomSphereSamplingTexDesc;
    RandomSphereSamplingTexDesc.Type = TEXTURE_TYPE_2D;
    RandomSphereSamplingTexDesc.Width = sm_iNumRandomSamplesOnSphere;
    RandomSphereSamplingTexDesc.Height = 1;
    RandomSphereSamplingTexDesc.MipLevels = 1;
    RandomSphereSamplingTexDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
    RandomSphereSamplingTexDesc.Usage = USAGE_STATIC;
    RandomSphereSamplingTexDesc.BindFlags = BIND_SHADER_RESOURCE;

    std::vector<float4> SphereSampling(sm_iNumRandomSamplesOnSphere);
    for(int iSample = 0; iSample < sm_iNumRandomSamplesOnSphere; ++iSample)
    {
        float4 &f4Sample = SphereSampling[iSample];
        f4Sample.z = ((float)rand()/(float)RAND_MAX) * 2.f - 1.f;
        float t = ((float)rand()/(float)RAND_MAX) * 2.f * PI;
        float r = sqrt( std::max(1.f - f4Sample.z*f4Sample.z, 0.f) );
        f4Sample.x = r * cos(t);
        f4Sample.y = r * sin(t);
        f4Sample.w = 0;
    }
    TextureSubResData Mip0Data;
    Mip0Data.pData = SphereSampling.data();
    Mip0Data.Stride = sm_iNumRandomSamplesOnSphere*sizeof( float4 );

    TextureData TexData;
    TexData.NumSubresources = 1;
    TexData.pSubResources = &Mip0Data;

    Diligent::RefCntAutoPtr<ITexture> ptex2DSphereRandomSampling;
    pDevice->CreateTexture( RandomSphereSamplingTexDesc, TexData, &ptex2DSphereRandomSampling );
    m_ptex2DSphereRandomSamplingSRV = ptex2DSphereRandomSampling->GetDefaultView( TEXTURE_VIEW_SHADER_RESOURCE );
    m_pResMapping->AddResource( "g_tex2DSphereRandomSampling", m_ptex2DSphereRandomSamplingSRV, true );
}

void LightSctrPostProcess :: CreatePrecomputedScatteringLUT(IRenderDevice *pDevice, IDeviceContext *pContext)
{
    const int ThreadGroupSize = 16;
    if( !m_pPrecomputeSingleSctrCS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.AddShaderMacro( "THREAD_GROUP_SIZE", ThreadGroupSize );
        Macros.Finalize();
        CreateShader( pDevice, "Precomputation.fx", "PrecomputeSingleScatteringCS", SHADER_TYPE_COMPUTE, Macros, &m_pPrecomputeSingleSctrCS );
    }
    
    if( !m_pComputeSctrRadianceCS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.AddShaderMacro( "THREAD_GROUP_SIZE", ThreadGroupSize );
        Macros.AddShaderMacro( "NUM_RANDOM_SPHERE_SAMPLES", sm_iNumRandomSamplesOnSphere );
        Macros.Finalize();
        CreateShader( pDevice, "Precomputation.fx", "ComputeSctrRadianceCS", SHADER_TYPE_COMPUTE, Macros, &m_pComputeSctrRadianceCS );
    }

    if( !m_pComputeScatteringOrderCS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.AddShaderMacro( "THREAD_GROUP_SIZE", ThreadGroupSize );
        Macros.Finalize();
        CreateShader( pDevice, "Precomputation.fx", "ComputeScatteringOrderCS", SHADER_TYPE_COMPUTE, Macros, &m_pComputeScatteringOrderCS );
    }

    if( !m_pInitHighOrderScatteringCS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.AddShaderMacro( "THREAD_GROUP_SIZE", ThreadGroupSize );
        Macros.Finalize();
        CreateShader( pDevice, "Precomputation.fx", "InitHighOrderScatteringCS", SHADER_TYPE_COMPUTE, Macros, &m_pInitHighOrderScatteringCS );
    }

    if( !m_pUpdateHighOrderScatteringCS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.AddShaderMacro( "THREAD_GROUP_SIZE", ThreadGroupSize );
        Macros.Finalize();
        CreateShader( pDevice, "Precomputation.fx", "UpdateHighOrderScatteringCS", SHADER_TYPE_COMPUTE, Macros, &m_pUpdateHighOrderScatteringCS );
    }

    if( !m_pCombineScatteringOrdersCS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.AddShaderMacro( "THREAD_GROUP_SIZE", ThreadGroupSize );
        Macros.Finalize();
        CreateShader( pDevice, "Precomputation.fx", "CombineScatteringOrdersCS", SHADER_TYPE_COMPUTE, Macros, &m_pCombineScatteringOrdersCS );
    }

    if( !m_ptex2DSphereRandomSamplingSRV )
        CreateRandomSphereSamplingTexture(pDevice);

    m_ptex3DSingleScatteringSRV.Release();
    m_ptex3DHighOrderScatteringSRV.Release();
    m_ptex3DMultipleScatteringSRV.Release();

    TextureDesc PrecomputedSctrTexDesc;
    PrecomputedSctrTexDesc.Type = TEXTURE_TYPE_3D;
    PrecomputedSctrTexDesc.Width  = sm_iPrecomputedSctrUDim;
    PrecomputedSctrTexDesc.Height = sm_iPrecomputedSctrVDim;
    PrecomputedSctrTexDesc.Depth  = sm_iPrecomputedSctrWDim * sm_iPrecomputedSctrQDim;
    PrecomputedSctrTexDesc.MipLevels = 1;
    PrecomputedSctrTexDesc.Format = TEX_FORMAT_RGBA16_FLOAT;
    PrecomputedSctrTexDesc.Usage = USAGE_DEFAULT;
    PrecomputedSctrTexDesc.BindFlags = BIND_UNORDERED_ACCESS | BIND_SHADER_RESOURCE;

    Diligent::RefCntAutoPtr<ITexture> ptex3DSingleSctr, ptex3DHighOrderSctr, ptex3DHighOrderSctr2, ptex3DMultipleSctr;
    pDevice->CreateTexture(PrecomputedSctrTexDesc, TextureData(), &ptex3DSingleSctr);
    m_ptex3DSingleScatteringSRV = ptex3DSingleSctr->GetDefaultView( TEXTURE_VIEW_SHADER_RESOURCE );
    m_ptex3DSingleScatteringSRV->SetSampler( m_pLinearClampSampler );
    m_pResMapping->AddResource( "g_rwtex3DSingleScattering", ptex3DSingleSctr->GetDefaultView( TEXTURE_VIEW_UNORDERED_ACCESS ), true );

    // We have to bother with two texture, because HLSL only allows read-write operations on single
    // component textures
    pDevice->CreateTexture(PrecomputedSctrTexDesc, TextureData(), &ptex3DHighOrderSctr);
    pDevice->CreateTexture(PrecomputedSctrTexDesc, TextureData(), &ptex3DHighOrderSctr2);
    ptex3DHighOrderSctr->GetDefaultView( TEXTURE_VIEW_SHADER_RESOURCE )->SetSampler( m_pLinearClampSampler );
    ptex3DHighOrderSctr2->GetDefaultView( TEXTURE_VIEW_SHADER_RESOURCE )->SetSampler( m_pLinearClampSampler );

   
    pDevice->CreateTexture(PrecomputedSctrTexDesc, TextureData(), &ptex3DMultipleSctr);
    m_ptex3DMultipleScatteringSRV = ptex3DMultipleSctr->GetDefaultView( TEXTURE_VIEW_SHADER_RESOURCE );
    m_ptex3DMultipleScatteringSRV->SetSampler( m_pLinearClampSampler );
    m_pResMapping->AddResource( "g_rwtex3DMultipleSctr", ptex3DMultipleSctr->GetDefaultView( TEXTURE_VIEW_UNORDERED_ACCESS ), true );

    // Precompute single scattering
    m_pPrecomputeSingleSctrCS->BindResources( m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED );
    DispatchComputeAttribs DispatchAttrs( PrecomputedSctrTexDesc.Width/ThreadGroupSize,
                                           PrecomputedSctrTexDesc.Height/ThreadGroupSize,
                                           PrecomputedSctrTexDesc.Depth);
    IShader *pCS = m_pPrecomputeSingleSctrCS;
    pContext->SetShaders(&pCS, 1);
    pContext->DispatchCompute(DispatchAttrs);


    // Precompute multiple scattering
    // We need higher precision to store intermediate data
    PrecomputedSctrTexDesc.Format = TEX_FORMAT_RGBA32_FLOAT;
    Diligent::RefCntAutoPtr<ITexture> ptex3DSctrRadiance, ptex3DInsctrOrder;
    Diligent::RefCntAutoPtr<ITextureView> ptex3DSctrRadianceSRV, ptex3DInsctrOrderSRV;
    pDevice->CreateTexture(PrecomputedSctrTexDesc, TextureData(), &ptex3DSctrRadiance);
    pDevice->CreateTexture(PrecomputedSctrTexDesc, TextureData(), &ptex3DInsctrOrder);
    ptex3DSctrRadianceSRV = ptex3DSctrRadiance->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    ptex3DInsctrOrderSRV = ptex3DInsctrOrder->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    ptex3DSctrRadianceSRV->SetSampler( m_pLinearClampSampler );
    ptex3DInsctrOrderSRV->SetSampler( m_pLinearClampSampler );
    m_pResMapping->AddResource( "g_rwtex3DSctrRadiance", ptex3DSctrRadiance->GetDefaultView( TEXTURE_VIEW_UNORDERED_ACCESS ), true );
    m_pResMapping->AddResource( "g_rwtex3DInsctrOrder", ptex3DInsctrOrder->GetDefaultView( TEXTURE_VIEW_UNORDERED_ACCESS ), true );

    m_pComputeSctrRadianceCS->BindResources( m_pResMapping, 0 );
    m_pComputeScatteringOrderCS->BindResources( m_pResMapping, 0 );
    m_pInitHighOrderScatteringCS->BindResources( m_pResMapping, 0 );
    m_pUpdateHighOrderScatteringCS->BindResources( m_pResMapping, 0 );

    const int iNumScatteringOrders = 3;
    for(int iSctrOrder = 1; iSctrOrder < iNumScatteringOrders; ++iSctrOrder)
    {
        // Step 1: compute differential in-scattering
        IShader *pCS = m_pComputeSctrRadianceCS;
        m_pComputeSctrRadianceCS->GetShaderVariable( "g_tex3DPreviousSctrOrder" )->Set( (iSctrOrder == 1) ? m_ptex3DSingleScatteringSRV : ptex3DInsctrOrderSRV );
        pContext->SetShaders(&pCS, 1);
        pContext->DispatchCompute(DispatchAttrs);

        // Step 2: integrate differential in-scattering
        m_pComputeScatteringOrderCS->GetShaderVariable( "g_tex3DPointwiseSctrRadiance" )->Set( ptex3DSctrRadianceSRV );
        pCS = m_pComputeScatteringOrderCS;
        pContext->SetShaders(&pCS, 1);
        pContext->DispatchCompute(DispatchAttrs);

        // Step 3: accumulate high-order scattering scattering
        if( iSctrOrder == 1 )
        {
            pCS = m_pInitHighOrderScatteringCS;
        }
        else
        {
            std::swap( ptex3DHighOrderSctr, ptex3DHighOrderSctr2 );
            m_pUpdateHighOrderScatteringCS->GetShaderVariable( "g_tex3DHighOrderOrderScattering" )->Set( ptex3DHighOrderSctr2->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE) );
            pCS = m_pUpdateHighOrderScatteringCS;
        }
        pCS->GetShaderVariable( "g_rwtex3DHighOrderSctr" )->Set( ptex3DHighOrderSctr->GetDefaultView( TEXTURE_VIEW_UNORDERED_ACCESS ) );
        pCS->GetShaderVariable( "g_tex3DCurrentOrderScattering" )->Set( ptex3DInsctrOrderSRV );
        pContext->SetShaders(&pCS, 1);
        pContext->DispatchCompute(DispatchAttrs);
        
        // It seemse like on Intel GPU, the driver accumulates work into big batch. 
        // The resulting batch turns out to be too big for GPU to process it in allowed time
        // limit, and the system kills the driver. So we have to flush the command buffer to
        // force execution of compute shaders.
        pContext->Flush();
    }
    

    m_pResMapping->AddResource("g_tex3DSingleSctrLUT", m_ptex3DSingleScatteringSRV, false);

    m_ptex3DHighOrderScatteringSRV = ptex3DHighOrderSctr->GetDefaultView( TEXTURE_VIEW_SHADER_RESOURCE );
    m_ptex3DHighOrderScatteringSRV->SetSampler( m_pLinearClampSampler );
    m_pResMapping->AddResource("g_tex3DHighOrderSctrLUT", m_ptex3DHighOrderScatteringSRV, false);

    m_pCombineScatteringOrdersCS->BindResources(  m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED );
    // Combine single scattering and higher order scattering into single texture
    pCS = m_pCombineScatteringOrdersCS;
    pContext->SetShaders(&pCS, 1);
    pContext->DispatchCompute(DispatchAttrs);

    m_pResMapping->AddResource("g_tex3DMultipleSctrLUT", m_ptex3DMultipleScatteringSRV, false);

    m_pResMapping->RemoveResourceByName( "g_rwtex3DMultipleSctr" );
    m_pResMapping->RemoveResourceByName( "g_rwtex3DSingleScattering" );
    m_pResMapping->RemoveResourceByName( "g_rwtex3DSctrRadiance" );
    m_pResMapping->RemoveResourceByName( "g_rwtex3DSctrOrder" );
}


void LightSctrPostProcess :: ReconstructCameraSpaceZ(FrameAttribs &FrameAttribs)
{
    // Depth buffer is non-linear and cannot be interpolated directly
    // We have to reconstruct camera space z to be able to use bilinear filtering

    if( !m_pReconstrCamSpaceZPS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.Finalize();

        CreateShader( FrameAttribs.pDevice, "ReconstructCameraSpaceZ.fx", "ReconstructCameraSpaceZPS", SHADER_TYPE_PIXEL, Macros, &m_pReconstrCamSpaceZPS );
    }

    // Bind input resources required by the shader
    m_pReconstrCamSpaceZPS->BindResources( m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED );

    m_pRenderScript->Run(FrameAttribs.pDeviceContext, "ReconstructCameraSpaceZ", m_pReconstrCamSpaceZPS );
}

void LightSctrPostProcess :: RenderSliceEndpoints(FrameAttribs &FrameAttribs)
{
    if( !m_pRendedSliceEndpointsPS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.Finalize();
        CreateShader( FrameAttribs.pDevice, "RenderSliceEndPoints.fx", "GenerateSliceEndpointsPS", SHADER_TYPE_PIXEL, Macros, &m_pRendedSliceEndpointsPS );
    }
    // Bind input resources required by the shader
    m_pRendedSliceEndpointsPS->BindResources( m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED );
    m_pRenderScript->Run(FrameAttribs.pDeviceContext, "RenderSliceEndPoints", m_pRendedSliceEndpointsPS );
}

void LightSctrPostProcess :: RenderCoordinateTexture(FrameAttribs &FrameAttribs)
{
    if( !m_pRendedCoordTexPS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.Finalize();
        CreateShader( FrameAttribs.pDevice, "RenderCoordinateTexture.fx", "GenerateCoordinateTexturePS", SHADER_TYPE_PIXEL, Macros, &m_pRendedCoordTexPS );
    }
    m_pRendedCoordTexPS->BindResources( m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED );
    m_pRenderScript->Run(FrameAttribs.pDeviceContext, "RenderCoordinateTexture", m_pRendedCoordTexPS );
}

void LightSctrPostProcess :: RenderCoarseUnshadowedInctr(FrameAttribs &FrameAttribs)
{
    if( !m_pRenderCoarseUnshadowedInsctrPS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.Finalize();
        auto EntryPoint = 
            m_PostProcessingAttribs.m_uiExtinctionEvalMode == EXTINCTION_EVAL_MODE_EPIPOLAR ? 
                "RenderCoarseUnshadowedInsctrAndExtinctionPS" : 
                "RenderCoarseUnshadowedInsctrPS";
        CreateShader( FrameAttribs.pDevice, "CoarseInsctr.fx", EntryPoint, SHADER_TYPE_PIXEL, Macros, &m_pRenderCoarseUnshadowedInsctrPS);
    }

    if( m_PostProcessingAttribs.m_uiExtinctionEvalMode == EXTINCTION_EVAL_MODE_EPIPOLAR && 
        !(m_uiUpToDateResourceFlags & UpToDateResourceFlags::ExtinctionTexture) )
    {
        m_pRenderScript->Run(FrameAttribs.pDeviceContext, "CreateExtinctionTexture", m_PostProcessingAttribs.m_uiNumEpipolarSlices, m_PostProcessingAttribs.m_uiMaxSamplesInSlice );
        m_uiUpToDateResourceFlags |= UpToDateResourceFlags::ExtinctionTexture;
        m_ptex2DEpipolarExtinctionRTV.Release();
        m_pRenderScript->GetTextureViewByName( "tex2DEpipolarExtinctionRTV", &m_ptex2DEpipolarExtinctionRTV );
    }

    ITextureView *pRTVs[] = {m_ptex2DEpipolarInscatteringRTV, m_ptex2DEpipolarExtinctionRTV};
    FrameAttribs.pDeviceContext->SetRenderTargets(_countof(pRTVs),pRTVs, m_ptex2DEpipolarImageDSV);

    float flt16max = 65504.f; // Epipolar Inscattering is 16-bit float
    const float InvalidInsctr[] = {-flt16max, -flt16max, -flt16max, -flt16max};
    if( m_ptex2DEpipolarInscatteringRTV )
        FrameAttribs.pDeviceContext->ClearRenderTarget(m_ptex2DEpipolarInscatteringRTV, InvalidInsctr);
    const float One[] = {1, 1, 1, 1};
    if( m_ptex2DEpipolarExtinctionRTV )
        FrameAttribs.pDeviceContext->ClearRenderTarget(m_ptex2DEpipolarExtinctionRTV, One);

    m_pRenderCoarseUnshadowedInsctrPS->BindResources( m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED );
    
    m_pRenderScript->Run(FrameAttribs.pDeviceContext, "RenderCoarseUnshadowedInctr", m_pRenderCoarseUnshadowedInsctrPS );
}

void LightSctrPostProcess :: RefineSampleLocations(FrameAttribs &FrameAttribs)
{
    if( !m_pRefineSampleLocationsCS )
    {
        // Thread group size must be at least as large as initial sample step
        m_uiSampleRefinementCSThreadGroupSize = std::max( m_uiSampleRefinementCSMinimumThreadGroupSize, m_PostProcessingAttribs.m_uiInitialSampleStepInSlice );
        // Thread group size cannot be larger than the total number of samples in slice
        m_uiSampleRefinementCSThreadGroupSize = std::min( m_uiSampleRefinementCSThreadGroupSize, m_PostProcessingAttribs.m_uiMaxSamplesInSlice );
        // Using small group size is inefficient since a lot of SIMD lanes become idle

        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.AddShaderMacro("INITIAL_SAMPLE_STEP", m_PostProcessingAttribs.m_uiInitialSampleStepInSlice);
        Macros.AddShaderMacro("THREAD_GROUP_SIZE"  , m_uiSampleRefinementCSThreadGroupSize );
        Macros.AddShaderMacro("REFINEMENT_CRITERION", m_PostProcessingAttribs.m_uiRefinementCriterion );
        Macros.AddShaderMacro("AUTO_EXPOSURE",        m_PostProcessingAttribs.m_bAutoExposure);
        Macros.Finalize();
        CreateShader( FrameAttribs.pDevice, "RefineSampleLocations.fx", "RefineSampleLocationsCS", SHADER_TYPE_COMPUTE, Macros, &m_pRefineSampleLocationsCS);
    }
    m_pRefineSampleLocationsCS->BindResources( m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED );
    
    DispatchComputeAttribs DispatchAttrs( m_PostProcessingAttribs.m_uiMaxSamplesInSlice/m_uiSampleRefinementCSThreadGroupSize,
                                           m_PostProcessingAttribs.m_uiNumEpipolarSlices,
                                           1);
    IShader *pCS = m_pRefineSampleLocationsCS;
    FrameAttribs.pDeviceContext->SetShaders(&pCS, 1);
    FrameAttribs.pDeviceContext->DispatchCompute(DispatchAttrs);
}

void LightSctrPostProcess :: MarkRayMarchingSamples(FrameAttribs &FrameAttribs)
{
    if( !m_pMarkRayMarchingSamplesInStencilPS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.Finalize();
        CreateShader( FrameAttribs.pDevice, "MarkRayMarchingSamples.fx", "MarkRayMarchingSamplesInStencilPS", SHADER_TYPE_PIXEL, Macros, &m_pMarkRayMarchingSamplesInStencilPS );
    }
    m_pMarkRayMarchingSamplesInStencilPS->BindResources(m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED);

    // Mark ray marching samples in the stencil
    // The depth stencil state is configured to pass only pixels, whose stencil value equals 1. Thus all epipolar samples with 
    // coordinates outsied the screen (generated on the previous pass) are automatically discarded. The pixel shader only
    // passes samples which are interpolated from themselves, the rest are discarded. Thus after this pass all ray
    // marching samples will be marked with 2 in stencil
    m_pRenderScript->Run(FrameAttribs.pDeviceContext, "MarkRayMarchingSamples", m_pMarkRayMarchingSamplesInStencilPS );
}

void LightSctrPostProcess :: RenderSliceUVDirAndOrig(FrameAttribs &FrameAttribs)
{
    if( !m_pRenderSliceUVDirInSMPS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.Finalize();

        CreateShader( FrameAttribs.pDevice, "MinMaxBinTree.fx", "RenderSliceUVDirInShadowMapTexturePS", SHADER_TYPE_PIXEL, Macros, &m_pRenderSliceUVDirInSMPS );
    }
    m_pRenderSliceUVDirInSMPS->BindResources(m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED);
    if( !(m_uiUpToDateResourceFlags & UpToDateResourceFlags::SliceUVDirAndOriginTex) )
    {
        m_pRenderScript->Run( "CreateSliceUVDirAndOriginTexture", m_PostProcessingAttribs.m_uiNumEpipolarSlices, m_PostProcessingAttribs.m_iNumCascades );
        m_uiUpToDateResourceFlags |= UpToDateResourceFlags::SliceUVDirAndOriginTex;
    }

    m_pRenderScript->Run( FrameAttribs.pDeviceContext, "RenderSliceUVDirAndOrigin", m_pRenderSliceUVDirInSMPS );
}

void LightSctrPostProcess :: Build1DMinMaxMipMap(FrameAttribs &FrameAttribs, 
                                                  int iCascadeIndex)
{
    if( !m_pInitializeMinMaxShadowMapPS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.AddShaderMacro("IS_32BIT_MIN_MAX_MAP", m_PostProcessingAttribs.m_bIs32BitMinMaxMipMap);
        Macros.Finalize();

        CreateShader( FrameAttribs.pDevice, "MinMaxBinTree.fx", "InitializeMinMaxShadowMapPS", SHADER_TYPE_PIXEL, Macros, &m_pInitializeMinMaxShadowMapPS );
    }

    if( !m_pComputeMinMaxSMLevelPS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.Finalize();

        CreateShader( FrameAttribs.pDevice, "MinMaxBinTree.fx", "ComputeMinMaxShadowMapLevelPS", SHADER_TYPE_PIXEL, Macros, &m_pComputeMinMaxSMLevelPS );
    }
        
    auto pShadowSampler = FrameAttribs.ptex2DShadowMapSRV->GetSampler();
    FrameAttribs.ptex2DShadowMapSRV->SetSampler( m_pLinearClampSampler );

    int iMinMaxTexHeight = m_PostProcessingAttribs.m_uiNumEpipolarSlices;
    if( m_bUseCombinedMinMaxTexture )
        iMinMaxTexHeight *= (m_PostProcessingAttribs.m_iNumCascades - m_PostProcessingAttribs.m_iFirstCascade);

    auto tex2DMinMaxShadowMap0 = m_ptex2DMinMaxShadowMapRTV[0]->GetTexture();
    auto tex2DMinMaxShadowMap1 = m_ptex2DMinMaxShadowMapRTV[1]->GetTexture();

    // Computing min/max mip map using compute shader is much slower because a lot of threads are idle
    Uint32 uiXOffset = 0;
    Uint32 uiPrevXOffset = 0;
    Uint32 uiParity = 0;
#ifdef _DEBUG
    {
        const auto &MinMaxShadowMapTexDesc = m_ptex2DMinMaxShadowMapRTV[0]->GetTexture()->GetDesc();
        assert( MinMaxShadowMapTexDesc.Width == m_PostProcessingAttribs.m_uiMinMaxShadowMapResolution );
        assert( MinMaxShadowMapTexDesc.Height == iMinMaxTexHeight );
    }
#endif
    // Note that we start rendering min/max shadow map from step == 2
    for(Uint32 iStep = 2; iStep <= (Uint32)m_PostProcessingAttribs.m_fMaxShadowMapStep; iStep *=2, uiParity = (uiParity+1)%2 )
    {
        // Use two buffers which are in turn used as the source and destination
        ITextureView *pRTVs[] = {m_ptex2DMinMaxShadowMapRTV[uiParity]};
        FrameAttribs.pDeviceContext->SetRenderTargets( _countof( pRTVs ), pRTVs, nullptr );

        Viewport VP;
        VP.Width = static_cast<float>( m_PostProcessingAttribs.m_uiMinMaxShadowMapResolution / iStep );
        VP.Height = static_cast<float>( iMinMaxTexHeight );
        VP.TopLeftX = static_cast<float>( uiXOffset );
        VP.TopLeftY = 0;
        FrameAttribs.pDeviceContext->SetViewports(1, &VP, 0, 0 );

        // Set source and destination min/max data offsets:
        {
            MapHelper<MiscDynamicParams> pMiscDynamicParams( FrameAttribs.pDeviceContext, m_pcbMiscParams, MAP_WRITE_DISCARD, 0 );
            pMiscDynamicParams->ui4SrcMinMaxLevelXOffset = uiPrevXOffset;
            pMiscDynamicParams->ui4DstMinMaxLevelXOffset = uiXOffset;
            pMiscDynamicParams->fCascadeInd = static_cast<float>(iCascadeIndex);
        }

        if( iStep == 2 )
        {
            // At the initial pass, the shader gathers 8 depths which will be used for
            // PCF filtering at the sample location and its next neighbor along the slice 
            // and outputs min/max depths
            m_pInitializeMinMaxShadowMapPS->BindResources(m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED);
            m_pRenderScript->Run( FrameAttribs.pDeviceContext, "InitMinMaxShadowMap", m_pInitializeMinMaxShadowMapPS );
        }
        else
        {
            // At the subsequent passes, the shader loads two min/max values from the next finer level 
            // to compute next level of the binary tree

            m_pResMapping->AddResource( "g_tex2DMinMaxLightSpaceDepth", m_ptex2DMinMaxShadowMapSRV[(uiParity + 1) % 2], false );
            m_pComputeMinMaxSMLevelPS->BindResources(m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED);
            m_pRenderScript->Run( FrameAttribs.pDeviceContext, "ComputeMinMaxShadowMapLevel", m_pComputeMinMaxSMLevelPS );
        }
     

        // All the data must reside in 0-th texture, so copy current level, if necessary, from 1-st texture
        if( uiParity == 1 )
        {
            Box SrcBox;
            SrcBox.MinX = uiXOffset;
            SrcBox.MaxX = uiXOffset + m_PostProcessingAttribs.m_uiMinMaxShadowMapResolution / iStep;
            SrcBox.MinY = 0;
            SrcBox.MaxY = iMinMaxTexHeight;
            
            tex2DMinMaxShadowMap0->CopyData( FrameAttribs.pDeviceContext, tex2DMinMaxShadowMap1, 0, 0, &SrcBox, 0, 0, uiXOffset, 0, 0);
        }

        uiPrevXOffset = uiXOffset;
        uiXOffset += m_PostProcessingAttribs.m_uiMinMaxShadowMapResolution / iStep;
    }
    m_pResMapping->AddResource( "g_tex2DMinMaxLightSpaceDepth", m_ptex2DMinMaxShadowMapSRV[0], false );
    FrameAttribs.ptex2DShadowMapSRV->SetSampler( pShadowSampler );
}

void LightSctrPostProcess :: DoRayMarching(FrameAttribs &FrameAttribs, 
                                            Uint32 uiMaxStepsAlongRay, 
                                            int iCascadeIndex)
{
    auto &pDoRayMarchPS = m_pDoRayMarchPS[m_PostProcessingAttribs.m_bUse1DMinMaxTree ? 1 : 0];
    if( !pDoRayMarchPS  )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.AddShaderMacro("CASCADE_PROCESSING_MODE", m_PostProcessingAttribs.m_uiCascadeProcessingMode);
        Macros.AddShaderMacro("USE_1D_MIN_MAX_TREE", m_PostProcessingAttribs.m_bUse1DMinMaxTree);
        Macros.Finalize();

        CreateShader( FrameAttribs.pDevice, "RayMarch.fx", "RayMarchPS", SHADER_TYPE_PIXEL, Macros, &pDoRayMarchPS );
    }

    {
        MapHelper<MiscDynamicParams> pMiscDynamicParams( FrameAttribs.pDeviceContext, m_pcbMiscParams, MAP_WRITE_DISCARD, 0 );
        pMiscDynamicParams->fMaxStepsAlongRay = static_cast<float>( uiMaxStepsAlongRay );
        pMiscDynamicParams->fCascadeInd = static_cast<float>(iCascadeIndex);
    }

    int iNumInst = 0;
    if( m_PostProcessingAttribs.m_bEnableLightShafts )
    {
        switch(m_PostProcessingAttribs.m_uiCascadeProcessingMode)
        {
            case CASCADE_PROCESSING_MODE_SINGLE_PASS:
            case CASCADE_PROCESSING_MODE_MULTI_PASS: 
                iNumInst = 1; 
                break;
            case CASCADE_PROCESSING_MODE_MULTI_PASS_INST: 
                iNumInst = m_PostProcessingAttribs.m_iNumCascades - m_PostProcessingAttribs.m_iFirstCascade; 
                break;
        }
    }
    else
    {
        iNumInst = 1;
    }

    pDoRayMarchPS->BindResources(m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED);

    // Depth stencil view now contains 2 for these pixels, for which ray marchings is to be performed
    // Depth stencil state is configured to pass only these pixels and discard the rest
    m_pRenderScript->Run( FrameAttribs.pDeviceContext, "RayMarch", pDoRayMarchPS, iNumInst );
}

void LightSctrPostProcess :: InterpolateInsctrIrradiance(FrameAttribs &FrameAttribs)
{
    if( !m_pInterpolateIrradiancePS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.Finalize();

        CreateShader( FrameAttribs.pDevice, "InterpolateIrradiance.fx", "InterpolateIrradiancePS", SHADER_TYPE_PIXEL, Macros, &m_pInterpolateIrradiancePS );
    }
    m_pInterpolateIrradiancePS->BindResources(m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED);
    m_pRenderScript->Run( FrameAttribs.pDeviceContext, "InterpolateIrradiance", m_pInterpolateIrradiancePS );
}


void LightSctrPostProcess :: UnwarpEpipolarScattering(FrameAttribs &FrameAttribs, bool bRenderLuminance)
{
    if( !m_pUnwarpEpipolarSctrImgPS  )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.AddShaderMacro("PERFORM_TONE_MAPPING", true);
        Macros.AddShaderMacro("AUTO_EXPOSURE", m_PostProcessingAttribs.m_bAutoExposure);
        Macros.AddShaderMacro("TONE_MAPPING_MODE", m_PostProcessingAttribs.m_uiToneMappingMode);
        Macros.AddShaderMacro("CORRECT_INSCATTERING_AT_DEPTH_BREAKS", m_PostProcessingAttribs.m_bCorrectScatteringAtDepthBreaks);
        Macros.Finalize();
        
        CreateShader( FrameAttribs.pDevice, "UnwarpEpipolarScattering.fx", "ApplyInscatteredRadiancePS", SHADER_TYPE_PIXEL, Macros, &m_pUnwarpEpipolarSctrImgPS );
    }

    if( !m_pUnwarpAndRenderLuminancePS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.AddShaderMacro("PERFORM_TONE_MAPPING", false);
        // No inscattering correction - we need to render the entire image in low resolution
        Macros.AddShaderMacro("CORRECT_INSCATTERING_AT_DEPTH_BREAKS", false);
        Macros.Finalize();
        
        CreateShader( FrameAttribs.pDevice, "UnwarpEpipolarScattering.fx", "ApplyInscatteredRadiancePS", SHADER_TYPE_PIXEL, Macros, &m_pUnwarpAndRenderLuminancePS );
    }

    FrameAttribs.ptex2DSrcColorBufferSRV->SetSampler(m_pPointClampSampler);
    auto &pPS = bRenderLuminance ? m_pUnwarpAndRenderLuminancePS : m_pUnwarpEpipolarSctrImgPS;
    pPS->BindResources(m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED);

    // Unwarp inscattering image and apply it to attenuated backgorund
    m_pRenderScript->Run( FrameAttribs.pDeviceContext, bRenderLuminance ? "UnwarpAndRenderLuminance" : "UnwarpEpipolarScattering", pPS );
}

void LightSctrPostProcess :: UpdateAverageLuminance(FrameAttribs &FrameAttribs)
{
    if( !m_pUpdateAverageLuminancePS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.AddShaderMacro( "LIGHT_ADAPTATION", m_PostProcessingAttribs.m_bLightAdaptation );
        // static_cast<int>() is required because Run() gets its arguments by reference
        // and gcc will try to find reference to sm_iLowResLuminanceMips, which does not exist
        Macros.AddShaderMacro("LOW_RES_LUMINANCE_MIPS", static_cast<int>(sm_iLowResLuminanceMips) );
        Macros.Finalize();

        CreateShader( FrameAttribs.pDevice, "UpdateAverageLuminance.fx", "UpdateAverageLuminancePS", SHADER_TYPE_PIXEL, Macros, &m_pUpdateAverageLuminancePS );
    }
    m_pUpdateAverageLuminancePS->BindResources(m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED);

    {
        MapHelper<MiscDynamicParams> pMiscDynamicParams( FrameAttribs.pDeviceContext, m_pcbMiscParams, MAP_WRITE_DISCARD, 0 );
        pMiscDynamicParams->fElapsedTime = (float)FrameAttribs.dElapsedTime;
    }
    m_pRenderScript->Run( FrameAttribs.pDeviceContext, "UpdateAverageLuminance", m_pUpdateAverageLuminancePS );
}


void LightSctrPostProcess :: FixInscatteringAtDepthBreaks(FrameAttribs &FrameAttribs, 
                                                           Uint32 uiMaxStepsAlongRay, 
                                                           EFixInscatteringMode Mode)
{
    bool bRenderLuminance = Mode == EFixInscatteringMode::LuminanceOnly;
    auto &pFixInsctrAtDepthBreaksPS = m_pFixInsctrAtDepthBreaksPS[ bRenderLuminance ? 1 : 0];

    if( !pFixInsctrAtDepthBreaksPS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.AddShaderMacro("CASCADE_PROCESSING_MODE", CASCADE_PROCESSING_MODE_SINGLE_PASS);
        Macros.AddShaderMacro("PERFORM_TONE_MAPPING", !bRenderLuminance);
        Macros.AddShaderMacro("AUTO_EXPOSURE", m_PostProcessingAttribs.m_bAutoExposure);
        Macros.AddShaderMacro("TONE_MAPPING_MODE", m_PostProcessingAttribs.m_uiToneMappingMode);
        Macros.AddShaderMacro("USE_1D_MIN_MAX_TREE", false);
        Macros.Finalize();

        CreateShader( FrameAttribs.pDevice, "RayMarch.fx", "FixAndApplyInscatteredRadiancePS", SHADER_TYPE_PIXEL, Macros, &pFixInsctrAtDepthBreaksPS );
    }
    
    {
        MapHelper<MiscDynamicParams> pMiscDynamicParams( FrameAttribs.pDeviceContext, m_pcbMiscParams, MAP_WRITE_DISCARD, 0 );
        pMiscDynamicParams->fMaxStepsAlongRay = static_cast<float>(uiMaxStepsAlongRay);
        pMiscDynamicParams->fCascadeInd = static_cast<float>(m_PostProcessingAttribs.m_iFirstCascade);
    }

    pFixInsctrAtDepthBreaksPS->BindResources(m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED);

    m_pRenderScript->Run( FrameAttribs.pDeviceContext, "FixInscatteringAtDepthBreaks", pFixInsctrAtDepthBreaksPS, static_cast<Int32>(Mode) );
}

void LightSctrPostProcess :: RenderSampleLocations(FrameAttribs &FrameAttribs)
{
    if( !m_pRenderSampleLocationsPS )
    {
        ShaderMacroHelper Macros;
        DefineMacros(Macros);
        Macros.Finalize();

        CreateShader( FrameAttribs.pDevice, "RenderSampling.fx", "RenderSampleLocationsVS", SHADER_TYPE_VERTEX, Macros, &m_pRenderSampleLocationsVS );
        CreateShader( FrameAttribs.pDevice, "RenderSampling.fx", "RenderSampleLocationsPS", SHADER_TYPE_PIXEL, Macros, &m_pRenderSampleLocationsPS );
    }
    m_pRenderSampleLocationsVS->BindResources(m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED);
    m_pRenderSampleLocationsPS->BindResources(m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED);

    m_pRenderScript->Run( FrameAttribs.pDeviceContext, "RenderSampleLocations", m_pRenderSampleLocationsVS, m_pRenderSampleLocationsPS, m_PostProcessingAttribs.m_uiMaxSamplesInSlice * m_PostProcessingAttribs.m_uiNumEpipolarSlices );
}


void LightSctrPostProcess :: PerformPostProcessing(FrameAttribs &FrameAttribs,
                                                    PostProcessingAttribs &PPAttribs)
{
#if 0
    if( GetAsyncKeyState(VK_F8) )
    {
        m_ReconstrCamSpaceZTech.Release();
        m_RendedSliceEndpointsTech.Release();
        m_RendedCoordTexTech.Release();
        m_RefineSampleLocationsTech.Release();
        m_RenderCoarseUnshadowedInsctrTech.Release();
        m_MarkRayMarchingSamplesInStencilTech.Release();
        m_RenderSliceUVDirInSMTech.Release();
        m_InitializeMinMaxShadowMapTech.Release();
        m_ComputeMinMaxSMLevelTech.Release();
        for(size_t i=0; i<_countof(m_DoRayMarchTech); ++i)
            m_DoRayMarchTech[i].Release();
        m_InterpolateIrradianceTech.Release();
        m_UnwarpEpipolarSctrImgTech.Release();
        m_UnwarpAndRenderLuminanceTech.Release();
        m_UpdateAverageLuminanceTech.Release();
        for(size_t i=0; i<_countof(m_FixInsctrAtDepthBreaksTech); ++i)
            m_FixInsctrAtDepthBreaksTech[i].Release();
        m_RenderSampleLocationsTech.Release();
        m_RenderSunTech.Release();
        m_PrecomputeSingleSctrTech.Release();
        m_ComputeSctrRadianceTech.Release();
        m_ComputeScatteringOrderTech.Release();
        m_AddScatteringOrderTech.Release();
    }
#endif

    bool bUseCombinedMinMaxTexture = PPAttribs.m_uiCascadeProcessingMode == CASCADE_PROCESSING_MODE_SINGLE_PASS ||
                                     PPAttribs.m_uiCascadeProcessingMode == CASCADE_PROCESSING_MODE_MULTI_PASS_INST ||
                                     PPAttribs.m_bCorrectScatteringAtDepthBreaks || 
                                     PPAttribs.m_uiLightSctrTechnique == LIGHT_SCTR_TECHNIQUE_BRUTE_FORCE;


    if( PPAttribs.m_uiNumEpipolarSlices != m_PostProcessingAttribs.m_uiNumEpipolarSlices || 
        PPAttribs.m_uiMaxSamplesInSlice != m_PostProcessingAttribs.m_uiMaxSamplesInSlice ||
        PPAttribs.m_bOptimizeSampleLocations != m_PostProcessingAttribs.m_bOptimizeSampleLocations )
        m_pRendedSliceEndpointsPS.Release();
    
    if( PPAttribs.m_uiMaxSamplesInSlice != m_PostProcessingAttribs.m_uiMaxSamplesInSlice )
        m_pRendedCoordTexPS.Release();

    if( PPAttribs.m_uiMaxSamplesInSlice != m_PostProcessingAttribs.m_uiMaxSamplesInSlice ||
        PPAttribs.m_uiInitialSampleStepInSlice != m_PostProcessingAttribs.m_uiInitialSampleStepInSlice ||
        PPAttribs.m_uiRefinementCriterion != m_PostProcessingAttribs.m_uiRefinementCriterion ||
        PPAttribs.m_bAutoExposure != m_PostProcessingAttribs.m_bAutoExposure )
        m_pRefineSampleLocationsCS.Release();

    if( PPAttribs.m_bUse1DMinMaxTree != m_PostProcessingAttribs.m_bUse1DMinMaxTree ||
        bUseCombinedMinMaxTexture != m_bUseCombinedMinMaxTexture ||
        PPAttribs.m_uiNumEpipolarSlices != m_PostProcessingAttribs.m_uiNumEpipolarSlices ||
        PPAttribs.m_bIs32BitMinMaxMipMap != m_PostProcessingAttribs.m_bIs32BitMinMaxMipMap )
    {
        m_pInitializeMinMaxShadowMapPS.Release();
    }

    if( PPAttribs.m_bUse1DMinMaxTree != m_PostProcessingAttribs.m_bUse1DMinMaxTree ||
        PPAttribs.m_uiCascadeProcessingMode != m_PostProcessingAttribs.m_uiCascadeProcessingMode ||
        bUseCombinedMinMaxTexture != m_bUseCombinedMinMaxTexture ||
        PPAttribs.m_bEnableLightShafts != m_PostProcessingAttribs.m_bEnableLightShafts ||
        PPAttribs.m_uiMultipleScatteringMode != m_PostProcessingAttribs.m_uiMultipleScatteringMode ||
        PPAttribs.m_uiSingleScatteringMode != m_PostProcessingAttribs.m_uiSingleScatteringMode)
    {
        for(int i=0; i<_countof(m_pDoRayMarchPS); ++i)
            m_pDoRayMarchPS[i].Release();
    }

    if( PPAttribs.m_uiNumEpipolarSlices != m_PostProcessingAttribs.m_uiNumEpipolarSlices ||
        PPAttribs.m_uiMaxSamplesInSlice != m_PostProcessingAttribs.m_uiMaxSamplesInSlice )
    {
        m_pUnwarpEpipolarSctrImgPS.Release();
        m_pUnwarpAndRenderLuminancePS.Release();
    }

    if( PPAttribs.m_bAutoExposure != m_PostProcessingAttribs.m_bAutoExposure || 
        PPAttribs.m_uiToneMappingMode != m_PostProcessingAttribs.m_uiToneMappingMode ||
        PPAttribs.m_bCorrectScatteringAtDepthBreaks != m_PostProcessingAttribs.m_bCorrectScatteringAtDepthBreaks )
    {
        m_pUnwarpEpipolarSctrImgPS.Release();
    }

    if( PPAttribs.m_bLightAdaptation != m_PostProcessingAttribs.m_bLightAdaptation )
    {
        m_pUpdateAverageLuminancePS.Release();
    }

    if( PPAttribs.m_uiCascadeProcessingMode != m_PostProcessingAttribs.m_uiCascadeProcessingMode ||
        bUseCombinedMinMaxTexture != m_bUseCombinedMinMaxTexture ||
        PPAttribs.m_bEnableLightShafts != m_PostProcessingAttribs.m_bEnableLightShafts ||
        PPAttribs.m_uiMultipleScatteringMode != m_PostProcessingAttribs.m_uiMultipleScatteringMode  ||
        PPAttribs.m_uiSingleScatteringMode != m_PostProcessingAttribs.m_uiSingleScatteringMode ||
        PPAttribs.m_bAutoExposure != m_PostProcessingAttribs.m_bAutoExposure || 
        PPAttribs.m_uiToneMappingMode != m_PostProcessingAttribs.m_uiToneMappingMode)
    {
        for(size_t i=0; i<_countof(m_pFixInsctrAtDepthBreaksPS); ++i)
            m_pFixInsctrAtDepthBreaksPS[i].Release();
    }
    
    if( PPAttribs.m_uiMaxSamplesInSlice != m_PostProcessingAttribs.m_uiMaxSamplesInSlice || 
        PPAttribs.m_uiNumEpipolarSlices != m_PostProcessingAttribs.m_uiNumEpipolarSlices )
    {
        m_uiUpToDateResourceFlags &= ~UpToDateResourceFlags::AuxTextures;
        m_uiUpToDateResourceFlags &= ~UpToDateResourceFlags::ExtinctionTexture;
        m_uiUpToDateResourceFlags &= ~UpToDateResourceFlags::SliceUVDirAndOriginTex;
    }

    if( PPAttribs.m_uiMinMaxShadowMapResolution != m_PostProcessingAttribs.m_uiMinMaxShadowMapResolution || 
        PPAttribs.m_uiNumEpipolarSlices != m_PostProcessingAttribs.m_uiNumEpipolarSlices ||
        PPAttribs.m_bUse1DMinMaxTree != m_PostProcessingAttribs.m_bUse1DMinMaxTree ||
        PPAttribs.m_bIs32BitMinMaxMipMap != m_PostProcessingAttribs.m_bIs32BitMinMaxMipMap ||
        bUseCombinedMinMaxTexture != m_bUseCombinedMinMaxTexture ||
        bUseCombinedMinMaxTexture && 
            (PPAttribs.m_iFirstCascade != m_PostProcessingAttribs.m_iFirstCascade || 
             PPAttribs.m_iNumCascades  != m_PostProcessingAttribs.m_iNumCascades) )
    {
        for(int i=0; i < _countof(m_ptex2DMinMaxShadowMapSRV); ++i)
            m_ptex2DMinMaxShadowMapSRV[i].Release();
        for(int i=0; i < _countof(m_ptex2DMinMaxShadowMapRTV); ++i)
            m_ptex2DMinMaxShadowMapRTV[i].Release();
    }

    if( PPAttribs.m_iNumCascades != m_PostProcessingAttribs.m_iNumCascades )
    {
        m_uiUpToDateResourceFlags &= ~UpToDateResourceFlags::SliceUVDirAndOriginTex;
    }


    if( PPAttribs.m_uiCascadeProcessingMode != m_PostProcessingAttribs.m_uiCascadeProcessingMode )
    {
        m_pComputeMinMaxSMLevelPS.Release();
    }
    
    if( PPAttribs.m_uiExtinctionEvalMode != m_PostProcessingAttribs.m_uiExtinctionEvalMode )
    {
        m_ptex2DEpipolarExtinctionRTV.Release();
        m_uiUpToDateResourceFlags &= ~UpToDateResourceFlags::ExtinctionTexture;
        m_pUnwarpEpipolarSctrImgPS.Release();
        m_pUnwarpAndRenderLuminancePS.Release();
        m_pRenderCoarseUnshadowedInsctrPS.Release();
    }

    if( PPAttribs.m_uiSingleScatteringMode != m_PostProcessingAttribs.m_uiSingleScatteringMode ||
        PPAttribs.m_uiMultipleScatteringMode != m_PostProcessingAttribs.m_uiMultipleScatteringMode )
        m_pRenderCoarseUnshadowedInsctrPS.Release();

    bool bRecomputeSctrCoeffs = m_PostProcessingAttribs.m_bUseCustomSctrCoeffs != PPAttribs.m_bUseCustomSctrCoeffs ||
                                m_PostProcessingAttribs.m_fAerosolDensityScale != PPAttribs.m_fAerosolDensityScale ||
                                m_PostProcessingAttribs.m_fAerosolAbsorbtionScale != PPAttribs.m_fAerosolAbsorbtionScale ||
                                PPAttribs.m_bUseCustomSctrCoeffs && 
                                    ( m_PostProcessingAttribs.m_f4CustomRlghBeta != PPAttribs.m_f4CustomRlghBeta ||
                                      m_PostProcessingAttribs.m_f4CustomMieBeta  != PPAttribs.m_f4CustomMieBeta );

    m_PostProcessingAttribs = PPAttribs;
    m_bUseCombinedMinMaxTexture = bUseCombinedMinMaxTexture;

    if( bRecomputeSctrCoeffs )
    {
        m_uiUpToDateResourceFlags &= ~UpToDateResourceFlags::PrecomputedOpticaLDepthTex;
        m_uiUpToDateResourceFlags &= ~UpToDateResourceFlags::AmbientSkyLightTex;
        m_ptex3DSingleScatteringSRV.Release();
        ComputeScatteringCoefficients(FrameAttribs.pDeviceContext);
    }

    if( !(m_uiUpToDateResourceFlags & UpToDateResourceFlags::AuxTextures) )
    {
        m_pRenderScript->Run( "CreateAuxTextures", m_PostProcessingAttribs.m_uiNumEpipolarSlices, m_PostProcessingAttribs.m_uiMaxSamplesInSlice );
        m_uiUpToDateResourceFlags |= UpToDateResourceFlags::AuxTextures;
        m_ptex2DEpipolarInscatteringRTV.Release();
        m_pRenderScript->GetTextureViewByName( "tex2DEpipolarInscatteringRTV", &m_ptex2DEpipolarInscatteringRTV );
        m_ptex2DEpipolarImageDSV.Release();
        m_pRenderScript->GetTextureViewByName( "tex2DEpipolarImageDSV", &m_ptex2DEpipolarImageDSV );
        // Make sure extinction texture is re-created when first needed
        m_uiUpToDateResourceFlags &= ~UpToDateResourceFlags::ExtinctionTexture;
        m_ptex2DEpipolarExtinctionRTV.Release();
        // Make sure slice UV and origin texture is re-created when first needed
        m_uiUpToDateResourceFlags &= ~UpToDateResourceFlags::SliceUVDirAndOriginTex;
    }

    if( !m_ptex2DMinMaxShadowMapSRV[0] && m_PostProcessingAttribs.m_bUse1DMinMaxTree )
    {
        CreateMinMaxShadowMap(FrameAttribs.pDevice);
    }

#if 0
    LightAttribs &LightAttribs = *FrameAttribs.pLightAttribs;
    const CameraAttribs &CamAttribs = FrameAttribs.CameraAttribs;

    // Note that in fact the outermost visible screen pixels do not lie exactly on the boundary (+1 or -1), but are biased by
    // 0.5 screen pixel size inwards. Using these adjusted boundaries improves precision and results in
    // smaller number of pixels which require inscattering correction
    assert( LightAttribs.bIsLightOnScreen == (fabs(FrameAttribs.pLightAttribs->f4LightScreenPos.x) <= 1.f - 1.f/(float)m_uiBackBufferWidth && 
                                              fabs(FrameAttribs.pLightAttribs->f4LightScreenPos.y) <= 1.f - 1.f/(float)m_uiBackBufferHeight) );

    const auto &SMAttribs = FrameAttribs.pLightAttribs->ShadowAttribs;

    UpdateConstantBuffer(FrameAttribs.pDeviceContext, m_pcbCameraAttribs, &CamAttribs, sizeof(CamAttribs));
    //UpdateConstantBuffer(FrameAttribs.pDeviceContext, m_pcbLightAttribs, &LightAttribs, sizeof(LightAttribs));
#endif

    {
        MapHelper<PostProcessingAttribs> pPPAttribsBuffData( FrameAttribs.pDeviceContext, m_pcbPostProcessingAttribs, MAP_WRITE_DISCARD, 0 );
        memcpy(pPPAttribsBuffData, &m_PostProcessingAttribs, sizeof(m_PostProcessingAttribs));
    }


    if( !(m_uiUpToDateResourceFlags & UpToDateResourceFlags::PrecomputedOpticaLDepthTex) )
    {
        CreatePrecomputedOpticalDepthTexture(FrameAttribs.pDevice, FrameAttribs.pDeviceContext);
    }


    if( (m_PostProcessingAttribs.m_uiMultipleScatteringMode > MULTIPLE_SCTR_MODE_NONE ||
         PPAttribs.m_uiSingleScatteringMode == SINGLE_SCTR_MODE_LUT) &&
        !m_ptex3DSingleScatteringSRV )
    {
        CreatePrecomputedScatteringLUT(FrameAttribs.pDevice, FrameAttribs.pDeviceContext);
    }


    if( m_PostProcessingAttribs.m_bAutoExposure && !(m_uiUpToDateResourceFlags & UpToDateResourceFlags::LowResLuminamceTex) )
    {
        // static_cast<int>() is required because Run() gets its arguments by reference
        // and gcc will try to find reference to sm_iLowResLuminanceMips, which does not exist
        m_pRenderScript->Run( FrameAttribs.pDeviceContext, "CreateLowResLuminanceTexture", static_cast<int>(sm_iLowResLuminanceMips) );
        m_ptex2DLowResLuminanceRTV.Release();
        m_ptex2DLowResLuminanceSRV.Release();
        m_pRenderScript->GetTextureViewByName( "tex2DLowResLuminanceRTV", &m_ptex2DLowResLuminanceRTV );
        m_pRenderScript->GetTextureViewByName( "tex2DLowResLuminanceSRV", &m_ptex2DLowResLuminanceSRV );
        m_uiUpToDateResourceFlags |= UpToDateResourceFlags::LowResLuminamceTex;
    }

    m_pResMapping->AddResource("g_tex2DDepthBuffer", FrameAttribs.ptex2DSrcDepthBufferSRV, false);
    m_pResMapping->AddResource("g_tex2DColorBuffer", FrameAttribs.ptex2DSrcColorBufferSRV, false);
    m_pResMapping->AddResource("g_tex2DLightSpaceDepthMap", FrameAttribs.ptex2DShadowMapSRV, false);
    m_pResMapping->AddResource("cbCameraAttribs", FrameAttribs.pcbCameraAttribs, false);
    m_pResMapping->AddResource("cbLightParams", FrameAttribs.pcbLightAttribs, false);

    ITextureView *pRTVs[] = { FrameAttribs.ptex2DSrcColorBufferRTV };
    FrameAttribs.pDeviceContext->SetRenderTargets( _countof( pRTVs ), pRTVs, FrameAttribs.ptex2DSrcDepthBufferDSV );
    RenderSun(FrameAttribs);

    ReconstructCameraSpaceZ(FrameAttribs);

    if( m_PostProcessingAttribs.m_uiLightSctrTechnique == LIGHT_SCTR_TECHNIQUE_EPIPOLAR_SAMPLING )
    {
        RenderSliceEndpoints(FrameAttribs);

        // Render coordinate texture and camera space z for epipolar location
        RenderCoordinateTexture(FrameAttribs);

        if( m_PostProcessingAttribs.m_uiRefinementCriterion == REFINEMENT_CRITERION_INSCTR_DIFF || 
            m_PostProcessingAttribs.m_uiExtinctionEvalMode == EXTINCTION_EVAL_MODE_EPIPOLAR )
        {
            RenderCoarseUnshadowedInctr(FrameAttribs);
        }

        // Refine initial ray marching samples
        RefineSampleLocations(FrameAttribs);

        // Mark all ray marching samples in stencil
        MarkRayMarchingSamples( FrameAttribs );

        if( m_PostProcessingAttribs.m_bEnableLightShafts && m_PostProcessingAttribs.m_bUse1DMinMaxTree )
        {
            RenderSliceUVDirAndOrig(FrameAttribs);
        }

        m_pRenderScript->Run(FrameAttribs.pDeviceContext, "ClearInitialScatteredLight" );

        int iLastCascade = (m_PostProcessingAttribs.m_bEnableLightShafts && m_PostProcessingAttribs.m_uiCascadeProcessingMode == CASCADE_PROCESSING_MODE_MULTI_PASS) ? m_PostProcessingAttribs.m_iNumCascades - 1 : m_PostProcessingAttribs.m_iFirstCascade;
        for(int iCascadeInd = m_PostProcessingAttribs.m_iFirstCascade; iCascadeInd <= iLastCascade; ++iCascadeInd)
        {
            // Build min/max mip map
            if( m_PostProcessingAttribs.m_bEnableLightShafts && m_PostProcessingAttribs.m_bUse1DMinMaxTree )
            {
                Build1DMinMaxMipMap(FrameAttribs, iCascadeInd);
            }
            // Perform ray marching for selected samples
            DoRayMarching(FrameAttribs, m_PostProcessingAttribs.m_uiShadowMapResolution, iCascadeInd);
        }

        // Interpolate ray marching samples onto the rest of samples
        InterpolateInsctrIrradiance(FrameAttribs);

        const Uint32 uiMaxStepsAlongRayAtDepthBreak0 = std::min(m_PostProcessingAttribs.m_uiShadowMapResolution/4, 256u);
        const Uint32 uiMaxStepsAlongRayAtDepthBreak1 = std::min(m_PostProcessingAttribs.m_uiShadowMapResolution/8, 128u);

        if( m_PostProcessingAttribs.m_bAutoExposure )
        {
            // Render scene luminance to low-resolution texture
            ITextureView *pRTVs[] = { m_ptex2DLowResLuminanceRTV };
            FrameAttribs.pDeviceContext->SetRenderTargets(_countof(pRTVs), pRTVs, nullptr);
            UnwarpEpipolarScattering(FrameAttribs, true);
            m_ptex2DLowResLuminanceSRV->GenerateMips( FrameAttribs.pDeviceContext );

            UpdateAverageLuminance(FrameAttribs);
        }
        // Set the main back & depth buffers
        FrameAttribs.pDeviceContext->SetRenderTargets( 0, nullptr, nullptr );

        // Clear depth to 1.0.
        FrameAttribs.pDeviceContext->ClearDepthStencil( nullptr, CLEAR_DEPTH_FLAG, 1.f );
        // Transform inscattering irradiance from epipolar coordinates back to rectangular
        // The shader will write 0.0 to the depth buffer, but all pixel that require inscattering
        // correction will be discarded and will keep 1.0
        UnwarpEpipolarScattering(FrameAttribs, false);

        // Correct inscattering for pixels, for which no suitable interpolation sources were found
        if( m_PostProcessingAttribs.m_bCorrectScatteringAtDepthBreaks )
        {
            FixInscatteringAtDepthBreaks(FrameAttribs, uiMaxStepsAlongRayAtDepthBreak0, EFixInscatteringMode::FixInscattering);
        }

        if( m_PostProcessingAttribs.m_bShowSampling )
        {
            RenderSampleLocations(FrameAttribs);
        }
    }
    else if(m_PostProcessingAttribs.m_uiLightSctrTechnique == LIGHT_SCTR_TECHNIQUE_BRUTE_FORCE )
    {
        if( m_PostProcessingAttribs.m_bAutoExposure )
        {
            // Render scene luminance to low-resolution texture
            ITextureView *pRTVs[] = { m_ptex2DLowResLuminanceRTV };
            FrameAttribs.pDeviceContext->SetRenderTargets(_countof(pRTVs), pRTVs, nullptr);

            FixInscatteringAtDepthBreaks(FrameAttribs, m_PostProcessingAttribs.m_uiShadowMapResolution, EFixInscatteringMode::LuminanceOnly);
            m_ptex2DLowResLuminanceSRV->GenerateMips( FrameAttribs.pDeviceContext );

            UpdateAverageLuminance(FrameAttribs);
        }

        // Set the main back & depth buffers
        FrameAttribs.pDeviceContext->SetRenderTargets( 0, nullptr, nullptr );

        FixInscatteringAtDepthBreaks(FrameAttribs, m_PostProcessingAttribs.m_uiShadowMapResolution, EFixInscatteringMode::FullScreenRayMarching);
    }

    FrameAttribs.pDeviceContext->SetRenderTargets( 0, nullptr, nullptr );
}


void LightSctrPostProcess :: CreateMinMaxShadowMap(IRenderDevice* pDevice)
{
    TextureDesc MinMaxShadowMapTexDesc;
    MinMaxShadowMapTexDesc.Type = TEXTURE_TYPE_2D;
    MinMaxShadowMapTexDesc.Width = m_PostProcessingAttribs.m_uiMinMaxShadowMapResolution;
    MinMaxShadowMapTexDesc.Height = m_PostProcessingAttribs.m_uiNumEpipolarSlices;
    MinMaxShadowMapTexDesc.MipLevels = 1;
    MinMaxShadowMapTexDesc.Format = m_PostProcessingAttribs.m_bIs32BitMinMaxMipMap ? TEX_FORMAT_RG32_FLOAT : TEX_FORMAT_RG16_UNORM;
    MinMaxShadowMapTexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;

    if( m_bUseCombinedMinMaxTexture )
    {
        MinMaxShadowMapTexDesc.Height *= (m_PostProcessingAttribs.m_iNumCascades - m_PostProcessingAttribs.m_iFirstCascade);
    }
    
    for(int i=0; i < 2; ++i)
    {
        m_ptex2DMinMaxShadowMapSRV[i].Release();
        m_ptex2DMinMaxShadowMapRTV[i].Release();
        Diligent::RefCntAutoPtr<ITexture> ptex2DMinMaxShadowMap;
        // Create 2-D texture, shader resource and target view buffers on the device
        pDevice->CreateTexture( MinMaxShadowMapTexDesc, TextureData(), &ptex2DMinMaxShadowMap);
        m_ptex2DMinMaxShadowMapSRV[i] = ptex2DMinMaxShadowMap->GetDefaultView( TEXTURE_VIEW_SHADER_RESOURCE );
        m_ptex2DMinMaxShadowMapRTV[i] = ptex2DMinMaxShadowMap->GetDefaultView( TEXTURE_VIEW_RENDER_TARGET );
    }
}



float2 exp(const float2 &fX){ return float2(exp(fX.x), exp(fX.y)); }
float3 exp(const float3 &fX){ return float3(exp(fX.x), exp(fX.y), exp(fX.z)); }

// fCosChi = Pi/2
float2 ChapmanOrtho(const float2 &f2x)
{
    static const float fConst = static_cast<float>( sqrt(M_PI / 2) );
    float2 f2SqrtX = float2( sqrt(f2x.x), sqrt(f2x.y) );
    return fConst * ( float2(1.f,1.f) / (2.f * f2SqrtX) + f2SqrtX );
}

// |fCosChi| < Pi/2
float2 f2ChapmanRising(const float2 &f2X, float fCosChi)
{
    float2 f2ChOrtho = ChapmanOrtho(f2X);
    return f2ChOrtho / ((f2ChOrtho-float2(1,1))*fCosChi + float2(1,1));
}

float2 GetDensityIntegralFromChapmanFunc(float fHeightAboveSurface,
                                         const float3 &f3EarthCentreToPointDir,
                                         const float3 &f3RayDir,
                                         const AirScatteringAttribs &SctrMediaAttribs)
{
    // Note: there is no intersection test with the Earth. However,
    // optical depth through the Earth is large, which effectively
    // occludes the light
    float fCosChi = dot(f3EarthCentreToPointDir, f3RayDir);
    float2 f2x = (fHeightAboveSurface + SctrMediaAttribs.fEarthRadius) * float2(1.f / SctrMediaAttribs.f2ParticleScaleHeight.x, 1.f / SctrMediaAttribs.f2ParticleScaleHeight.y);
    float2 f2VerticalAirMass = SctrMediaAttribs.f2ParticleScaleHeight * exp(-float2(fHeightAboveSurface,fHeightAboveSurface)/SctrMediaAttribs.f2ParticleScaleHeight);
    if( fCosChi >= 0.f )
    {
        return f2VerticalAirMass * f2ChapmanRising(f2x, fCosChi);
    }
    else
    {
        float fSinChi = sqrt(1.f - fCosChi*fCosChi);
        float fh0 = (fHeightAboveSurface + SctrMediaAttribs.fEarthRadius) * fSinChi - SctrMediaAttribs.fEarthRadius;
        float2 f2VerticalAirMass0 = SctrMediaAttribs.f2ParticleScaleHeight * exp(-float2(fh0,fh0)/SctrMediaAttribs.f2ParticleScaleHeight);
        float2 f2x0 = float2(fh0 + SctrMediaAttribs.fEarthRadius,fh0 + SctrMediaAttribs.fEarthRadius)/SctrMediaAttribs.f2ParticleScaleHeight;
        float2 f2ChOrtho_x0 = ChapmanOrtho(f2x0);
        float2 f2Ch = f2ChapmanRising(f2x, -fCosChi);
        return f2VerticalAirMass0 * (2.f * f2ChOrtho_x0) - f2VerticalAirMass*f2Ch;
    }
}

void LightSctrPostProcess :: ComputeSunColor( const float3 &vDirectionOnSun,
                                               const float4 &f4ExtraterrestrialSunColor,
                                               float4 &f4SunColorAtGround,
                                               float4 &f4AmbientLight)
{

    // Compute the ambient light values
    float zenithFactor = std::min( std::max(vDirectionOnSun.y, 0.0f), 1.0f);
    f4AmbientLight.x = zenithFactor*0.15f;
    f4AmbientLight.y = zenithFactor*0.1f;
    f4AmbientLight.z = std::max(0.005f, zenithFactor*0.25f);
    f4AmbientLight.w = 0.0f;

    float2 f2NetParticleDensityToAtmTop = GetDensityIntegralFromChapmanFunc(0, float3(0,1,0), vDirectionOnSun, m_MediaParams);


    float3 f3RlghExtCoeff = std::max( (float3&)m_MediaParams.f4RayleighExtinctionCoeff, float3(1e-8f,1e-8f,1e-8f) );
    float3 f3RlghOpticalDepth = f3RlghExtCoeff * f2NetParticleDensityToAtmTop.x;
    float3 f3MieExtCoeff = std::max( (float3&)m_MediaParams.f4MieExtinctionCoeff, float3(1e-8f,1e-8f,1e-8f) );
    float3 f3MieOpticalDepth  = f3MieExtCoeff * f2NetParticleDensityToAtmTop.y;
    float3 f3TotalExtinction = exp( -(f3RlghOpticalDepth + f3MieOpticalDepth ) );
    const float fEarthReflectance = 0.1f;// See [BN08]
    (float3&)f4SunColorAtGround = ((float3&)f4ExtraterrestrialSunColor) * f3TotalExtinction * fEarthReflectance;
}

void LightSctrPostProcess :: ComputeScatteringCoefficients(IDeviceContext *pDeviceCtx)
{
    // For details, see "A practical Analytic Model for Daylight" by Preetham & Hoffman, p.23

    // Wave lengths
    // [BN08] follows [REK04] and gives the following values for Rayleigh scattering coefficients:
    // RayleighBetha(lambda = (680nm, 550nm, 440nm) ) = (5.8, 13.5, 33.1)e-6
    static const double dWaveLengths[] = 
    {
        680e-9,     // red
        550e-9,     // green
        440e-9      // blue
    }; 
        
    // Calculate angular and total scattering coefficients for Rayleigh scattering:
    {
        float4 &f4AngularRayleighSctrCoeff = m_MediaParams.f4AngularRayleighSctrCoeff;
        float4 &f4TotalRayleighSctrCoeff = m_MediaParams.f4TotalRayleighSctrCoeff;
        float4 &f4RayleighExtinctionCoeff = m_MediaParams.f4RayleighExtinctionCoeff;
    
        double n = 1.0003;    // - Refractive index of air in the visible spectrum
        double N = 2.545e+25; // - Number of molecules per unit volume
        double Pn = 0.035;    // - Depolarization factor for air which exoresses corrections 
                              //   due to anisotropy of air molecules

        double dRayleighConst = 8.0*M_PI*M_PI*M_PI * (n*n - 1.0) * (n*n - 1.0) / (3.0 * N) * (6.0 + 3.0*Pn) / (6.0 - 7.0*Pn);
        for(int WaveNum = 0; WaveNum < 3; WaveNum++)
        {
            double dSctrCoeff;
            if( m_PostProcessingAttribs.m_bUseCustomSctrCoeffs )
                dSctrCoeff = f4TotalRayleighSctrCoeff[WaveNum] = m_PostProcessingAttribs.m_f4CustomRlghBeta[WaveNum];
            else
            {
                double Lambda2 = dWaveLengths[WaveNum] * dWaveLengths[WaveNum];
                double Lambda4 = Lambda2 * Lambda2;
                dSctrCoeff = dRayleighConst / Lambda4;
                // Total Rayleigh scattering coefficient is the integral of angular scattering coefficient in all directions
                f4TotalRayleighSctrCoeff[WaveNum] = static_cast<float>( dSctrCoeff );
            }
            // Angular scattering coefficient is essentially volumetric scattering coefficient multiplied by the
            // normalized phase function
            // p(Theta) = 3/(16*Pi) * (1 + cos^2(Theta))
            // f4AngularRayleighSctrCoeff contains all the terms exepting 1 + cos^2(Theta):
            f4AngularRayleighSctrCoeff[WaveNum] = static_cast<float>( 3.0 / (16.0*M_PI) * dSctrCoeff );
            // f4AngularRayleighSctrCoeff[WaveNum] = f4TotalRayleighSctrCoeff[WaveNum] * p(Theta)
        }
        // Air molecules do not absorb light, so extinction coefficient is only caused by out-scattering
        f4RayleighExtinctionCoeff = f4TotalRayleighSctrCoeff;
    }

    // Calculate angular and total scattering coefficients for Mie scattering:
    {
        float4 &f4AngularMieSctrCoeff = m_MediaParams.f4AngularMieSctrCoeff;
        float4 &f4TotalMieSctrCoeff = m_MediaParams.f4TotalMieSctrCoeff;
        float4 &f4MieExtinctionCoeff = m_MediaParams.f4MieExtinctionCoeff;
        
        if( m_PostProcessingAttribs.m_bUseCustomSctrCoeffs )
        {
            f4TotalMieSctrCoeff = m_PostProcessingAttribs.m_f4CustomMieBeta * m_PostProcessingAttribs.m_fAerosolDensityScale;
        }
        else
        {
            const bool bUsePreethamMethod = false;
            if( bUsePreethamMethod )
            {
                // Values for K came from the table 2 in the "A practical Analytic Model 
                // for Daylight" by Preetham & Hoffman, p.28
                double K[] = 
                { 
                    0.68455,                //  K[650nm]
                    0.678781,               //  K[570nm]
                    (0.668532+0.669765)/2.0 // (K[470nm]+K[480nm])/2
                };

                assert( m_MediaParams.fTurbidity >= 1.f );
        
                // Beta is an Angstrom's turbidity coefficient and is approximated by:
                //float beta = 0.04608365822050f * m_fTurbidity - 0.04586025928522f; ???????

                double c = (0.6544*m_MediaParams.fTurbidity - 0.6510)*1E-16; // concentration factor
                const double v = 4; // Junge's exponent
        
                double dTotalMieBetaTerm = 0.434 * c * M_PI * pow(2.0*M_PI, v-2);

                for(int WaveNum = 0; WaveNum < 3; WaveNum++)
                {
                    double Lambdav_minus_2 = pow( dWaveLengths[WaveNum], v-2);
                    double dTotalMieSctrCoeff = dTotalMieBetaTerm * K[WaveNum] / Lambdav_minus_2;
                    f4TotalMieSctrCoeff[WaveNum]   = static_cast<float>( dTotalMieSctrCoeff );
                }
            
                //AtmScatteringAttribs.f4AngularMieSctrCoeff *= 0.02f;
                //AtmScatteringAttribs.f4TotalMieSctrCoeff *= 0.02f;
            }
            else
            {
                // [BN08] uses the following value (independent of wavelength) for Mie scattering coefficient: 2e-5
                // For g=0.76 and MieBetha=2e-5 [BN08] was able to reproduce the same luminance as given by the 
                // reference CIE sky light model 
                const float fMieBethaBN08 = 2e-5f * m_PostProcessingAttribs.m_fAerosolDensityScale;
                m_MediaParams.f4TotalMieSctrCoeff = float4(fMieBethaBN08, fMieBethaBN08, fMieBethaBN08, 0);
            }
        }

        for(int WaveNum = 0; WaveNum < 3; WaveNum++)
        {
            // Normalized to unity Cornette-Shanks phase function has the following form:
            // F(theta) = 1/(4*PI) * 3*(1-g^2) / (2*(2+g^2)) * (1+cos^2(theta)) / (1 + g^2 - 2g*cos(theta))^(3/2)
            // The angular scattering coefficient is the volumetric scattering coefficient multiplied by the phase 
            // function. 1/(4*PI) is baked into the f4AngularMieSctrCoeff, the other terms are baked into f4CS_g
            f4AngularMieSctrCoeff[WaveNum] = f4TotalMieSctrCoeff[WaveNum]  / static_cast<float>(4.0 * M_PI);
            // [BN08] also uses slight absorption factor which is 10% of scattering
            f4MieExtinctionCoeff[WaveNum] = f4TotalMieSctrCoeff[WaveNum] * (1.f + m_PostProcessingAttribs.m_fAerosolAbsorbtionScale);
        }
    }
    
    {
        // For g=0.76 and MieBetha=2e-5 [BN08] was able to reproduce the same luminance as is given by the 
        // reference CIE sky light model 
        // Cornette phase function (see Nishita et al. 93):
        // F(theta) = 1/(4*PI) * 3*(1-g^2) / (2*(2+g^2)) * (1+cos^2(theta)) / (1 + g^2 - 2g*cos(theta))^(3/2)
        // 1/(4*PI) is baked into the f4AngularMieSctrCoeff
        float4 &f4CS_g = m_MediaParams.f4CS_g;
        float f_g = m_MediaParams.m_fAerosolPhaseFuncG;
        f4CS_g.x = 3*(1.f - f_g*f_g) / ( 2*(2.f + f_g*f_g) );
        f4CS_g.y = 1.f + f_g*f_g;
        f4CS_g.z = -2.f*f_g;
        f4CS_g.w = 1.f;
    }

    m_MediaParams.f4TotalExtinctionCoeff = m_MediaParams.f4RayleighExtinctionCoeff + m_MediaParams.f4MieExtinctionCoeff;

    if( pDeviceCtx && m_pcbMediaAttribs )
    {
        m_pcbMediaAttribs->UpdateData( pDeviceCtx, 0, sizeof( m_MediaParams ), &m_MediaParams );
    }
}


void LightSctrPostProcess :: RenderSun(FrameAttribs &FrameAttribs)
{
    if( FrameAttribs.pLightAttribs->f4LightScreenPos.w <= 0 )
        return;

    m_pRenderScript->Run(FrameAttribs.pDeviceContext, "RenderSun" );
}

void LightSctrPostProcess :: ComputeAmbientSkyLightTexture(IRenderDevice *pDevice, IDeviceContext *pContext)
{
    if( !(m_uiUpToDateResourceFlags & UpToDateResourceFlags::PrecomputedOpticaLDepthTex) )
    {
        CreatePrecomputedOpticalDepthTexture(pDevice, pContext);
    }
    
    if( !m_ptex3DSingleScatteringSRV )
    {
        CreatePrecomputedScatteringLUT(pDevice, pContext);
    }

    if( !m_pPrecomputeAmbientSkyLightPS )
    {
        ShaderMacroHelper Macros;
        Macros.AddShaderMacro( "NUM_RANDOM_SPHERE_SAMPLES", sm_iNumRandomSamplesOnSphere );
        Macros.Finalize();
        CreateShader( pDevice, "Precomputation.fx", "PrecomputeAmbientSkyLightPS", SHADER_TYPE_PIXEL, Macros, &m_pPrecomputeAmbientSkyLightPS );
    }

    Diligent::RefCntAutoPtr<ITextureView> ptex2DAmbientSkyLightRTV;
    m_pRenderScript->GetTextureViewByName( "tex2DAmbientSkyLightRTV", &ptex2DAmbientSkyLightRTV );
    // Create 2-D texture, shader resource and target view buffers on the device
    
    m_pPrecomputeAmbientSkyLightPS->BindResources( m_pResMapping, BIND_SHADER_RESOURCES_ALL_RESOLVED );

    ITextureView *pRTVs[] = {ptex2DAmbientSkyLightRTV};
    pContext->SetRenderTargets(1, pRTVs, nullptr);

    m_pRenderScript->Run(pContext, "PrecomputeAmbientSkyLight", m_pPrecomputeAmbientSkyLightPS);
    m_uiUpToDateResourceFlags |= UpToDateResourceFlags::AmbientSkyLightTex;
}


ITextureView* LightSctrPostProcess :: GetAmbientSkyLightSRV(IRenderDevice *pDevice, IDeviceContext *pContext)
{
    if( !(m_uiUpToDateResourceFlags & UpToDateResourceFlags::AmbientSkyLightTex) )
    {
        ComputeAmbientSkyLightTexture(pDevice, pContext);
    }

    return m_ptex2DAmbientSkyLightSRV;
}
