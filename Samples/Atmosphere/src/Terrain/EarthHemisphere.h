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

#include <vector>

#include "RenderDevice.h"
#include "DeviceContext.h"
#include "Buffer.h"
#include "Texture.h"
#include "BufferView.h"
#include "TextureView.h"
#include "GraphicsTypes.h"
#include "RefCntAutoPtr.h"

#include "AdvancedMath.h"
using Diligent::float4;
using Diligent::float4x4;

namespace Diligent
{
#include "../../assets/shaders/HostSharedTerrainStructs.fxh"
}

// Structure describing terrain rendering parameters
struct RenderingParams
{
    Diligent::TerrainAttribs m_TerrainAttribs;

    enum TEXTURING_MODE
    {
        TM_HEIGHT_BASED = 0,
		TM_MATERIAL_MASK = 1,
        TM_MATERIAL_MASK_NM = 2
    };
    
    // Patch rendering params
    TEXTURING_MODE m_TexturingMode;
    int m_iRingDimension;
    int m_iNumRings;

    int m_iNumShadowCascades;
    int m_bBestCascadeSearch;
    int m_bSmoothShadows;
    int m_iColOffset, m_iRowOffset;
    Diligent::TEXTURE_FORMAT DstRTVFormat;

    RenderingParams() : 
		m_TexturingMode     (TM_MATERIAL_MASK_NM),
        m_iRingDimension    (65),
        m_iNumRings         (15),
        m_iNumShadowCascades(6),
        m_bBestCascadeSearch(1),
        m_bSmoothShadows    (1),
        m_iColOffset        (1356), 
        m_iRowOffset        (924),
        DstRTVFormat        (Diligent::TEX_FORMAT_R11G11B10_FLOAT)
	{}
};

struct RingSectorMesh
{
    Diligent::RefCntAutoPtr<Diligent::IBuffer> pIndBuff;
    Diligent::Uint32 uiNumIndices;
    Diligent::BoundBox BndBox;
    RingSectorMesh() : uiNumIndices(0){}
};

// This class renders the adaptive model using DX11 API
class EarthHemsiphere
{
public:
    EarthHemsiphere(void) : m_ValidShaders(0){}
    
    EarthHemsiphere             (const EarthHemsiphere&) = delete;
    EarthHemsiphere& operator = (const EarthHemsiphere&) = delete;
    EarthHemsiphere             (EarthHemsiphere&&)      = delete;
    EarthHemsiphere& operator = (EarthHemsiphere&&)      = delete;

    // Renders the model
	void Render(Diligent::IDeviceContext*   pContext,
                const RenderingParams&      NewParams,
                const Diligent::float3&     vCameraPosition, 
                const Diligent::float4x4&   CameraViewProjMatrix,
                Diligent::ITextureView*     pShadowMapSRV,
                Diligent::ITextureView*     pPrecomputedNetDensitySRV,
                Diligent::ITextureView*     pAmbientSkylightSRV,
                bool bZOnlyPass);

    // Creates device resources
    void Create(class ElevationDataSource* pDataSource,
                const RenderingParams&     Params,
                Diligent::IRenderDevice*   pDevice,
                Diligent::IDeviceContext*  pContext,
                const char*                MaterialMaskPath,
				const char*                TileTexturePath[],
                const char*                TileNormalMapPath[],
                Diligent::IBuffer*         pcbCameraAttribs,
                Diligent::IBuffer*         pcbLightAttribs,
                Diligent::IBuffer*         pcMediaScatteringParams);

    enum {NUM_TILE_TEXTURES = 1 + 4};// One base material + 4 masked materials

private:

    void RenderNormalMap(Diligent::IRenderDevice*  pd3dDevice,
                         Diligent::IDeviceContext* pd3dImmediateContext,
                         const Diligent::Uint16*   pHeightMap,
                         size_t                    HeightMapPitch,
                         int                       HeightMapDim,
                         Diligent::ITexture*       ptex2DNormalMap);

    RenderingParams m_Params;

    Diligent::RefCntAutoPtr<Diligent::IRenderDevice> m_pDevice;

    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pcbTerrainAttribs;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_pVertBuff;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_ptex2DNormalMapSRV, m_ptex2DMtrlMaskSRV;
        
	Diligent::RefCntAutoPtr<Diligent::ITextureView> m_ptex2DTilesSRV[NUM_TILE_TEXTURES];
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_ptex2DTilNormalMapsSRV[NUM_TILE_TEXTURES];

    Diligent::RefCntAutoPtr<Diligent::IResourceMapping> m_pResMapping;
    Diligent::RefCntAutoPtr<Diligent::IShader> m_pHemisphereVS;
    
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_pHemisphereZOnlyPSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pHemisphereZOnlySRB;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState>         m_pHemispherePSO;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_pHemisphereSRB;
    Diligent::RefCntAutoPtr<Diligent::ISampler> m_pComparisonSampler;

    std::vector<RingSectorMesh> m_SphereMeshes;
    
    Diligent::Uint32 m_ValidShaders;
};
