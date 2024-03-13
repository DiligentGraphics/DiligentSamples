/*
 *  Copyright 2019-2024 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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

#include <vector>
#include <memory>
#include <array>

#include "SampleBase.hpp"
#include "GLTFLoader.hpp"
#include "GLTF_PBR_Renderer.hpp"
#include "BasicMath.hpp"
#include "TrackballCamera.hpp"
#include "GBuffer.hpp"

namespace Diligent
{

namespace HLSL
{
struct CameraAttribs;
}

class EnvMapRenderer;
class VectorFieldRenderer;
class PostFXContext;
class ScreenSpaceReflection;
class BoundBoxRenderer;

class GLTFViewer final : public SampleBase
{
public:
    GLTFViewer();
    ~GLTFViewer();
    virtual CommandLineStatus ProcessCommandLine(int argc, const char* const* argv) override final;

    virtual void Initialize(const SampleInitInfo& InitInfo) override final;
    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "GLTF Viewer"; }

private:
    void LoadModel(const char* Path);
    void UpdateScene();
    void UpdateUI();
    void CreateGLTFResourceCache();
    void UpdateModelsList(const std::string& Dir);
    bool SetEnvironmentMap(ITextureView* pEnvMap);
    void CreateGLTFRenderer();
    void CrateEnvMapRenderer();
    void CrateBoundBoxRenderer();
    void CreateVectorFieldRenderer();

    enum class BackgroundMode : int
    {
        None,
        EnvironmentMap,
        Irradiance,
        PrefilteredEnvMap,
        NumModes
    } m_BackgroundMode = BackgroundMode::PrefilteredEnvMap;

    GLTF_PBR_Renderer::RenderInfo m_RenderParams;

    struct ShaderParams
    {
        float OcclusionStrength = 1;
        float EmissionScale     = 1;
        float IBLScale          = 1;
        float AverageLogLum     = 0.3f;
        float MiddleGray        = 0.18f;
        float WhitePoint        = 3.f;

        float4 HighlightColor = float4{0, 0, 0, 0};
        float4 WireframeColor = float4{0.8f, 0.7f, 0.5f, 1.0f};

        float SSRScale        = 1;
        float SSAOScale       = 1;
        int   PostFXDebugMode = 0;
    };
    ShaderParams m_ShaderAttribs;

    GLTF::Light m_DefaultLight;
    float3      m_LightDirection;
    float       m_EnvMapMipLevel = 1.f;
    int         m_SelectedModel  = 0;

    float4x4 m_BoundBoxTransform;

    struct ModelInfo
    {
        std::string Name;
        std::string Path;
    };
    std::vector<ModelInfo>   m_Models;
    std::vector<const char*> m_ModelNames;

    enum class BoundBoxMode : int
    {
        None = 0,
        Local,
        Global
    };

    BoundBoxMode       m_BoundBoxMode   = BoundBoxMode::None;
    bool               m_PlayAnimation  = false;
    int                m_AnimationIndex = 0;
    std::vector<float> m_AnimationTimers;
    float              m_ElapsedTime = 0.f;

    std::unique_ptr<GLTF_PBR_Renderer>   m_GLTFRenderer;
    std::unique_ptr<GLTF::Model>         m_Model;
    std::array<GLTF::ModelTransforms, 2> m_Transforms; // [0] - current frame, [1] - previous frame
    BoundBox                             m_ModelAABB;
    float4x4                             m_ModelTransform;
    float                                m_SceneScale = 1.f;
    RefCntAutoPtr<IBuffer>               m_FrameAttribsCB;
    RefCntAutoPtr<ITextureView>          m_EnvironmentMapSRV;
    RefCntAutoPtr<ITextureView>          m_WhiteFurnaceEnvMapSRV;

    ITextureView* m_pCurrentEnvMapSRV = nullptr;

    std::unique_ptr<GBuffer> m_GBuffer;

    struct ApplyPosteffects
    {
        RefCntAutoPtr<IPipelineState>         pPSO;
        RefCntAutoPtr<IShaderResourceBinding> pSRB;

        IShaderResourceVariable* ptex2DRadianceVar         = nullptr;
        IShaderResourceVariable* ptex2DNormalVar           = nullptr;
        IShaderResourceVariable* ptex2DSSR                 = nullptr;
        IShaderResourceVariable* ptex2DPecularIBL          = nullptr;
        IShaderResourceVariable* ptex2DBaseColorVar        = nullptr;
        IShaderResourceVariable* ptex2DMaterialDataVar     = nullptr;
        IShaderResourceVariable* ptex2DPreintegratedGGXVar = nullptr;

        void Initialize(IRenderDevice* pDevice, TEXTURE_FORMAT RTVFormat, IBuffer* pFrameAttribsCB);
        operator bool() const { return pPSO != nullptr; }
    };
    ApplyPosteffects m_ApplyPostFX;

    std::unique_ptr<EnvMapRenderer>        m_EnvMapRenderer;
    std::unique_ptr<BoundBoxRenderer>      m_BoundBoxRenderer;
    std::unique_ptr<VectorFieldRenderer>   m_VectorFieldRenderer;
    std::unique_ptr<PostFXContext>         m_PostFXContext;
    std::unique_ptr<ScreenSpaceReflection> m_SSR;

    bool                                    m_bUseResourceCache = false;
    RefCntAutoPtr<GLTF::ResourceManager>    m_pResourceMgr;
    GLTF_PBR_Renderer::ResourceCacheUseInfo m_CacheUseInfo;

    GLTF_PBR_Renderer::ModelResourceBindings m_ModelResourceBindings;
    GLTF_PBR_Renderer::ResourceCacheBindings m_CacheBindings;

    TrackballCamera<float>                 m_Camera;
    std::unique_ptr<HLSL::CameraAttribs[]> m_CameraAttribs; // [0] - current frame, [1] - previous frame
    bool                                   m_bResetPrevCamera = true;

    Uint32 m_CameraId = 0;

    std::vector<const GLTF::Node*> m_CameraNodes;
    std::vector<const GLTF::Node*> m_LightNodes;

    std::string m_ModelPath;

    bool m_bComputeBoundingBoxes = false;
    bool m_bWireframeSupported   = false;
    bool m_bEnablePostProcessing = false;

    PBR_Renderer::SHADER_TEXTURE_ARRAY_MODE m_TextureArrayMode = PBR_Renderer::SHADER_TEXTURE_ARRAY_MODE_NONE;
};

} // namespace Diligent
