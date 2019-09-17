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

#include <cmath>
#include <algorithm>

#include "AntTweakBar.h"

#include "AtmosphereSample.h"
#include "MapHelper.h"
#include "GraphicsUtilities.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new AtmosphereSample();
}

AtmosphereSample::AtmosphereSample()
{}

void AtmosphereSample::GetEngineInitializationAttribs(DeviceType DevType, EngineCreateInfo& Attribs)
{
    SampleBase::GetEngineInitializationAttribs(DevType, Attribs);
#if VULKAN_SUPPORTED
    if(DevType == DeviceType::Vulkan)
    {
        auto& VkAttrs = static_cast<EngineVkCreateInfo&>(Attribs);
        VkAttrs.EnabledFeatures.depthClamp = true;
        VkAttrs.EnabledFeatures.shaderStorageImageExtendedFormats = true;
    }
#endif
}

void AtmosphereSample::Initialize(IEngineFactory* pEngineFactory, IRenderDevice *pDevice, IDeviceContext **ppContexts, Uint32 NumDeferredCtx, ISwapChain *pSwapChain)
{
    const auto& deviceCaps = pDevice->GetDeviceCaps();
    if(!deviceCaps.bComputeShadersSupported)
    {
        throw std::runtime_error("Compute shaders are required to run this sample");
    }

    SampleBase::Initialize(pEngineFactory, pDevice, ppContexts, NumDeferredCtx, pSwapChain);

    m_bIsGLDevice = deviceCaps.IsGLDevice();
    if( pDevice->GetDeviceCaps().DevType == DeviceType::OpenGLES )
    {
        m_ShadowSettings.Resolution                        = 512;
        m_TerrainRenderParams.m_FilterAcrossShadowCascades = false;
        m_ShadowSettings.iFixedFilterSize                  = 3;
        m_PPAttribs.iFirstCascadeToRayMarch                = 2;
        m_PPAttribs.iSingleScatteringMode                  = SINGLE_SCTR_MODE_LUT;
        m_TerrainRenderParams.m_iNumShadowCascades         = 4;
        m_TerrainRenderParams.m_iNumRings                  = 10;
        m_TerrainRenderParams.m_TexturingMode              = RenderingParams::TM_MATERIAL_MASK;
    }

    m_f4CustomRlghBeta = m_PPAttribs.f4CustomRlghBeta;
    m_f4CustomMieBeta = m_PPAttribs.f4CustomMieBeta;

	m_strRawDEMDataFile = "Terrain\\HeightMap.tif";
    m_strMtrlMaskFile = "Terrain\\Mask.png";
    m_strTileTexPaths[0] = "Terrain\\Tiles\\gravel_DM.dds";
    m_strTileTexPaths[1] = "Terrain\\Tiles\\grass_DM.dds";
    m_strTileTexPaths[2] = "Terrain\\Tiles\\cliff_DM.dds";
    m_strTileTexPaths[3] = "Terrain\\Tiles\\snow_DM.dds";
    m_strTileTexPaths[4] = "Terrain\\Tiles\\grassDark_DM.dds";
    m_strNormalMapTexPaths[0] = "Terrain\\Tiles\\gravel_NM.dds";
    m_strNormalMapTexPaths[1] = "Terrain\\Tiles\\grass_NM.dds";
    m_strNormalMapTexPaths[2] = "Terrain\\Tiles\\cliff_NM.dds";
    m_strNormalMapTexPaths[3] = "Terrain\\Tiles\\Snow_NM.jpg";
    m_strNormalMapTexPaths[4] = "Terrain\\Tiles\\grass_NM.dds";

    // Create data source
    try
    {
		m_pElevDataSource.reset( new ElevationDataSource(m_strRawDEMDataFile.c_str()) );
        m_pElevDataSource->SetOffsets(m_TerrainRenderParams.m_iColOffset, m_TerrainRenderParams.m_iRowOffset);
        m_fMinElevation = m_pElevDataSource->GetGlobalMinElevation() * m_TerrainRenderParams.m_TerrainAttribs.m_fElevationScale;
        m_fMaxElevation = m_pElevDataSource->GetGlobalMaxElevation() * m_TerrainRenderParams.m_TerrainAttribs.m_fElevationScale;
    }
    catch(const std::exception &)
    {
        LOG_ERROR("Failed to create elevation data source");
        return;
    }

	const Char *strTileTexPaths[EarthHemsiphere::NUM_TILE_TEXTURES], *strNormalMapPaths[EarthHemsiphere::NUM_TILE_TEXTURES];
	for(int iTile=0; iTile < _countof(strTileTexPaths); ++iTile )
    {
		strTileTexPaths[iTile] = m_strTileTexPaths[iTile].c_str();
        strNormalMapPaths[iTile] = m_strNormalMapTexPaths[iTile].c_str();
    }
    
    CreateUniformBuffer( pDevice, sizeof( CameraAttribs ), "Camera Attribs CB", &m_pcbCameraAttribs );
    CreateUniformBuffer( pDevice, sizeof( LightAttribs ), "Light Attribs CB", &m_pcbLightAttribs );

    const auto &SCDesc = pSwapChain->GetDesc();
    m_pLightSctrPP.reset( new EpipolarLightScattering(m_pDevice, m_pImmediateContext, SCDesc.ColorBufferFormat, SCDesc.DepthBufferFormat, TEX_FORMAT_R11G11B10_FLOAT) );
    auto *pcMediaScatteringParams = m_pLightSctrPP->GetMediaAttribsCB();

    m_EarthHemisphere.Create(m_pElevDataSource.get(), 
                             m_TerrainRenderParams, 
                             m_pDevice, 
                             m_pImmediateContext, 
                             m_strMtrlMaskFile.c_str(), 
                             strTileTexPaths, 
                             strNormalMapPaths, 
                             m_pcbCameraAttribs,
                             m_pcbLightAttribs,
                             pcMediaScatteringParams );

    CreateShadowMap();

    InitUI();
}

void AtmosphereSample::InitUI()
{
    // Create a tweak bar
    TwBar *bar = TwNewBar("Settings");
    TwDefine(" GLOBAL fontsize=3 ");
    int barSize[2] = {300, 900};
#ifdef ANDROID
    barSize[0] = 800;
    barSize[1] = 1000;
#endif
    TwSetParam(bar, NULL, "size", TW_PARAM_INT32, 2, barSize);

    TwAddVarRW(bar, "FPS", TW_TYPE_FLOAT, &m_fFPS, "readonly=true");

    TwAddVarRW(bar, "Light Direction", TW_TYPE_DIR3F, &m_f3LightDir, "");

    TwAddVarRW( bar, "Camera altitude", TW_TYPE_FLOAT, &m_f3CameraPos.y, "min=2000 max=100000 step=100 keyincr=PGUP keydecr=PGDOWN" );

    // Shadows
    {
        // Define a new enum type for the tweak bar
        TwEnumVal ShadowMapRes[] = // array used to describe the shadow map resolution
        {
            { 512, "512" },
            { 1024, "1024" },
            { 2048, "2048" }
        };
        TwType modeType = TwDefineEnum( "Shadow Map Resolution", ShadowMapRes, _countof( ShadowMapRes ) );  // create a new TwType associated to the enum defined by the ShadowMapRes array
        TwAddVarCB( bar, "Shadow map resolution", modeType,
            [](const void *value, void * clientData)
            {
                AtmosphereSample *pTheSample = reinterpret_cast<AtmosphereSample*>( clientData );
                pTheSample->m_ShadowSettings.Resolution = static_cast<Uint32>(*reinterpret_cast<const int*>(value));
                pTheSample->CreateShadowMap();
            },
            [](void* value, void* clientData)
            {
                AtmosphereSample *pTheSample = reinterpret_cast<AtmosphereSample*>( clientData );
                *static_cast<int*>(value) = static_cast<int>(pTheSample->m_ShadowSettings.Resolution);
            },
            this, "group=Shadows" );

        TwAddVarRW( bar, "Show cascades", TW_TYPE_BOOLCPP, &m_ShadowSettings.bVisualizeCascades, "group=Shadows" );
        TwAddVarCB( bar, "Num cascades", TW_TYPE_INT32, 
            [](const void* value, void* clientData)
            {
                AtmosphereSample *pTheSample = reinterpret_cast<AtmosphereSample*>( clientData );
                pTheSample->m_TerrainRenderParams.m_iNumShadowCascades = *static_cast<const int *>(value);
                pTheSample->CreateShadowMap();
            }, 
            [](void *value, void * clientData)
            {
                AtmosphereSample *pTheSample = reinterpret_cast<AtmosphereSample*>( clientData );
                *static_cast<int *>(value) = pTheSample->m_TerrainRenderParams.m_iNumShadowCascades;
            },
            this, "min=1 max=8 group=Shadows" );
    }

    TwAddVarRW( bar, "Enable Light Scattering", TW_TYPE_BOOLCPP, &m_bEnableLightScattering, "" );

    // Light scattering GUI controls
    {
        TwAddVarRW( bar, "Enable light shafts", TW_TYPE_BOOL32, &m_PPAttribs.bEnableLightShafts, "group=Scattering" );

        // Define a new enum type for the tweak bar
        TwEnumVal LightSctrTech[] = // array used to describe the shadow map resolution
        {
            { LIGHT_SCTR_TECHNIQUE_EPIPOLAR_SAMPLING, "Epipolar" },
            { LIGHT_SCTR_TECHNIQUE_BRUTE_FORCE, "Brute force" }
        };
        TwType LightSctrTechType = TwDefineEnum( "Light scattering tech", LightSctrTech, _countof( LightSctrTech ) );
        TwAddVarRW( bar, "Light scattering tech", LightSctrTechType, &m_PPAttribs.iLightSctrTechnique, "group=Scattering" );

        TwEnumVal Pow2Values[] =
        {
            { 1, "1" },
            { 2, "2" },
            { 4, "4" },
            { 8, "8" },
            { 16, "16" },
            { 32, "32" },
            { 64, "64" },
            { 128, "128" },
            { 256, "256" },
            { 512, "512" },
            { 1024, "1024" },
            { 2048, "2048" }
        };
        TwType BigPow2Enum = TwDefineEnum( "Large powers of two", Pow2Values + 7, 5 );
        TwAddVarRW( bar, "NumSlices", BigPow2Enum, &m_PPAttribs.uiNumEpipolarSlices, "group=Scattering label=\'Num slices\'" );
        TwAddVarRW( bar, "MaxSamples", BigPow2Enum, &m_PPAttribs.uiMaxSamplesInSlice, "group=Scattering label=\'Max samples\'" );
        TwType SmallPow2Enum = TwDefineEnum( "Small powers of two", Pow2Values+2, 5 );
        TwAddVarRW( bar, "IntialStep", SmallPow2Enum, &m_PPAttribs.uiInitialSampleStepInSlice, "group=Scattering label=\'Initial step\'" );
        
        TwAddVarRW( bar, "ShowSampling", TW_TYPE_BOOL32, &m_PPAttribs.bShowSampling, "group=Scattering label=\'Show Sampling\'" );
        TwAddVarRW( bar, "RefinementThreshold", TW_TYPE_FLOAT, &m_PPAttribs.fRefinementThreshold, "group=Scattering label=\'Refinement Threshold\' min=0.01 max=0.5 step=0.01" );
        TwAddVarRW( bar, "1DMinMaxOptimization", TW_TYPE_BOOL32, &m_PPAttribs.bUse1DMinMaxTree, "group=Scattering label=\'Use 1D min/max trees\'" );
        TwAddVarRW( bar, "OptimizeSampleLocations", TW_TYPE_BOOL32, &m_PPAttribs.bOptimizeSampleLocations, "group=Scattering label=\'Optimize Sample Locations\'" );
        TwAddVarRW( bar, "CorrectScattering", TW_TYPE_BOOL32, &m_PPAttribs.bCorrectScatteringAtDepthBreaks, "group=Scattering label=\'Correct Scattering At Depth Breaks\'" );
        TwAddVarRW( bar, "ShowDepthBreaks", TW_TYPE_BOOL32, &m_PPAttribs.bShowDepthBreaks, "group=Scattering label=\'Show Depth Breaks\'" );
        TwAddVarRW( bar, "LightingOnly", TW_TYPE_BOOL32, &m_PPAttribs.bShowLightingOnly, "group=Scattering label=\'Lighting Only\'" );
        //TwAddVarRW( bar, "ScatteringScale", TW_TYPE_FLOAT, &m_fScatteringScale, "group=Scattering label=\'Scattering scale\' min=0 max=2 step=0.1" );

        TwAddVarRW( bar, "NumIntegrationSteps", TW_TYPE_UINT32, &m_PPAttribs.uiInstrIntegralSteps, "min=5 max=100 step=5 group=Advanced label=\'Num Integrtion Steps\'" );
        TwDefine( "Settings/Advanced group=Scattering" );

        {
            TwType EpipoleSamplingDensityEnum = TwDefineEnum( "Epipole sampling density enum", Pow2Values, 4 );
            TwAddVarRW( bar, "EpipoleSamplingDensity", EpipoleSamplingDensityEnum, &m_PPAttribs.uiEpipoleSamplingDensityFactor, "group=Advanced label=\'Epipole sampling density\'" );
        }
        {
            TwEnumVal SinglSctrMode[] =
            {
                { SINGLE_SCTR_MODE_NONE, "None" },
                { SINGLE_SCTR_MODE_INTEGRATION, "Integration" },
                { SINGLE_SCTR_MODE_LUT, "Look-up table" }
            };
            TwType SinglSctrModeEnum = TwDefineEnum( "Single scattering mode enum", SinglSctrMode, _countof(SinglSctrMode) );
            TwAddVarRW( bar, "SingleSctrMode", SinglSctrModeEnum, &m_PPAttribs.iSingleScatteringMode, "group=Advanced label=\'Single scattering\'" );
        }
        {
            TwEnumVal MultSctrMode[] =
            {
                { MULTIPLE_SCTR_MODE_NONE, "None" },
                { MULTIPLE_SCTR_MODE_UNOCCLUDED, "Unoccluded" },
                { MULTIPLE_SCTR_MODE_OCCLUDED, "Occluded" }
            };
            TwType MultSctrModeEnum = TwDefineEnum( "Higher-order scattering mode enum", MultSctrMode, _countof( MultSctrMode ) );
            TwAddVarRW( bar, "MultipleSctrMode", MultSctrModeEnum, &m_PPAttribs.iMultipleScatteringMode, "group=Advanced label=\'Higher-order scattering\'" );
        }
        {
            TwEnumVal CascadeProcessingMode[] =
            {
                { CASCADE_PROCESSING_MODE_SINGLE_PASS, "Single pass" },
                { CASCADE_PROCESSING_MODE_MULTI_PASS, "Multi-pass" },
                { CASCADE_PROCESSING_MODE_MULTI_PASS_INST, "Multi-pass inst" }
            };
            TwType CascadeProcessingModeEnum = TwDefineEnum( "Cascade processing mode enum", CascadeProcessingMode, _countof( CascadeProcessingMode ) );
            TwAddVarRW( bar, "CascadeProcessingMode", CascadeProcessingModeEnum, &m_PPAttribs.iCascadeProcessingMode, "group=Advanced label=\'Cascade processing mode\'" );
        }
        TwAddVarRW( bar, "FirstCascadeToRayMarch", TW_TYPE_INT32, &m_PPAttribs.iFirstCascadeToRayMarch, "min=0 max=8 step=1 group=Advanced label=\'Start cascade\'" );
        TwAddVarRW( bar, "Is32BitMinMaxShadowMap", TW_TYPE_BOOL32, &m_PPAttribs.bIs32BitMinMaxMipMap, "group=Advanced label=\'Use 32-bit float min/max SM\'" );
        {
            TwEnumVal RefinementCriterion[] =
            {
                { REFINEMENT_CRITERION_DEPTH_DIFF, "Depth difference" },
                { REFINEMENT_CRITERION_INSCTR_DIFF, "Scattering difference" }
            };
            TwType CascadeProcessingModeEnum = TwDefineEnum( "Refinement criterion enum", RefinementCriterion, _countof( RefinementCriterion ) );
            TwAddVarRW( bar, "RefinementCriterion", CascadeProcessingModeEnum, &m_PPAttribs.iRefinementCriterion, "group=Advanced label=\'Refinement criterion\'" );
        }
        {
            TwEnumVal ExtinctionEvalMode[] =
            {
                { EXTINCTION_EVAL_MODE_PER_PIXEL, "Per pixel" },
                { EXTINCTION_EVAL_MODE_EPIPOLAR, "Epipolar" }
            };
            TwType ExtinctionEvalModeEnum = TwDefineEnum( "Extinction eval mode enum", ExtinctionEvalMode, _countof( ExtinctionEvalMode ) );
            TwAddVarRW( bar, "ExtinctionEval", ExtinctionEvalModeEnum, &m_PPAttribs.iExtinctionEvalMode, "group=Advanced label=\'Extinction eval mode\'" );
        }
        TwAddVarRW( bar, "AerosolDensity", TW_TYPE_FLOAT, &m_PPAttribs.fAerosolDensityScale, "group=Advanced label=\'Aerosol density\' min=0.1 max=5.0 step=0.1" );
        TwAddVarRW( bar, "AerosolAbsorption", TW_TYPE_FLOAT, &m_PPAttribs.fAerosolAbsorbtionScale, "group=Advanced label=\'Aerosol absorption\' min=0.0 max=5.0 step=0.1" );
        TwAddVarRW( bar, "UseCustomSctrCoeffs", TW_TYPE_BOOL32, &m_PPAttribs.bUseCustomSctrCoeffs, "group=Advanced label=\'Use custom scattering coeffs\'" );

        #define RLGH_COLOR_SCALE 5e-5f
        #define MIE_COLOR_SCALE 5e-5f
        TwAddVarCB(bar, "RayleighColor", TW_TYPE_COLOR4F, 
            []( const void *value, void * clientData )
            {
                AtmosphereSample *pTheSample = reinterpret_cast<AtmosphereSample*>( clientData );
                pTheSample->m_f4CustomRlghBeta = *reinterpret_cast<const float4 *>(value) * RLGH_COLOR_SCALE;
                if( (float3&)pTheSample->m_f4CustomRlghBeta == float3( 0, 0, 0 ) )
                {
                    pTheSample->m_f4CustomRlghBeta = float4( 1, 1, 1, 1 ) * RLGH_COLOR_SCALE / 255.f;
                }
            },
            [](void *value, void * clientData)
            {
                AtmosphereSample *pTheSample = reinterpret_cast<AtmosphereSample*>( clientData );
                float4 RlghColor = pTheSample->m_f4CustomRlghBeta / RLGH_COLOR_SCALE;
                RlghColor.w = 1;
                *reinterpret_cast<float4*>(value) = RlghColor;
            },
            this, "group=Advanced label=\'Rayleigh color\' colormode=rgb");

        TwAddVarCB(bar, "MieColor", TW_TYPE_COLOR4F, 
            []( const void *value, void * clientData )
            {
                AtmosphereSample *pTheSample = reinterpret_cast<AtmosphereSample*>( clientData );
                pTheSample->m_f4CustomMieBeta = *reinterpret_cast<const float4 *>(value) * MIE_COLOR_SCALE;
                if( (float3&)pTheSample->m_f4CustomMieBeta == float3( 0, 0, 0 ) )
                {
                    pTheSample->m_f4CustomMieBeta = float4( 1, 1, 1, 1 ) * MIE_COLOR_SCALE / 255.f;
                }
            },
            [](void *value, void * clientData)
            {
                AtmosphereSample *pTheSample = reinterpret_cast<AtmosphereSample*>( clientData );
                float4 MieColor = pTheSample->m_f4CustomMieBeta / MIE_COLOR_SCALE;
                MieColor.w = 1;
                *reinterpret_cast<float4*>(value) = MieColor;
            },
            this, "group=Advanced label=\'Mie color\' colormode=rgb");
        #undef RLGH_COLOR_SCALE
        #undef MIE_COLOR_SCALE
        TwAddButton(bar, "UpdateCoeffsBtn", 
                    [](void *clientData)
                    {
                        AtmosphereSample *pTheSample = reinterpret_cast<AtmosphereSample*>( clientData );
                        pTheSample->m_PPAttribs.f4CustomRlghBeta = pTheSample->m_f4CustomRlghBeta;
                        pTheSample->m_PPAttribs.f4CustomMieBeta = pTheSample->m_f4CustomMieBeta;
                    }, 
                    this, "group=Advanced label=\'Update coefficients\'");
    }

    // Tone mapping GUI controls
    {
        {
            TwEnumVal ToneMappingMode[] =
            {
                {TONE_MAPPING_MODE_EXP,          "Exp"},
                {TONE_MAPPING_MODE_REINHARD,     "Reinhard"},
                {TONE_MAPPING_MODE_REINHARD_MOD, "Reinhard Mod"},
                {TONE_MAPPING_MODE_UNCHARTED2,   "Uncharted 2"},
                {TONE_MAPPING_FILMIC_ALU,        "Filmic ALU"},
                {TONE_MAPPING_LOGARITHMIC,       "Logarithmic"},
                {TONE_MAPPING_ADAPTIVE_LOG,      "Adaptive log"}
            };
            TwType ToneMappingModeEnum = TwDefineEnum( "Tone mapping mode enum", ToneMappingMode, _countof( ToneMappingMode ) );
            TwAddVarRW( bar, "ToneMappingMode", ToneMappingModeEnum, &m_PPAttribs.ToneMapping.iToneMappingMode, "group=ToneMapping label=\'Mode\'" );
        }
        TwAddVarRW( bar, "WhitePoint", TW_TYPE_FLOAT, &m_PPAttribs.ToneMapping.fWhitePoint, "group=ToneMapping label=\'White point\' min=0.01 max=10.0 step=0.1" );
        TwAddVarRW( bar, "LumSaturation", TW_TYPE_FLOAT, &m_PPAttribs.ToneMapping.fLuminanceSaturation, "group=ToneMapping label=\'Luminance saturation\' min=0.01 max=2.0 step=0.1" );
        TwAddVarRW( bar, "MiddleGray", TW_TYPE_FLOAT, &m_PPAttribs.ToneMapping.fMiddleGray, "group=ToneMapping label=\'Middle Gray\' min=0.01 max=1.0 step=0.01" );
        TwAddVarRW( bar, "AutoExposure", TW_TYPE_BOOL32, &m_PPAttribs.ToneMapping.bAutoExposure, "group=ToneMapping label=\'Auto exposure\'" );
        TwAddVarRW( bar, "LightAdaptation", TW_TYPE_BOOL32, &m_PPAttribs.ToneMapping.bLightAdaptation, "group=ToneMapping label=\'Light adaptation\'" );
    }

    const auto& RG16UAttribs = m_pDevice->GetTextureFormatInfoExt( TEX_FORMAT_RG16_UNORM );
    const auto& RG32FAttribs = m_pDevice->GetTextureFormatInfoExt( TEX_FORMAT_RG32_FLOAT );
    bool RG16USupported = RG16UAttribs.Supported && RG16UAttribs.ColorRenderable;
    bool RG32FSupported = RG32FAttribs.Supported && RG32FAttribs.ColorRenderable;
    if( !RG16USupported && !RG32FSupported )
    {
        int32_t IsVisible = 0;
        TwSetParam( bar, "1DMinMaxOptimization", "visible", TW_PARAM_INT32, 1, &IsVisible );
        m_PPAttribs.bUse1DMinMaxTree = FALSE;
    }

    if( !RG16USupported || !RG32FSupported )
    {
        int32_t IsVisible = 0;
        TwSetParam( bar, "Is32BitMinMaxShadowMap", "visible", TW_PARAM_INT32, 1, &IsVisible );

        if( RG16USupported && !RG32FSupported )
            m_PPAttribs.bIs32BitMinMaxMipMap = FALSE;

        if( !RG16USupported && RG32FSupported )
            m_PPAttribs.bIs32BitMinMaxMipMap = TRUE;
    }
}

void TwSetEnabled( TwBar* bar, const char *VarName, bool bEnabled )
{
    int32_t ReadOnly = bEnabled ? 0 : 1;
    TwSetParam( bar, VarName, "readonly", TW_PARAM_INT32, 1, &ReadOnly );
}

void AtmosphereSample::UpdateGUI()
{
    auto bar = TwGetBarByName( "Settings" );

    {
        int32_t IsVisible = m_bEnableLightScattering ? 1 : 0;
        TwSetParam( bar, "Scattering", "visible", TW_PARAM_INT32, 1, &IsVisible );
        TwSetParam( bar, "ToneMapping", "visible", TW_PARAM_INT32, 1, &IsVisible );
    }

    bool bIsEpipolarSampling = m_PPAttribs.iLightSctrTechnique == LIGHT_SCTR_TECHNIQUE_EPIPOLAR_SAMPLING;
    TwSetEnabled( bar, "NumSlices", bIsEpipolarSampling );
    TwSetEnabled( bar, "MaxSamples", bIsEpipolarSampling );
    TwSetEnabled( bar, "IntialStep", bIsEpipolarSampling );
    TwSetEnabled( bar, "EpipoleSamplingDensity", bIsEpipolarSampling );
    TwSetEnabled( bar, "RefinementThreshold", bIsEpipolarSampling );
    TwSetEnabled( bar, "1DMinMaxOptimization", bIsEpipolarSampling );
    TwSetEnabled( bar, "OptimizeSampleLocations", bIsEpipolarSampling );
    TwSetEnabled( bar, "ShowSampling", bIsEpipolarSampling );
    TwSetEnabled( bar, "CorrectScattering", bIsEpipolarSampling );
    TwSetEnabled( bar, "ShowDepthBreaks", bIsEpipolarSampling && m_PPAttribs.bCorrectScatteringAtDepthBreaks != 0);
    TwSetEnabled( bar, "NumIntegrationSteps", !m_PPAttribs.bEnableLightShafts && m_PPAttribs.iSingleScatteringMode == SINGLE_SCTR_MODE_INTEGRATION );

    {
        int32_t IsVisible = m_PPAttribs.bUseCustomSctrCoeffs ? 1 : 0;
        TwSetParam( bar, "RayleighColor", "visible", TW_PARAM_INT32, 1, &IsVisible );
        TwSetParam( bar, "MieColor", "visible", TW_PARAM_INT32, 1, &IsVisible );
        TwSetParam( bar, "UpdateCoeffsBtn", "visible", TW_PARAM_INT32, 1, &IsVisible );
    }

    TwSetEnabled( bar, "WhitePoint", m_PPAttribs.ToneMapping.iToneMappingMode == TONE_MAPPING_MODE_REINHARD_MOD ||
                                     m_PPAttribs.ToneMapping.iToneMappingMode == TONE_MAPPING_MODE_UNCHARTED2 ||
                                     m_PPAttribs.ToneMapping.iToneMappingMode == TONE_MAPPING_LOGARITHMIC ||
                                     m_PPAttribs.ToneMapping.iToneMappingMode == TONE_MAPPING_ADAPTIVE_LOG );
    
    TwSetEnabled( bar, "LumSaturation", m_PPAttribs.ToneMapping.iToneMappingMode == TONE_MAPPING_MODE_EXP ||
                                        m_PPAttribs.ToneMapping.iToneMappingMode == TONE_MAPPING_MODE_REINHARD ||
                                        m_PPAttribs.ToneMapping.iToneMappingMode == TONE_MAPPING_MODE_REINHARD_MOD ||
                                        m_PPAttribs.ToneMapping.iToneMappingMode == TONE_MAPPING_LOGARITHMIC ||
                                        m_PPAttribs.ToneMapping.iToneMappingMode == TONE_MAPPING_ADAPTIVE_LOG );
    TwSetEnabled( bar, "LightAdaptation", m_PPAttribs.ToneMapping.bAutoExposure ? true : false );
}

AtmosphereSample::~AtmosphereSample()
{
}

void AtmosphereSample::CreateShadowMap()
{
    ShadowMapManager::InitInfo SMMgrInitInfo;
    SMMgrInitInfo.Format      = m_TerrainRenderParams.ShadowMapFormat;
    SMMgrInitInfo.Resolution  = m_ShadowSettings.Resolution;
    SMMgrInitInfo.NumCascades = m_TerrainRenderParams.m_iNumShadowCascades;
    SMMgrInitInfo.ShadowMode  = SHADOW_MODE_PCF;

    if (!m_pComparisonSampler)
    {
        SamplerDesc ComparsionSampler;
        ComparsionSampler.ComparisonFunc = COMPARISON_FUNC_LESS; 
        // Note: anisotropic filtering requires SampleGrad to fix artifacts at 
        // cascade boundaries
        ComparsionSampler.MinFilter      = FILTER_TYPE_COMPARISON_LINEAR;
        ComparsionSampler.MagFilter      = FILTER_TYPE_COMPARISON_LINEAR;
        ComparsionSampler.MipFilter      = FILTER_TYPE_COMPARISON_LINEAR;
        m_pDevice->CreateSampler(ComparsionSampler, &m_pComparisonSampler);
    }
    SMMgrInitInfo.pComparisonSampler = m_pComparisonSampler;

    m_ShadowMapMgr.Initialize(m_pDevice, SMMgrInitInfo);
}

void AtmosphereSample::RenderShadowMap(IDeviceContext*  pContext,
                                       LightAttribs&    LightAttribs, 
                                       const float4x4&  mCameraView, 
                                       const float4x4&  mCameraProj)
{
    auto& ShadowAttribs = LightAttribs.ShadowAttribs;

    ShadowMapManager::DistributeCascadeInfo DistrInfo;
    DistrInfo.pCameraView = &mCameraView;
    DistrInfo.pCameraProj = &mCameraProj;
    DistrInfo.pLightDir   = &m_f3LightDir;
    DistrInfo.fPartitioningFactor = 0.95f;
    DistrInfo.SnapCascades        = true;
    DistrInfo.EqualizeExtents     = true;
    DistrInfo.StabilizeExtents    = true;
    DistrInfo.AdjustCascadeRange = 
        [this](int iCascade, float& MinZ, float& MaxZ)
        {
            if (iCascade < 0)
            {
                // Snap camera z range to the exponential scale
                const float pw = 1.1f;
                MinZ = std::pow(pw, std::floor(std::log(std::max(MinZ, 1.f))/std::log(pw)));
                MinZ = std::max(MinZ, 10.f);
                MaxZ = std::pow(pw, std::ceil(std::log(std::max(MaxZ, 1.f))/std::log(pw)));
            }
            else if (iCascade == m_PPAttribs.iFirstCascadeToRayMarch)
            {
                // Ray marching always starts at the camera position, not at the near plane.
                // So we must make sure that the first cascade used for ray marching covers the camera position
                MinZ = 10.f;
            }
        };
    m_ShadowMapMgr.DistributeCascades(DistrInfo, ShadowAttribs);

    // Render cascades
    for (int iCascade = 0; iCascade < m_TerrainRenderParams.m_iNumShadowCascades; ++iCascade)
    {
        auto* pCascadeDSV = m_ShadowMapMgr.GetCascadeDSV(iCascade);
        
        m_pImmediateContext->SetRenderTargets( 0, nullptr, pCascadeDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION );
        m_pImmediateContext->ClearDepthStencil( pCascadeDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION );

        const auto CascadeProjMatr = m_ShadowMapMgr.GetCascadeTranform(iCascade).Proj;

        auto WorldToLightViewSpaceMatr = ShadowAttribs.mWorldToLightViewT.Transpose();
        auto WorldToLightProjSpaceMatr = WorldToLightViewSpaceMatr * CascadeProjMatr;

        {
            MapHelper<CameraAttribs> CamAttribs( m_pImmediateContext, m_pcbCameraAttribs, MAP_WRITE, MAP_FLAG_DISCARD );
            CamAttribs->mViewProjT = WorldToLightProjSpaceMatr.Transpose();
        }

        m_EarthHemisphere.Render(m_pImmediateContext, m_TerrainRenderParams, m_f3CameraPos, WorldToLightProjSpaceMatr, nullptr, nullptr, nullptr, true);
    }

    pContext->SetRenderTargets( 0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION );
}


// Render a frame
void AtmosphereSample::Render()
{
    float4x4 mViewProj = m_mCameraView * m_mCameraProj;

    LightAttribs LightAttrs;
    LightAttrs.f4Direction = m_f3LightDir;
    LightAttrs.f4Direction.w = 0;

    float4 f4ExtraterrestrialSunColor = float4(10,10,10,10);
    LightAttrs.f4Intensity = f4ExtraterrestrialSunColor;// *m_fScatteringScale;
    LightAttrs.f4AmbientLight = float4( 0, 0, 0, 0 );

    LightAttrs.ShadowAttribs.iNumCascades = m_TerrainRenderParams.m_iNumShadowCascades;
    if (m_ShadowSettings.Resolution >= 2048)
        LightAttrs.ShadowAttribs.fFixedDepthBias = 0.0025f;
    else if (m_ShadowSettings.Resolution >= 1024)
        LightAttrs.ShadowAttribs.fFixedDepthBias = 0.0050f;
    else
        LightAttrs.ShadowAttribs.fFixedDepthBias = 0.0075f;

    // m_iFirstCascade must be initialized before calling RenderShadowMap()!
    m_PPAttribs.iFirstCascadeToRayMarch = std::min(m_PPAttribs.iFirstCascadeToRayMarch, m_TerrainRenderParams.m_iNumShadowCascades - 1);

	RenderShadowMap(m_pImmediateContext, LightAttrs, m_mCameraView, m_mCameraProj);
    
    LightAttrs.ShadowAttribs.bVisualizeCascades = m_ShadowSettings.bVisualizeCascades ? TRUE : FALSE;

    {
        MapHelper<LightAttribs> LightAttribsCBData( m_pImmediateContext, m_pcbLightAttribs, MAP_WRITE, MAP_FLAG_DISCARD );
        *LightAttribsCBData = LightAttrs;
    }

    // The first time GetAmbientSkyLightSRV() is called, the ambient sky light texture 
    // is computed and render target is set. So we need to query the texture before setting 
    // render targets
    auto *pAmbientSkyLightSRV = m_pLightSctrPP->GetAmbientSkyLightSRV(m_pDevice, m_pImmediateContext);

    m_pImmediateContext->SetRenderTargets( 0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION );

    const float ClearColor[] = {  0.350f,  0.350f,  0.350f, 1.0f }; 
    const float Zero[] = {  0.f,  0.f,  0.f, 0.f };
    m_pImmediateContext->ClearRenderTarget(nullptr, m_bEnableLightScattering ? Zero : ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    ITextureView *pRTV = nullptr, *pDSV = nullptr;
    if( m_bEnableLightScattering )
    {
        pRTV = m_pOffscreenColorBuffer->GetDefaultView( TEXTURE_VIEW_RENDER_TARGET );
        pDSV = m_pOffscreenDepthBuffer->GetDefaultView( TEXTURE_VIEW_DEPTH_STENCIL );
        m_pImmediateContext->SetRenderTargets( 1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION );
        m_pImmediateContext->ClearRenderTarget(pRTV, Zero, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }
    else
    {
        pRTV = nullptr;
        pDSV = nullptr;
        m_pImmediateContext->SetRenderTargets( 0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION );
    }
        
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    CameraAttribs CamAttribs;
    CamAttribs.mViewT        = m_mCameraView.Transpose();
    CamAttribs.mProjT        = m_mCameraProj.Transpose();
    CamAttribs.mViewProjT    = mViewProj.Transpose();
    CamAttribs.mViewProjInvT = mViewProj.Inverse().Transpose();
    float fNearPlane = 0.f, fFarPlane = 0.f;
    m_mCameraProj.GetNearFarClipPlanes(fNearPlane, fFarPlane, m_bIsGLDevice);
    CamAttribs.fNearPlaneZ = fNearPlane;
    CamAttribs.fFarPlaneZ = fFarPlane * 0.999999f;
    CamAttribs.f4Position = m_f3CameraPos;
    CamAttribs.f4ViewportSize.x = static_cast<float>(m_pSwapChain->GetDesc().Width);
    CamAttribs.f4ViewportSize.y = static_cast<float>(m_pSwapChain->GetDesc().Height);
    CamAttribs.f4ViewportSize.z = 1.f / CamAttribs.f4ViewportSize.x;
    CamAttribs.f4ViewportSize.w = 1.f / CamAttribs.f4ViewportSize.y;

    {
        MapHelper<CameraAttribs> CamAttribsCBData( m_pImmediateContext, m_pcbCameraAttribs, MAP_WRITE, MAP_FLAG_DISCARD );
        *CamAttribsCBData = CamAttribs;
    }    

    // Render terrain
    auto *pPrecomputedNetDensitySRV = m_pLightSctrPP->GetPrecomputedNetDensitySRV();
    m_TerrainRenderParams.DstRTVFormat = m_bEnableLightScattering ? m_pOffscreenColorBuffer->GetDesc().Format : m_pSwapChain->GetDesc().ColorBufferFormat;
    m_EarthHemisphere.Render( m_pImmediateContext, 
                              m_TerrainRenderParams, 
                              m_f3CameraPos, 
                              mViewProj, 
                              m_ShadowMapMgr.GetSRV(), 
                              pPrecomputedNetDensitySRV, 
                              pAmbientSkyLightSRV, 
                              false);
	
    if( m_bEnableLightScattering )
    {
        EpipolarLightScattering::FrameAttribs FrameAttribs;

        FrameAttribs.pDevice        = m_pDevice;
        FrameAttribs.pDeviceContext = m_pImmediateContext;
        FrameAttribs.dElapsedTime   = m_fElapsedTime;
        FrameAttribs.pLightAttribs  = &LightAttrs;
        FrameAttribs.pCameraAttribs = &CamAttribs;

        m_PPAttribs.iNumCascades = m_TerrainRenderParams.m_iNumShadowCascades;
        m_PPAttribs.fNumCascades = (float)m_TerrainRenderParams.m_iNumShadowCascades;

        FrameAttribs.pcbLightAttribs  = m_pcbLightAttribs;
        FrameAttribs.pcbCameraAttribs = m_pcbCameraAttribs;

        m_PPAttribs.fMaxShadowMapStep = static_cast<float>(m_ShadowSettings.Resolution / 4);
        
        m_PPAttribs.f2ShadowMapTexelSize = float2( 1.f / static_cast<float>(m_ShadowSettings.Resolution), 1.f / static_cast<float>(m_ShadowSettings.Resolution) );
        m_PPAttribs.uiMaxSamplesOnTheRay = m_ShadowSettings.Resolution;
        // During the ray marching, on each step we move by the texel size in either horz 
        // or vert direction. So resolution of min/max mipmap should be the same as the 
        // resolution of the original shadow map
        m_PPAttribs.uiMinMaxShadowMapResolution = m_ShadowSettings.Resolution;
        m_PPAttribs.uiInitialSampleStepInSlice = std::min( m_PPAttribs.uiInitialSampleStepInSlice, m_PPAttribs.uiMaxSamplesInSlice );
        m_PPAttribs.uiEpipoleSamplingDensityFactor = std::min( m_PPAttribs.uiEpipoleSamplingDensityFactor, m_PPAttribs.uiInitialSampleStepInSlice );

        FrameAttribs.ptex2DSrcColorBufferSRV = m_pOffscreenColorBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        FrameAttribs.ptex2DSrcColorBufferRTV = m_pOffscreenColorBuffer->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);
        FrameAttribs.ptex2DSrcDepthBufferSRV = m_pOffscreenDepthBuffer->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
        FrameAttribs.ptex2DSrcDepthBufferDSV = m_pOffscreenDepthBuffer->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);
        FrameAttribs.ptex2DShadowMapSRV      = m_ShadowMapMgr.GetSRV();
        FrameAttribs.pDstRTV                 = nullptr;// mpBackBufferRTV;

        // Perform the post processing
        m_pLightSctrPP->PerformPostProcessing(FrameAttribs, m_PPAttribs);
    }
}


void GetRaySphereIntersection(float3 f3RayOrigin,
                              const float3 &f3RayDirection,
                              const float3 &f3SphereCenter,
                              float fSphereRadius,
                              float2 &f2Intersections)
{
    // http://wiki.cgsociety.org/index.php/Ray_Sphere_Intersection
    f3RayOrigin -= f3SphereCenter;
    float A = dot(f3RayDirection, f3RayDirection);
    float B = 2 * dot(f3RayOrigin, f3RayDirection);
    float C = dot(f3RayOrigin, f3RayOrigin) - fSphereRadius*fSphereRadius;
    float D = B*B - 4*A*C;
    // If discriminant is negative, there are no real roots hence the ray misses the
    // sphere
    if( D<0 )
    {
        f2Intersections = float2(-1,-1);
    }
    else
    {
        D = sqrt(D);
        f2Intersections = float2(-B - D, -B + D) / (2*A); // A must be positive here!!
    }
}

void ComputeApproximateNearFarPlaneDist(const float3 &CameraPos,
                                        const float4x4 &ViewMatr,
                                        const float4x4 &ProjMatr, 
                                        const float3 &EarthCenter,
                                        float fEarthRadius,
                                        float fMinRadius,
                                        float fMaxRadius,
                                        float &fNearPlaneZ,
                                        float &fFarPlaneZ)
{
    float4x4 ViewProjMatr = ViewMatr * ProjMatr;
    float4x4 ViewProjInv = ViewProjMatr.Inverse();
    
    // Compute maximum view distance for the current camera altitude
    float3 f3CameraGlobalPos = CameraPos - EarthCenter;
    float fCameraElevationSqr = dot(f3CameraGlobalPos, f3CameraGlobalPos);
    float fMaxViewDistance = (float)(sqrt( (double)fCameraElevationSqr - (double)fEarthRadius*fEarthRadius ) + 
                                     sqrt( (double)fMaxRadius*fMaxRadius - (double)fEarthRadius*fEarthRadius ));
    float fCameraElev = sqrt(fCameraElevationSqr);

    fNearPlaneZ = 50.f;
    if( fCameraElev > fMaxRadius )
    {
        // Adjust near clipping plane
        fNearPlaneZ = (fCameraElev - fMaxRadius) / sqrt( 1 + 1.f/(ProjMatr._11*ProjMatr._11) + 1.f/(ProjMatr._22*ProjMatr._22) );
    }

    fNearPlaneZ = std::max(fNearPlaneZ, 50.f);
    fFarPlaneZ = 1000;
    
    const int iNumTestDirections = 5;
    for(int i=0; i<iNumTestDirections; ++i)
    {
        for(int j=0; j<iNumTestDirections; ++j)
        {
            float3 PosPS, PosWS, DirFromCamera;
            PosPS.x = (float)i / (float)(iNumTestDirections-1) * 2.f - 1.f;
            PosPS.y = (float)j / (float)(iNumTestDirections-1) * 2.f - 1.f;
            PosPS.z = 0; // Far plane is at 0 in complimentary depth buffer
            PosWS = PosPS * ViewProjInv;

            DirFromCamera = PosWS - CameraPos;
            DirFromCamera = normalize(DirFromCamera);

            float2 IsecsWithBottomBoundSphere;
            GetRaySphereIntersection(CameraPos, DirFromCamera, EarthCenter, fMinRadius, IsecsWithBottomBoundSphere);

            float fNearIsecWithBottomSphere = IsecsWithBottomBoundSphere.x > 0 ? IsecsWithBottomBoundSphere.x : IsecsWithBottomBoundSphere.y;
            if( fNearIsecWithBottomSphere > 0 )
            {
                // The ray hits the Earth. Use hit point to compute camera space Z
                float3 HitPointWS = CameraPos + DirFromCamera*fNearIsecWithBottomSphere;
                float3 HitPointCamSpace;
                HitPointCamSpace = HitPointWS * ViewMatr;
                fFarPlaneZ = std::max(fFarPlaneZ, HitPointCamSpace.z);
            }
            else
            {
                // The ray misses the Earth. In that case the whole earth could be seen
                fFarPlaneZ = fMaxViewDistance;
            }
        }
    }
}


void AtmosphereSample::Update(double CurrTime, double ElapsedTime)
{
    const auto& mouseState = m_InputController.GetMouseState();
    float MouseDeltaX = 0;
    float MouseDeltaY = 0;
    if (m_LastMouseState.PosX >=0 && m_LastMouseState.PosY >= 0 &&
        m_LastMouseState.ButtonFlags != MouseState::BUTTON_FLAG_NONE)
    {
        MouseDeltaX = mouseState.PosX - m_LastMouseState.PosX;
        MouseDeltaY = mouseState.PosY - m_LastMouseState.PosY;
    }
    m_LastMouseState = mouseState;

    if ((m_LastMouseState.ButtonFlags & MouseState::BUTTON_FLAG_LEFT) != 0)
    {
        constexpr float CameraRotationSpeed = 0.005f;
        m_fCameraYaw   += MouseDeltaX * CameraRotationSpeed;
        m_fCameraPitch += MouseDeltaY * CameraRotationSpeed;
    }
    m_CameraRotation =
        Quaternion::RotationFromAxisAngle(float3{1,0,0}, -m_fCameraPitch) *
        Quaternion::RotationFromAxisAngle(float3{0,1,0}, -m_fCameraYaw);
    m_f3CameraPos.y += mouseState.WheelDelta * 500.f;
    m_f3CameraPos.y = std::max(m_f3CameraPos.y, 2000.f);
    m_f3CameraPos.y = std::min(m_f3CameraPos.y, 100000.f);

    auto CameraRotationMatrix = m_CameraRotation.ToMatrix();

    if ((m_LastMouseState.ButtonFlags & MouseState::BUTTON_FLAG_RIGHT) != 0)
    {
        constexpr float LightRotationSpeed = 0.001f;
        float fYawDelta   = MouseDeltaX * LightRotationSpeed;
        float fPitchDelta = MouseDeltaY * LightRotationSpeed;
        float3 WorldUp   {CameraRotationMatrix._12, CameraRotationMatrix._22, CameraRotationMatrix._32};
        float3 WorldRight{CameraRotationMatrix._11, CameraRotationMatrix._21, CameraRotationMatrix._31};
        m_f3LightDir = float4(m_f3LightDir, 0) *
                        float4x4::RotationArbitrary(WorldUp,    fYawDelta) * 
                        float4x4::RotationArbitrary(WorldRight, fPitchDelta);
    }

    SampleBase::Update(CurrTime, ElapsedTime);

    m_fElapsedTime = static_cast<float>(ElapsedTime);

    const auto& SCDesc = m_pSwapChain->GetDesc();
    // Set world/view/proj matrices and global shader constants
    float aspectRatio = (float)SCDesc.Width / SCDesc.Height;

    m_mCameraView =
        float4x4::Translation(-m_f3CameraPos) *
        CameraRotationMatrix;

    // This projection matrix is only used to set up directions in view frustum
    // Actual near and far planes are ignored
    float FOV = PI_F/4.f;
    float4x4 mTmpProj = float4x4::Projection(FOV, aspectRatio, 50.f, 500000.f, m_bIsGLDevice);

    float fEarthRadius = AirScatteringAttribs().fEarthRadius;
    float3 EarthCenter(0, -fEarthRadius, 0);
    float fNearPlaneZ, fFarPlaneZ;
    ComputeApproximateNearFarPlaneDist(m_f3CameraPos,
                                       m_mCameraView,
                                       mTmpProj,
                                       EarthCenter,
                                       fEarthRadius,
                                       fEarthRadius + m_fMinElevation,
                                       fEarthRadius + m_fMaxElevation,
                                       fNearPlaneZ,
                                       fFarPlaneZ);
    fNearPlaneZ = std::max(fNearPlaneZ, 50.f);
    fFarPlaneZ  = std::max(fFarPlaneZ, fNearPlaneZ+100.f);
    fFarPlaneZ  = std::max(fFarPlaneZ, 1000.f);

    m_mCameraProj = float4x4::Projection(FOV, aspectRatio, fNearPlaneZ, fFarPlaneZ, m_bIsGLDevice);

#if 0
    if( m_bAnimateSun )
    {
        auto &LightOrientationMatrix = *m_pDirLightOrienationCamera->GetParentMatrix();
        float3 RotationAxis( 0.5f, 0.3f, 0.0f );
        float3 LightDir = m_pDirLightOrienationCamera->GetLook() * -1;
        float fRotationScaler = ( LightDir.y > +0.2f ) ? 50.f : 1.f;
        float4x4 RotationMatrix = float4x4RotationAxis(RotationAxis, 0.02f * (float)deltaSeconds * fRotationScaler);
        LightOrientationMatrix = LightOrientationMatrix * RotationMatrix;
        m_pDirLightOrienationCamera->SetParentMatrix(LightOrientationMatrix);
    }

    float dt = (float)ElapsedTime;
    if (m_Animate && dt > 0 && dt < 0.2f)
    {
        float3 axis;
        float angle = 0;
        AxisAngleFromRotation(axis, angle, m_Rotation);
        if (length(axis) < 1.0e-6f) 
            axis[1] = 1;
        angle += m_AnimationSpeed * dt;
        if (angle >= 2.0f*FLOAT_PI)
            angle -= 2.0f*FLOAT_PI;
        else if (angle <= 0)
            angle += 2.0f*FLOAT_PI;
        m_Rotation = RotationFromAxisAngle(axis, angle);
    }
#endif
    UpdateGUI();
}

void AtmosphereSample :: WindowResize( Uint32 Width, Uint32 Height )
{
    m_pLightSctrPP->OnWindowResize( m_pDevice, Width, Height );
    // Flush is required because Intel driver does not release resources until
    // command buffer is flushed. When window is resized, WindowResize() is called for
    // every intermediate window size, and light scattering object creates resources
    // for the new size. This resources are then released by the light scattering object, but 
    // not by Intel driver, which results in memory exhaustion.
    m_pImmediateContext->Flush();

    m_pOffscreenColorBuffer.Release();
    m_pOffscreenDepthBuffer.Release();

    TextureDesc ColorBuffDesc;
    ColorBuffDesc.Name = "Offscreen color buffer";
    ColorBuffDesc.Type = RESOURCE_DIM_TEX_2D;
    ColorBuffDesc.Width  = Width;
    ColorBuffDesc.Height = Height;
    ColorBuffDesc.MipLevels = 1;
    ColorBuffDesc.Format = TEX_FORMAT_R11G11B10_FLOAT;
    ColorBuffDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_RENDER_TARGET;
    m_pDevice->CreateTexture( ColorBuffDesc, nullptr, &m_pOffscreenColorBuffer );

    TextureDesc DepthBuffDesc = ColorBuffDesc;
	DepthBuffDesc.Name = "Offscreen depth buffer";
    DepthBuffDesc.Format = TEX_FORMAT_D32_FLOAT;
    DepthBuffDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_DEPTH_STENCIL;
    m_pDevice->CreateTexture( DepthBuffDesc, nullptr, &m_pOffscreenDepthBuffer );
}

}
