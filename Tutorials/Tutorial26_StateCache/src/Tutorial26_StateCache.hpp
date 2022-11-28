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

#include <string>
#include <memory>

#include "SampleBase.hpp"
#include "BasicMath.hpp"
#include "FirstPersonCamera.hpp"
#include "RenderStateNotationLoader.h"
#include "RenderStateCache.h"

namespace Diligent
{

namespace
{
namespace HLSL
{
struct SceneAttribs;
}
} // namespace

class Tutorial26_StateCache final : public SampleBase
{
public:
    virtual void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override final;

    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "Tutorial26: Render State Cache"; }

    virtual void WindowResize(Uint32 Width, Uint32 Height) override final;

    Tutorial26_StateCache();
    ~Tutorial26_StateCache();

private:
    void UpdateUI();
    void CreateGBuffer();
    void CreatePathTracePSO();

    RefCntAutoPtr<IRenderStateNotationParser> m_pRSNParser;
    RefCntAutoPtr<IRenderStateNotationLoader> m_pRSNLoader;
    RefCntAutoPtr<IRenderStateCache>          m_pStateCache;

    RefCntAutoPtr<IBuffer> m_pShaderConstantsCB;

    RefCntAutoPtr<IPipelineState>         m_pGBufferPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pGBufferSRB;
    RefCntAutoPtr<IPipelineState>         m_pPathTracePSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pPathTraceSRB;
    RefCntAutoPtr<IPipelineState>         m_pResolvePSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pResolveSRB;

    std::string m_StateCachePath;

    struct GBuffer
    {
        explicit operator bool() const
        {
            return pBaseColor && pNormal && pEmittance && pPhysDesc && pDepth;
        }

        static constexpr auto BaseColorFormat = TEX_FORMAT_RGBA8_UNORM;
        static constexpr auto NormalFormat    = TEX_FORMAT_RGBA8_UNORM;
        static constexpr auto EmittanceFormat = TEX_FORMAT_R11G11B10_FLOAT;
        static constexpr auto PhysDescFormat  = TEX_FORMAT_RG8_UNORM; // Metallic-roughness
        static constexpr auto DepthFormat     = TEX_FORMAT_R32_FLOAT; // 16-bit is not enough

        RefCntAutoPtr<ITexture> pBaseColor;
        RefCntAutoPtr<ITexture> pNormal;
        RefCntAutoPtr<ITexture> pEmittance;
        RefCntAutoPtr<ITexture> pPhysDesc;
        RefCntAutoPtr<ITexture> pDepth;
    };
    GBuffer m_GBuffer;

    static constexpr auto   RadianceAccumulationFormat = TEX_FORMAT_RGBA32_FLOAT;
    RefCntAutoPtr<ITexture> m_pRadianceAccumulationBuffer;

    enum BRDF_SAMPLING_MODE
    {
        BRDF_SAMPLING_MODE_COS_WEIGHTED = 0,
        BRDF_SAMPLING_MODE_IMPORTANCE_SAMPLING
    };
    int m_BRDFSamplingMode = BRDF_SAMPLING_MODE_IMPORTANCE_SAMPLING;

    enum NEE_MODE
    {
        NEE_MODE_LIGHT,     // Sample light
        NEE_MODE_BRDF,      // Sample BRDF
        NEE_MODE_MIS,       // Multiple importance sampling
        NEE_MODE_MIS_LIGHT, // MIS - light component
        NEE_MODE_MIS_BRDF   // MIS - BRDF component
    };
    int m_NEEMode = NEE_MODE_MIS;

    int   m_NumBounces             = 4;
    int   m_NumSamplesPerFrame     = 4;
    bool  m_ShowOnlyLastBounce     = false;
    bool  m_UseNEE                 = true;
    bool  m_FullBRDFReflectance    = false;
    float m_BalanceHeuristicsPower = 2;

    int  m_SampleCount      = 0;
    bool m_LimitSampleCount = false;
    int  m_MaxSamples       = 1024;

    float4x4 m_LastFrameViewProj;

    FirstPersonCamera m_Camera;
    MouseState        m_LastMouseState;

    std::unique_ptr<HLSL::SceneAttribs> m_Scene;
};

} // namespace Diligent
