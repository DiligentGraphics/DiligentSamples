/*
 *  Copyright 2019-2021 Diligent Graphics LLC
 *  
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *  
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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

#include "SampleBase.hpp"
#include "BasicMath.hpp"
#include "FirstPersonCamera.hpp"

namespace Diligent
{

class Tutorial24_VRS final : public SampleBase
{
public:
    virtual void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override final;
    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "Tutorial24: Variable shading rate"; }

    virtual void WindowResize(Uint32 Width, Uint32 Height) override final;

private:
    void UpdateUI();
    void LoadTexture();
    void CreateVRSPipelineState(IShaderSourceInputStreamFactory* pShaderSourceFactory);        // for desktop D3D12 and Vulkan and Metal
    void CreateDensityMapPipelineState(IShaderSourceInputStreamFactory* pShaderSourceFactory); // only for mobile Vulkan
    void CreateBlitPipelineState(IShaderSourceInputStreamFactory* pShaderSourceFactory);
    void UpdateVRSPattern(float MPosX, float MPosY, Uint32 Width, Uint32 Height);

    float GetSurfaceScale() const
    {
        return m_SurfaceScalePOT >= 0 ? static_cast<float>(1u << m_SurfaceScalePOT) : 1.f / static_cast<float>(1u << -m_SurfaceScalePOT);
    }

    Uint32 ScaleSurface(Uint32 Dim) const
    {
        return static_cast<Uint32>(Dim * GetSurfaceScale() + 0.5f);
    }

    static constexpr int        VRSModes        = 3;
    static constexpr int        MaxShadingRates = SHADING_RATE_MAX;
    static const TEXTURE_FORMAT ColorFormat     = TEX_FORMAT_RGBA8_UNORM;
    static const TEXTURE_FORMAT DepthFormat     = TEX_FORMAT_D32_FLOAT;

    enum VRS_MODE : Uint32
    {
        VRS_MODE_PER_DRAW      = 0,
        VRS_MODE_PER_PRIMITIVE = 1,
        VRS_MODE_TEXTURE_BASED = 2,
    };

    struct
    {
        RefCntAutoPtr<IShaderResourceBinding> SRB;
        RefCntAutoPtr<IPipelineState>         PSO[VRSModes];
    } m_VRS;

    // Cube resources
    RefCntAutoPtr<IBuffer>      m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer>      m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer>      m_Constants;
    RefCntAutoPtr<ITextureView> m_TextureSRV;

#if METAL_SUPPORTED
    RefCntAutoPtr<IDeviceObject> m_pShadingRateMap;
    RefCntAutoPtr<IBuffer>       m_pShadingRateParamBuffer;
#else
    RefCntAutoPtr<ITextureView> m_pShadingRateMap;
#endif
    RefCntAutoPtr<ITextureView>           m_pRTV;
    RefCntAutoPtr<ITextureView>           m_pDSV;
    float2                                m_PrevMPos;
    RefCntAutoPtr<IShaderResourceBinding> m_BlitSRB;
    RefCntAutoPtr<IPipelineState>         m_BlitPSO;

    int  m_SurfaceScalePOT = 0;
    int  m_VRSMode         = 0;
    bool m_ShowShadingRate = false;
    bool m_Animation       = true;

    // Supported VRS modes
    int         m_NumVRSModes            = 0;
    const char* m_VRSModeNames[VRSModes] = {};
    VRS_MODE    m_VRSModeList[VRSModes]  = {};

    // Supported shading rates for per draw mode
    const char*  m_ShadingRateNames[MaxShadingRates] = {};
    SHADING_RATE m_ShadingRates[MaxShadingRates]     = {};
    int          m_NumShadingRates                   = 0;
    int          m_ShadingRateIndex                  = 0;

    float4x4 m_WorldViewProjMatrix;
};

} // namespace Diligent
