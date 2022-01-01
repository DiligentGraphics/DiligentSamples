/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#include <utility>
#include <vector>

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

    virtual const Char* GetSampleName() const override final { return "Tutorial24: Variable rate shading"; }

    virtual void WindowResize(Uint32 Width, Uint32 Height) override final;

private:
    void UpdateUI();
    void LoadTexture();
    void CreateVRSPipelineState(IShaderSourceInputStreamFactory* pShaderSourceFactory);        // For desktop D3D12 and Vulkan and Metal
    void CreateDensityMapPipelineState(IShaderSourceInputStreamFactory* pShaderSourceFactory); // For mobile Vulkan only
    void CreateBlitPipelineState(IShaderSourceInputStreamFactory* pShaderSourceFactory);
    void UpdateVRSPattern(float2 MPos);

    float GetSurfaceScale() const
    {
        return m_SurfaceScaleExp2 >= 0 ? static_cast<float>(1u << m_SurfaceScaleExp2) : 1.f / static_cast<float>(1u << -m_SurfaceScaleExp2);
    }

    Uint32 ScaleSurface(Uint32 Dim) const
    {
        return static_cast<Uint32>(Dim * GetSurfaceScale());
    }

    static const TEXTURE_FORMAT ColorFormat = TEX_FORMAT_RGBA8_UNORM;
    static const TEXTURE_FORMAT DepthFormat = TEX_FORMAT_D32_FLOAT;

    enum VRS_MODE : Uint32
    {
        VRS_MODE_PER_DRAW      = 0,
        VRS_MODE_PER_PRIMITIVE = 1,
        VRS_MODE_TEXTURE_BASED = 2,
        VRS_MODE_COUNT
    };

    struct
    {
        RefCntAutoPtr<IShaderResourceBinding> SRB;
        RefCntAutoPtr<IPipelineState>         PSO[VRS_MODE_COUNT];
    } m_VRS;

    // Cube resources
    RefCntAutoPtr<IBuffer>      m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer>      m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer>      m_Constants;
    RefCntAutoPtr<ITextureView> m_TextureSRV;

#if PLATFORM_MACOS || PLATFORM_IOS
    RefCntAutoPtr<IBuffer> m_pShadingRateParamBuffer;
#endif
    RefCntAutoPtr<ITextureView>           m_pShadingRateMap;
    RefCntAutoPtr<ITextureView>           m_pRTV;
    RefCntAutoPtr<ITextureView>           m_pDSV;
    float2                                m_PrevNormMPos{0.5f};
    RefCntAutoPtr<IShaderResourceBinding> m_BlitSRB;
    RefCntAutoPtr<IPipelineState>         m_BlitPSO;

    int  m_SurfaceScaleExp2 = 0;
    bool m_ShowShadingRate  = true;
    bool m_Animation        = false;

    // Supported VRS modes ((mode, name) pairs)
    std::vector<std::pair<VRS_MODE, const char*>> m_VRSModes;

    // Supported shading rates for per draw mode ((rate, name) pairs)
    std::vector<std::pair<SHADING_RATE, const char*>> m_ShadingRates;

    VRS_MODE     m_VRSMode     = VRS_MODE_TEXTURE_BASED;
    SHADING_RATE m_ShadingRate = SHADING_RATE_1X1;

    float    m_fCurrentTime = 0.f;
    float4x4 m_WorldViewProjMatrix;
};

} // namespace Diligent
