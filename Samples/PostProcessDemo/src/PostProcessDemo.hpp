/*
 *  Copyright 2019-2023 Diligent Graphics LLC
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
#include "GLTFLoader.hpp"
#include "GLTF_PBR_Renderer.hpp"
#include "BasicMath.hpp"
#include "FirstPersonCamera.hpp"
#include "ScreenSpaceReflection.hpp"

namespace Diligent
{

class PostProcessDemo final : public SampleBase
{
public:
    PostProcessDemo();

    ~PostProcessDemo() override;

    CommandLineStatus ProcessCommandLine(int argc, const char* const* argv) override;

    void Initialize(const SampleInitInfo& InitInfo) override;

    void WindowResize(Uint32 Width, Uint32 Height) override;

    void Render() override;

    void Update(double CurrTime, double ElapsedTime) override;

    const Char* GetSampleName() const override { return "PostProcessDemo"; }

private:
    void LoadModel(const char* Path);

    void UpdateScene();

    void UpdateUI();

private:
    GLTF_PBR_Renderer::RenderInfo m_RenderParams;

    struct SSRShaderParams
    {
        float SceneRoughness                     = 0.1f;
        float DepthBufferThickness               = 5.0f;
        float RoughnessThreshold                 = 1.0f;
        int   MaxTraversalIntersections          = 128;
        int   MostDetailedMip                    = 0;
        float GGXImportanceSampleBias            = 0.3f;
        float SpatialReconstructionRadius        = 4.0f;
        float TemporalRadianceStabilityFactor    = 1.0f;
        float TemporalVarianceStabilityFactor    = 0.8f;
        float BilateralCleanupSpatialSigmaFactor = 0.9f;
    };

    struct ShaderParams
    {
        float           OcclusionStrength = 1;
        float           EmissionScale     = 1;
        float           IBLScale          = 1;
        float           AverageLogLum     = 0.3f;
        float           MiddleGray        = 0.18f;
        float           WhitePoint        = 3.f;
        float4          HighlightColor    = float4{0, 0, 0, 0};
        float4          WireframeColor    = float4{0.8f, 0.7f, 0.5f, 1.0f};
        SSRShaderParams SSRParams         = {};
    } m_ShaderAttribs;

    std::unique_ptr<GLTF_PBR_Renderer> m_GLTFRenderer;
    std::unique_ptr<GLTF::Model>       m_Model;
    GLTF::ModelTransforms              m_Transforms;

    RefCntAutoPtr<IBuffer>      m_pFrameAttribsCB;
    RefCntAutoPtr<IBuffer>      m_pToneMapingAttribsCB;
    RefCntAutoPtr<ITextureView> m_pEnvironmentMapSRV;

    GLTF_PBR_Renderer::ModelResourceBindings m_ModelResourceBindings;
    GLTF_PBR_Renderer::ResourceCacheBindings m_CacheBindings;
    std::unique_ptr<ScreenSpaceReflection>   m_pScreenSpaceReflection;

    RefCntAutoPtr<IPipelineState>         m_pComputeMotionVectorsPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pComputeMotionVectorsSRB;

    RefCntAutoPtr<IPipelineState>         m_pGenerateRoughnessNormalPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pGenerateRoughnessNormalSRB;

    RefCntAutoPtr<IPipelineState>         m_pApplyPostEffectsPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pApplyPostEffectsSRB;

    FirstPersonCamera m_Camera;
    float4x4          m_CameraViewLast;
    std::string       m_InitialModelPath;

    RefCntAutoPtr<ITexture> m_pDepthBuffer;
    RefCntAutoPtr<ITexture> m_pColorBuffer;
    RefCntAutoPtr<ITexture> m_pNormalBuffer;
    RefCntAutoPtr<ITexture> m_pMaterialBuffer;
    RefCntAutoPtr<ITexture> m_pMotionVectors;
    void*                   m_pImGuiFont;
};

} // namespace Diligent
