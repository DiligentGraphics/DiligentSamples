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

#include "Terrain.hpp"
#include "Buildings.hpp"
#include "Profiler.hpp"

namespace Diligent
{

class Tutorial23_CommandQueues final : public SampleBase
{
public:
    ~Tutorial23_CommandQueues() override;

    virtual void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override final;
    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "Tutorial23: Command queues"; }

    virtual void WindowResize(Uint32 Width, Uint32 Height) override final;

private:
    void UpdateUI();
    void CreatePostProcessPSO(IShaderSourceInputStreamFactory* pShaderSourceFactory);
    void DownSample();
    void PostProcess();

    void ComputePass();
    void UploadPass();
    void GraphicsPass1();
    void GraphicsPass2();

    Uint32 GetCpuToGpuTransferRateMb() const { return m_TransferRateMbExp2 ? 1u << m_TransferRateMbExp2 : 0u; }

    Uint32 ScaleSurface(Uint32 Dim) const
    {
        float Scale = m_SurfaceScaleExp2 >= 0 ? static_cast<float>(1u << m_SurfaceScaleExp2) : 1.f / static_cast<float>(1u << -m_SurfaceScaleExp2);
        return static_cast<Uint32>(Dim * Scale + 0.5f);
    }

    RefCntAutoPtr<IBuffer> m_DrawConstants;
    RefCntAutoPtr<IBuffer> m_PostProcessConstants;

    Terrain   m_Terrain;
    Buildings m_Buildings;

    // Post-processing PSO and SRB
    RefCntAutoPtr<IPipelineState>         m_PostProcessPSO[2];
    RefCntAutoPtr<IShaderResourceBinding> m_PostProcessSRB;

    RefCntAutoPtr<IPipelineState>         m_DownSamplePSO;
    static constexpr Uint32               DownSampleFactor = 5;
    RefCntAutoPtr<IShaderResourceBinding> m_DownSampleSRB[DownSampleFactor];

    FirstPersonCamera m_Camera;

    RefCntAutoPtr<IDeviceContext> m_ComputeCtx; // or second graphics on mobile GPUs
    RefCntAutoPtr<IDeviceContext> m_TransferCtx;

    RefCntAutoPtr<IFence> m_GraphicsCtxFence;
    RefCntAutoPtr<IFence> m_ComputeCtxFence;
    RefCntAutoPtr<IFence> m_TransferCtxFence;

    Uint64 m_GraphicsCtxFenceValue = 0;
    Uint64 m_ComputeCtxFenceValue  = 0;
    Uint64 m_TransferCtxFenceValue = 0;

    struct GBuffer
    {
        RefCntAutoPtr<ITexture>     Color;
        RefCntAutoPtr<ITextureView> ColorRTVs[DownSampleFactor];
        RefCntAutoPtr<ITextureView> ColorSRBs[DownSampleFactor];
        RefCntAutoPtr<ITexture>     Depth;
    };
    GBuffer m_GBuffer;

    TEXTURE_FORMAT m_ColorTargetFormat = TEX_FORMAT_RGBA8_UNORM;
    TEXTURE_FORMAT m_DepthTargetFormat = TEX_FORMAT_UNKNOWN;

    int          m_TransferRateMbExp2 = 2; // two to the power of
    bool         m_UseAsyncCompute    = false;
    bool         m_UseAsyncTransfer   = false;
    bool         m_Glow               = true;
    float3       m_LightDir           = normalize(float3{-0.49f, -0.60f, 0.64f});
    const float  m_AmbientLight       = 0.1f;
    const float3 m_FogColor           = {0.73f, 0.65f, 0.59f};
    const float3 m_SkyColor           = {0.7f, 0.5f, 0.2f};
    int          m_SurfaceScaleExp2   = 0; // two to the power of

    std::vector<ImmediateContextCreateInfo> m_ContextCI;

    Profiler m_Profiler;
};

} // namespace Diligent
