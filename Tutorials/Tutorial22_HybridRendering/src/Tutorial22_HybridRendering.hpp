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

// We only need a 3x3 matrix, but in Vulkan and Metal, the rows of a float3x3 matrix are aligned to 16 bytes,
// which is effectively a float4x3 matrix.
// In DirectX, the rows of a float3x3 matrix are not aligned.
// We will use a float4x3 for compatibility between all APIs.
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

    template <typename MatType>
    float4x3(const MatType& Other) :
        // clang-format off
        m00{Other.m00}, m01{Other.m01}, m02{Other.m02}, 
        m10{Other.m10}, m11{Other.m11}, m12{Other.m12}, 
        m20{Other.m20}, m21{Other.m21}, m22{Other.m22}
    // clang-format on
    {}
};

namespace HLSL
{
#include "../assets/Structures.fxh"
}

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
    void CreateSceneMaterials(uint2& CubeMaterialRange, Uint32& GroundMaterial, std::vector<HLSL::MaterialAttribs>& Materials);
    void CreateSceneObjects(uint2 CubeMaterialRange, Uint32 GroundMaterial);
    void CreateSceneAccelStructs();
    void UpdateTLAS();
    void CreateRasterizationPSO(IShaderSourceInputStreamFactory* pShaderSourceFactory);
    void CreatePostProcessPSO(IShaderSourceInputStreamFactory* pShaderSourceFactory);
    void CreateRayTracingPSO(IShaderSourceInputStreamFactory* pShaderSourceFactory);

    // Pipeline resource signature for scene resources used by the ray-tracing PSO
    RefCntAutoPtr<IPipelineResourceSignature> m_pRayTracingSceneResourcesSign;
    // Pipeline resource signature for screen resources used by the ray-tracing PSO
    RefCntAutoPtr<IPipelineResourceSignature> m_pRayTracingScreenResourcesSign;

    // Ray-tracing PSO
    RefCntAutoPtr<IPipelineState> m_RayTracingPSO;
    // Scene resources for ray-tracing PSO
    RefCntAutoPtr<IShaderResourceBinding> m_RayTracingSceneSRB;
    // Screen resources for ray-tracing PSO
    RefCntAutoPtr<IShaderResourceBinding> m_RayTracingScreenSRB;

    // G-buffer rendering PSO and SRB
    RefCntAutoPtr<IPipelineState>         m_RasterizationPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_RasterizationSRB;

    // Post-processing PSO and SRB
    RefCntAutoPtr<IPipelineState>         m_PostProcessPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_PostProcessSRB;

    // Simple implementation of a mesh
    struct Mesh
    {
        String Name;

        RefCntAutoPtr<IBottomLevelAS> BLAS;
        RefCntAutoPtr<IBuffer>        VertexBuffer;
        RefCntAutoPtr<IBuffer>        IndexBuffer;

        Uint32 NumVertices = 0;
        Uint32 NumIndices  = 0;
        Uint32 FirstIndex  = 0; // Offset in the index buffer if IB and VB are shared between multiple meshes
        Uint32 FirstVertex = 0; // Offset in the vertex buffer
    };
    static Mesh CreateTexturedPlaneMesh(IRenderDevice* pDevice, float2 UVScale);

    // Objects with the same mesh are grouped for instanced draw call
    struct InstancedObjects
    {
        Uint32 MeshInd             = 0; // Index in m_Scene.Meshes
        Uint32 ObjectAttribsOffset = 0; // Offset in m_Scene.ObjectAttribsBuffer
        Uint32 NumObjects          = 0; // Number of instances for a draw call
    };

    struct DynamicObject
    {
        Uint32 ObjectAttribsIndex = 0; // Index in m_Scene.ObjectAttribsBuffer
    };

    struct Scene
    {
        std::vector<InstancedObjects>    ObjectInstances;
        std::vector<DynamicObject>       DynamicObjects;
        std::vector<HLSL::ObjectAttribs> Objects; // CPU-visible array of HLSL::ObjectAttribs

        // Resources used by shaders
        std::vector<Mesh>                    Meshes;
        RefCntAutoPtr<IBuffer>               MaterialAttribsBuffer;
        RefCntAutoPtr<IBuffer>               ObjectAttribsBuffer; // GPU-visible array of HLSL::ObjectAttribs
        std::vector<RefCntAutoPtr<ITexture>> Textures;
        std::vector<RefCntAutoPtr<ISampler>> Samplers;
        RefCntAutoPtr<IBuffer>               ObjectConstants;

        // Resources for ray tracing
        RefCntAutoPtr<ITopLevelAS> TLAS;
        RefCntAutoPtr<IBuffer>     TLASInstancesBuffer; // Used to update TLAS
        RefCntAutoPtr<IBuffer>     TLASScratchBuffer;   // Used to update TLAS
    };
    Scene m_Scene;

    // Constants shared between all PSOs
    RefCntAutoPtr<IBuffer> m_Constants;

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

    float3 m_LightDir = normalize(float3{-0.49f, -0.60f, 0.64f});
    int    m_DrawMode = 0;

    // Vulkan and DirectX require DXC shader compiler.
    // Metal uses the builtin glslang compiler.
#if PLATFORM_MACOS || PLATFORM_IOS || PLATFORM_TVOS
    const SHADER_COMPILER m_ShaderCompiler = SHADER_COMPILER_DEFAULT;
#else
    const SHADER_COMPILER m_ShaderCompiler = SHADER_COMPILER_DXC;
#endif
};

} // namespace Diligent
