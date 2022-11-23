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

#include "SampleBase.hpp"
#include "BasicMath.hpp"
#include "FirstPersonCamera.hpp"

namespace Diligent
{

class Tutorial25_StatePackager final : public SampleBase
{
public:
    virtual void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override final;

    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "Tutorial25: Render State Packager"; }

    virtual void WindowResize(Uint32 Width, Uint32 Height) override final;

private:
    void UpdateUI();
    void CreateGBuffer();

    RefCntAutoPtr<IBuffer> m_pShaderConstantsCB;

    RefCntAutoPtr<IPipelineState>         m_pGBufferPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pGBufferSRB;
    RefCntAutoPtr<IPipelineState>         m_pPathTracePSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pPathTraceSRB;
    RefCntAutoPtr<IPipelineState>         m_pResolvePSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pResolveSRB;

    struct GBuffer
    {
        explicit operator bool() const
        {
            return pAlbedo && pNormal && pEmittance && pDepth;
        }

        static constexpr auto AlbedoFormat    = TEX_FORMAT_RGBA8_UNORM;
        static constexpr auto NormalFormat    = TEX_FORMAT_RGBA8_UNORM;
        static constexpr auto DepthFormat     = TEX_FORMAT_R32_FLOAT; // 16-bit is not enough
        static constexpr auto EmittanceFormat = TEX_FORMAT_R11G11B10_FLOAT;

        RefCntAutoPtr<ITexture> pAlbedo;
        RefCntAutoPtr<ITexture> pNormal;
        RefCntAutoPtr<ITexture> pEmittance;
        RefCntAutoPtr<ITexture> pDepth;
    };
    GBuffer m_GBuffer;

    static constexpr auto   RadianceAccumulationFormat = TEX_FORMAT_RGBA32_FLOAT;
    RefCntAutoPtr<ITexture> m_pRadianceAccumulationBuffer;

    int    m_NumBounces         = 3;
    int    m_NumSamplesPerFrame = 8;
    bool   m_ShowOnlyLastBounce = false;
    bool   m_UseNEE             = true;
    float3 m_LightColor         = {1, 1, 1};
    float  m_LightIntensity     = 15.f;
    float2 m_LightPos           = {0, 0};
    float2 m_LightSize          = {1.5f, 1.5f};

    int      m_SampleCount = 0;
    float4x4 m_LastFrameViewProj;

    FirstPersonCamera m_Camera;
    MouseState        m_LastMouseState;
};

} // namespace Diligent
