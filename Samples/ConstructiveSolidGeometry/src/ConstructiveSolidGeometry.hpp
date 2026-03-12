/*
 *  Copyright 2026 Diligent Graphics LLC
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
#include "ResourceRegistry.hpp"
#include "PBR_Renderer.hpp"
#include "PipelineStateManager.hpp"

namespace Diligent
{

namespace HLSL
{
struct CameraAttribs;
struct MaterialAttribs;
struct ObjectAttribs;
struct ABufferConstants;
struct CSGOperationAttribs;
} // namespace HLSL

class EnvMapRenderer;
class PBR_Renderer;

static constexpr Uint32 DivCeil(Uint32 x, Uint32 y) { return (x + y - 1) / y; }

class ConstructiveSolidGeometry final : public SampleBase
{
public:
    ConstructiveSolidGeometry();

    void Initialize(const SampleInitInfo& InitInfo) override final;

    void Render() override final;

    void Update(double CurrTime, double ElapsedTime, bool DoUpdateUI) override final;

    void WindowResize(Uint32 Width, Uint32 Height) override final;

    virtual void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override final;

    const Char* GetSampleName() const override final { return "Constructive Solid Geometry"; }

protected:
    virtual void UpdateUI() override final;

private:
    void PrepareResources();

    void ComputeToneMapping();

    void ComputeGammaCorrection();

    void ClearABuffer();

    void GenerateCSGGeometry();

    void GenerateCSGMesh();

    void ResolveCSG();

    void LoadMesh(const char* FileName);

    void LoadEnvironmentMap(const char* FileName);

private:
    enum RESOURCE_IDENTIFIER : Uint32
    {
        RESOURCE_IDENTIFIER_CAMERA_CONSTANT_BUFFER = 0,
        RESOURCE_IDENTIFIER_PBR_ATTRIBS_CONSTANT_BUFFER,
        RESOURCE_IDENTIFIER_OBJECT_ATTRIBS_CONSTANT_BUFFER,
        RESOURCE_IDENTIFIER_MATERIAL_ATTRIBS_CONSTANT_BUFFER,
        RESOURCE_IDENTIFIER_OBJECT_AABB_VERTEX_BUFFER,
        RESOURCE_IDENTIFIER_OBJECT_AABB_INDEX_BUFFER,
        RESOURCE_IDENTIFIER_RADIANCE,
        RESOURCE_IDENTIFIER_PREFILTERED_ENVIRONMENT_MAP,
        RESOURCE_IDENTIFIER_BRDF_INTEGRATION_MAP,
        RESOURCE_IDENTIFIER_TONE_MAPPING,
        RESOURCE_IDENTIFIER_ABUFFER_HEAD_POINTER_BUFFER,
        RESOURCE_IDENTIFIER_ABUFFER_NODE_BUFFER,
        RESOURCE_IDENTIFIER_ABUFFER_COUNTER_BUFFER,
        RESOURCE_IDENTIFIER_ABUFFER_CONSTANTS_BUFFER,
        RESOURCE_IDENTIFIER_CSG_OPERATIONS_BUFFER,
        RESOURCE_IDENTIFIER_MESH_VERTEX_BUFFER,
        RESOURCE_IDENTIFIER_MESH_INDEX_BUFFER,
        RESOURCE_IDENTIFIER_COUNT
    };

    ResourceRegistry m_Resources{RESOURCE_IDENTIFIER_COUNT};

    enum PSO_IDENTIFIER : Uint32
    {
        PSO_IDENTIFIER_CLEAR_ABUFFER = 0,
        PSO_IDENTIFIER_GENERATE_CSG_GEOMETRY,
        PSO_IDENTIFIER_GENERATE_CSG_MESH,
        PSO_IDENTIFIER_RESOLVE_CSG,
        PSO_IDENTIFIER_TONE_MAPPING,
        PSO_IDENTIFIER_GAMMA_CORRECTION,
        PSO_IDENTIFIER_COUNT
    };

    RefCntAutoPtr<IRenderStateNotationLoader> m_pRSNLoader;
    std::unique_ptr<PipelineStateManager>     m_PSOManager;

    std::unique_ptr<PBR_Renderer> m_IBLBacker;

    FirstPersonCamera                            m_Camera;
    std::unique_ptr<HLSL::CameraAttribs[]>       m_CameraAttribs;
    std::unique_ptr<HLSL::ObjectAttribs[]>       m_ObjectAttribs;
    std::unique_ptr<HLSL::MaterialAttribs[]>     m_MaterialAttribs;
    std::unique_ptr<HLSL::CSGOperationAttribs[]> m_CSGOperationAttribs;

    float  m_AnimationTime     = 0.0f;
    bool   m_IsAnimationActive = true;
    Uint32 m_ObjectCount       = 0;
    Uint32 m_MaterialCount     = 0;
    Uint32 m_OperationCount    = 0;
    Uint32 m_MeshObjectStart   = 0;
    Uint32 m_MeshObjectCount   = 0;
    Uint32 m_MeshIndexCount    = 0;

    float m_PlaneHeight    = -0.3f;
    int   m_PlaneCSGOp     = 1; // CSG_OP_SUBTRACTION
    bool  m_PlaneIsPrimary = true;

    static constexpr Uint32 m_MaxObjectCount              = 32;
    static constexpr Uint32 m_MaxMaterialCount            = 24;
    static constexpr Uint32 m_MaxOperationCount           = 8;
    static constexpr Uint32 m_MaxABufferFragmentsPerPixel = 16;
    static constexpr Uint32 m_ThreadGroupSize             = 16;

    // Tone mapping parameters
    float m_AverageLogLum          = 0.2f;
    float m_MiddleGray             = 0.18f;
    float m_WhitePoint             = 3.0f;
    float m_PrefilteredCubeLastMip = 0.0f;
};

} // namespace Diligent
