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
namespace
{

struct float4x3
{
    float m00 = 0.f;
    float m01 = 0.f;
    float m02 = 0.f;
    float m03 = 0.f; // Unused

    float m10 = 0.f;
    float m11 = 0.f;
    float m12 = 0.f;
    float m13 = 0.f; // Unused

    float m20 = 0.f;
    float m21 = 0.f;
    float m22 = 0.f;
    float m23 = 0.f; // Unused

    float4x3() {}
    float4x3(const float3x3& Other) :
        m00{Other.m00}, m01{Other.m01}, m02{Other.m02}, m10{Other.m10}, m11{Other.m11}, m12{Other.m12}, m20{Other.m20}, m21{Other.m21}, m22{Other.m22}
    {}
};

#include "../assets/structures.fxh"

} // namespace


class Tutorial22_HybridRendering final : public SampleBase
{
public:
    virtual void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override final;
    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "Tutorial22: Hybrid rendering"; }

    virtual void WindowResize(Uint32 Width, Uint32 Height) override final;

private:
    void UpdateUI();
    void CreateScene();
    void CreateRasterizationPSO(IShaderSourceInputStreamFactory* pShaderSourceFactory);
    void CreatePostProcessPSO(IShaderSourceInputStreamFactory* pShaderSourceFactory);
    void CreateRayTracingPSO(IShaderSourceInputStreamFactory* pShaderSourceFactory);

    // Pipeline resource signature for scene resources used by the ray-tracing PSO
    RefCntAutoPtr<IPipelineResourceSignature> m_pRayTracingSceneResourcesSign;
    // Pipeline resource signature for screen resources used by the ray-tracing PSO
    RefCntAutoPtr<IPipelineResourceSignature> m_pRayTracingScreenResourcesSign;

    RefCntAutoPtr<IPipelineState>         m_RayTracingPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_RayTracingSceneSRB;
    RefCntAutoPtr<IShaderResourceBinding> m_RayTracingScreenSRB;
    RefCntAutoPtr<IPipelineState>         m_RasterizationPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_RasterizationSRB;
    RefCntAutoPtr<IPipelineState>         m_PostProcessPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_PostProcessSRB;

    struct Mesh
    {
        String                        Name;
        RefCntAutoPtr<IBottomLevelAS> BLAS;
        RefCntAutoPtr<IBuffer>        VertexBuffer;
        RefCntAutoPtr<IBuffer>        IndexBuffer;
        Uint32                        NumVertices = 0;
        Uint32                        NumIndices  = 0;
        Uint32                        FirstIndex  = 0; // offset in index buffer if IB and VB are shared between multiple meshes
    };

    struct InstancedObjects
    {
        Uint32 MeshInd          = 0; // index in m_Scene.Meshes
        Uint32 ObjectDataOffset = 0; // offset in m_Scene.ObjectAttribsBuffer
        Uint32 NumObjects       = 0; // number of instances for draw call
    };

    struct SceneTempData
    {
        uint2                        CubeMaterialRange;
        Uint32                       GroundMaterial = 0;
        std::vector<ObjectAttribs>   Objects;
        std::vector<MaterialAttribs> Materials;
    };

    struct Scene
    {
        std::vector<InstancedObjects>        Objects;
        std::vector<Mesh>                    Meshes;
        RefCntAutoPtr<IBuffer>               MaterialAttribsBuffer;
        RefCntAutoPtr<IBuffer>               ObjectAttribsBuffer;
        std::vector<RefCntAutoPtr<ITexture>> Textures;
        std::vector<RefCntAutoPtr<ISampler>> Samplers;
        RefCntAutoPtr<ITopLevelAS>           TLAS;
    };
    static void CreateSceneMaterials(IRenderDevice* pDevice, Scene& scene, SceneTempData& temp);
    static void CreateSceneObjects(IRenderDevice* pDevice, Scene& scene, SceneTempData& temp);
    static void CreateSceneAccelStructs(IRenderDevice* pDevice, IDeviceContext* pContext, Scene& scene, SceneTempData& temp);
    static Mesh CreateTexturedPlaneMesh(IRenderDevice* pDevice, float2 UVScale);

    Scene                  m_Scene;
    RefCntAutoPtr<IBuffer> m_Constants;
    RefCntAutoPtr<IBuffer> m_ObjectConstants;

    FirstPersonCamera m_Camera;

    struct GBuffer
    {
        RefCntAutoPtr<ITexture> Color;
        RefCntAutoPtr<ITexture> Normal;
        RefCntAutoPtr<ITexture> Depth;
    };

    const uint2    m_BlockSize          = {8, 8};
    TEXTURE_FORMAT m_ColorTargetFormat  = TEX_FORMAT_RGBA8_UNORM;
    TEXTURE_FORMAT m_NormalTargetFormat = TEX_FORMAT_RGBA16_FLOAT;
    TEXTURE_FORMAT m_DepthTargetFormat  = TEX_FORMAT_D32_FLOAT;
    TEXTURE_FORMAT m_RayTracedTexFormat = TEX_FORMAT_RGBA16_FLOAT;

    GBuffer                 m_GBuffer;
    RefCntAutoPtr<ITexture> m_RayTracedTex;

    const float4 m_SkyColor = {0.412f, 0.796f, 1.0f, 1.0f};
    float3       m_LightPos = {70.f, 100.f, 88.f};
    int          m_DrawMode = 0;

#if PLATFORM_MACOS || PLATFORM_IOS
    const SHADER_COMPILER m_ShaderCompiler = SHADER_COMPILER_DEFAULT;
#else
    const SHADER_COMPILER m_ShaderCompiler = SHADER_COMPILER_DXC;
#endif
};

} // namespace Diligent
