/*
 *  Copyright 2024 Diligent Graphics LLC
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

namespace Diligent
{

class Tutorial29_OIT final : public SampleBase
{
public:
    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "Tutorial29: Order-Independent Transparency"; }

private:
    void CreatePipelineStates();
    void CreateInstanceBuffer();
    void PrepareOITResources();
    void UpdateUI();
    void PopulateInstanceBuffer();
    void RenderGrid();

    RefCntAutoPtr<IBuffer>  m_VertexBuffer;
    RefCntAutoPtr<IBuffer>  m_IndexBuffer;
    RefCntAutoPtr<IBuffer>  m_InstanceBuffer;
    RefCntAutoPtr<IBuffer>  m_Constants;
    RefCntAutoPtr<ITexture> m_OITLayers;

    Uint32 m_NumIndices = 0;

    RefCntAutoPtr<IPipelineState>         m_ClearOITLayersPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_ClearOITLayersSRB;

    RefCntAutoPtr<IPipelineState>         m_AlphaBlendPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_AlphaBlendSRB;
    RefCntAutoPtr<IPipelineState>         m_OITBlendPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_OITBlendSRB;
    RefCntAutoPtr<IPipelineState>         m_BuildOITLayersPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_BuildOITLayersSRB;

    enum class RenderMode : int
    {
        UnsortedAlphaBlend,
        LayeredROV,
        Count
    } m_RenderMode = RenderMode::UnsortedAlphaBlend;

    bool   m_Animate       = true;
    double m_AnimationTime = 0.0;

    float4x4             m_ProjMatrix;
    float4x4             m_ViewProjMatrix;
    int                  m_GridSize          = 8;
    float                m_MinOpacity        = 0.2f;
    float                m_MaxOpacity        = 1.0f;
    Uint32               m_ThreadGroupSizeXY = 16;
    static constexpr int MaxGridSize         = 32;
    static constexpr int MaxInstances        = MaxGridSize * MaxGridSize * MaxGridSize;
};

} // namespace Diligent
