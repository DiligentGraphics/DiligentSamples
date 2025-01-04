/*
 *  Copyright 2024-2025 Diligent Graphics LLC
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

#include <array>

#include "SampleBase.hpp"
#include "BasicMath.hpp"

namespace Diligent
{

class Tutorial29_OIT final : public SampleBase
{
public:
    virtual void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override final;
    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "Tutorial29: Order-Independent Transparency"; }

    virtual void WindowResize(Uint32 Width, Uint32 Height) override final;

private:
    void CreatePipelineStates();
    void CreateGeometryBuffers();
    void PrepareOITResources();
    void UpdateUI();
    void CreateInstanceBuffers();
    void RenderGrid(bool IsTransparent, IPipelineState* pPSO, IShaderResourceBinding* pSRB);

    RefCntAutoPtr<IBuffer>  m_VertexBuffer;
    RefCntAutoPtr<IBuffer>  m_IndexBuffer;
    RefCntAutoPtr<IBuffer>  m_Constants;
    RefCntAutoPtr<IBuffer>  m_OITLayers;
    RefCntAutoPtr<ITexture> m_ColorBuffer;
    RefCntAutoPtr<ITexture> m_DepthBuffer;

    std::array<RefCntAutoPtr<IBuffer>, 2> m_InstanceBuffer; // 0 - opaque, 1 - transparent

    static constexpr TEXTURE_FORMAT TailTransmittanceFormat = TEX_FORMAT_RGBA8_UNORM;
    RefCntAutoPtr<ITexture>         m_OITTail;

    Uint32 m_NumIndices = 0;

    RefCntAutoPtr<IPipelineState>         m_ClearOITLayersPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_ClearOITLayersSRB;

    RefCntAutoPtr<IPipelineState>         m_OpaquePSO;
    RefCntAutoPtr<IPipelineState>         m_AlphaBlendPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_AlphaBlendSRB;
    RefCntAutoPtr<IPipelineState>         m_OITBlendPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_OITBlendSRB;
    RefCntAutoPtr<IPipelineState>         m_UpdateOITLayersPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_UpdateOITLayersSRB;
    RefCntAutoPtr<IPipelineState>         m_AttenuateBackgroundPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_AttenuateBackgroundSRB;

    enum class RenderMode : int
    {
        UnsortedAlphaBlend,
        Layered,
        Count
    } m_RenderMode = RenderMode::Layered;

    TEXTURE_FORMAT m_DepthFormat                = TEX_FORMAT_D32_FLOAT;
    bool           m_EarlyDepthStencilSupported = false;

    bool   m_Animate       = true;
    double m_AnimationTime = 0.0;

    int m_NumOITLayers = 4;

    float4x4              m_ProjMatrix;
    float4x4              m_ViewProjMatrix;
    int                   m_GridSize          = 10;
    float                 m_PercentOpaque     = 10;
    float                 m_MinOpacity        = 0.2f;
    float                 m_MaxOpacity        = 1.0f;
    Uint32                m_ThreadGroupSizeXY = 16;
    std::array<Uint32, 2> m_NumInstances{};
    static constexpr int  MaxGridSize = 32;
};

} // namespace Diligent
