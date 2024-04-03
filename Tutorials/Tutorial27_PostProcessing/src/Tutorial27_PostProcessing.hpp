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

#include <array>
#include <memory>

#include "SampleBase.hpp"
#include "FirstPersonCamera.hpp"
#include "PostFXRenderTechnique.hpp"
#include "ResourceRegistry.hpp"
#include "PBR_Renderer.hpp"

namespace Diligent
{

namespace HLSL
{
struct CameraAttribs;
struct MaterialAttribs;
struct ObjectAttribs;
} // namespace HLSL

class EnvMapRenderer;
class PostFXContext;
class ScreenSpaceReflection;
class ScreenSpaceAmbientOcclusion;
class TemporalAntiAliasing;
class Bloom;
class GBuffer;

class Tutorial27_PostProcessing final : public SampleBase
{
public:
    Tutorial27_PostProcessing();

    void Initialize(const SampleInitInfo& InitInfo) override final;

    void Render() override final;

    void Update(double CurrTime, double ElapsedTime) override final;

    void WindowResize(Uint32 Width, Uint32 Height) override final;

    const Char* GetSampleName() const override final { return "Tutorial27: Post Processing"; }

private:
    void PrepareResources();

    void GenerateGeometry();

    void ComputePostFX();

    void ComputeSSR();

    void ComputeSSAO();

    void ComputeLighting();

    void ComputeTAA();

    void ComputeBloom();

    void ApplyToneMap();

    void UpdateUI();

    void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override;

private:
    using RenderTechnique  = PostFXRenderTechnique;
    using ResourceInternal = RefCntAutoPtr<IDeviceObject>;
    struct ShaderSettings;

    enum RENDER_TECH : Uint32
    {
        RENDER_TECH_GENERATE_GEOMETRY = 0,
        RENDER_TECH_COMPUTE_MOTION_VECTORS,
        RENDER_TECH_COMPUTE_LIGHTING,
        RENDER_TECH_APPLY_TONE_MAP,
        RENDER_TECH_COUNT
    };

    enum RESOURCE_IDENTIFIER : Uint32
    {
        RESOURCE_IDENTIFIER_CAMERA_CONSTANT_BUFFER = 0,
        RESOURCE_IDENTIFIER_PBR_ATTRIBS_CONSTANT_BUFFER,
        RESOURCE_IDENTIFIER_OBJECT_ATTRIBS_CONSTANT_BUFFER,
        RESOURCE_IDENTIFIER_MATERIAL_ATTRIBS_CONSTANT_BUFFER,
        RESOURCE_IDENTIFIER_OBJECT_AABB_VERTEX_BUFFER,
        RESOURCE_IDENTIFIER_OBJECT_AABB_INDEX_BUFFER,
        RESOURCE_IDENTIFIER_RADIANCE0,
        RESOURCE_IDENTIFIER_RADIANCE1,
        RESOURCE_IDENTIFIER_DEPTH0,
        RESOURCE_IDENTIFIER_DEPTH1,
        RESOURCE_IDENTIFIER_ENVIRONMENT_MAP,
        RESOURCE_IDENTIFIER_PREFILTERED_ENVIRONMENT_MAP,
        RESOURCE_IDENTIFIER_IRRADIANCE_MAP,
        RESOURCE_IDENTIFIER_BRDF_INTEGRATION_MAP,
        RESOURCE_IDENTIFIER_OUTPUT,
        RESOURCE_IDENTIFIER_COUNT
    };

    std::array<RenderTechnique, RENDER_TECH_COUNT> m_RenderTech{};
    ResourceRegistry                               m_Resources{};

    std::unique_ptr<GBuffer>                     m_GBuffer;
    std::unique_ptr<EnvMapRenderer>              m_EnvironmentMapRenderer;
    std::unique_ptr<PostFXContext>               m_PostFXContext;
    std::unique_ptr<ScreenSpaceReflection>       m_ScreenSpaceReflection;
    std::unique_ptr<ScreenSpaceAmbientOcclusion> m_ScreenSpaceAmbientOcclusion;
    std::unique_ptr<TemporalAntiAliasing>        m_TemporalAntiAliasing;
    std::unique_ptr<Bloom>                       m_Bloom;
    std::unique_ptr<ShaderSettings>              m_ShaderSettings;

    FirstPersonCamera                        m_Camera;
    std::unique_ptr<HLSL::CameraAttribs[]>   m_CameraAttribs;
    std::unique_ptr<HLSL::ObjectAttribs[]>   m_ObjectAttribs;
    std::unique_ptr<HLSL::MaterialAttribs[]> m_MaterialAttribs;
    std::array<std::vector<float4x4>, 2>     m_ObjectTransforms;

    float  m_AnimationTime     = 0.0f;
    bool   m_IsAnimationActive = true;
    Uint32 m_ObjectCount       = 0;
    Uint32 m_MaterialCount     = 0;

    static constexpr Uint32 m_MaxObjectCount   = 32;
    static constexpr Uint32 m_MaxMaterialCount = 24;

    Uint32 m_SSRSettingsDisplayMode = 0;
};

} // namespace Diligent
