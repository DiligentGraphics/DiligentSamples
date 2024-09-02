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
#include <array>
#include <cfloat>
#include <unordered_map>
#include <mutex>
#include <atomic>
#include <functional>
#include <string>
#include <limits>
#include <algorithm>

#include "../../../DiligentCore/Platforms/interface/PlatformMisc.hpp"
#include "../../../DiligentCore/Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "../../../DiligentCore/Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "../../../DiligentCore/Graphics/GraphicsEngine/interface/GraphicsTypesX.hpp"
#include "../../../DiligentCore/Common/interface/RefCntAutoPtr.hpp"
#include "../../../DiligentCore/Common/interface/AdvancedMath.hpp"
#include "../../../DiligentCore/Common/interface/STDAllocator.hpp"
#include "GLTFResourceManager.hpp"

namespace tinygltf
{

class Node;
class Model;
struct Mesh;
struct Material;
struct Image;

} // namespace tinygltf

namespace Diligent
{

enum IMAGE_FILE_FORMAT : Uint8;

namespace GLTF
{

class ModelBuilder;
class MaterialBuilder;

/// Texture attribute description.
struct TextureAttributeDesc
{
    /// Texture attribute name (e.g. "baseColorTexture", "metallicRoughnessTexture", etc.)
    const char* Name = nullptr;

    /// Texture attribute index in Material.ShaderAttribs (e.g. UVSelectorX, TextureSliceX, UVScaleBias[X]).
    Uint32 Index = 0;
};

static constexpr char BaseColorTextureName[]            = "baseColorTexture";
static constexpr char MetallicRoughnessTextureName[]    = "metallicRoughnessTexture";
static constexpr char NormalTextureName[]               = "normalTexture";
static constexpr char OcclusionTextureName[]            = "occlusionTexture";
static constexpr char EmissiveTextureName[]             = "emissiveTexture";
static constexpr char DiffuseTextureName[]              = "diffuseTexture";
static constexpr char SpecularGlossinessTextureName[]   = "specularGlossinessTexture";
static constexpr char ClearcoatTextureName[]            = "clearcoatTexture";
static constexpr char ClearcoatRoughnessTextureName[]   = "clearcoatRoughnessTexture";
static constexpr char ClearcoatNormalTextureName[]      = "clearcoatNormalTexture";
static constexpr char SheenColorTextureName[]           = "sheenColorTexture";
static constexpr char SheenRoughnessTextureName[]       = "sheenRoughnessTexture";
static constexpr char AnisotropyTextureName[]           = "anisotropyTexture";
static constexpr char IridescenceTextureName[]          = "iridescenceTexture";
static constexpr char IridescenceThicknessTextureName[] = "iridescenceThicknessTexture";
static constexpr char TransmissionTextureName[]         = "transmissionTexture";
static constexpr char ThicknessTextureName[]            = "thicknessTexture";

static constexpr Uint32 DefaultBaseColorTextureAttribId            = 0;
static constexpr Uint32 DefaultMetallicRoughnessTextureAttribId    = 1;
static constexpr Uint32 DefaultNormalTextureAttribId               = 2;
static constexpr Uint32 DefaultOcclusionTextureAttribId            = 3;
static constexpr Uint32 DefaultEmissiveTextureAttribId             = 4;
static constexpr Uint32 DefaultDiffuseTextureAttribId              = 0; // Same as base color
static constexpr Uint32 DefaultSpecularGlossinessTextureAttibId    = 1; // Same as metallic-roughness
static constexpr Uint32 DefaultClearcoatTextureAttribId            = 5;
static constexpr Uint32 DefaultClearcoatRoughnessTextureAttribId   = 6;
static constexpr Uint32 DefaultClearcoatNormalTextureAttribId      = 7;
static constexpr Uint32 DefaultSheenColorTextureAttribId           = 8;
static constexpr Uint32 DefaultSheenRoughnessTextureAttribId       = 9;
static constexpr Uint32 DefaultAnisotropyTextureAttribId           = 10;
static constexpr Uint32 DefaultIridescenceTextureAttribId          = 11;
static constexpr Uint32 DefaultIridescenceThicknessTextureAttribId = 12;
static constexpr Uint32 DefaultTransmissionTextureAttribId         = 13;
static constexpr Uint32 DefaultThicknessTextureAttribId            = 14;

// clang-format off
static constexpr std::array<TextureAttributeDesc, 17> DefaultTextureAttributes =
{
    // Metallic-roughness
    TextureAttributeDesc{BaseColorTextureName,            DefaultBaseColorTextureAttribId},
    TextureAttributeDesc{MetallicRoughnessTextureName,    DefaultMetallicRoughnessTextureAttribId},
    TextureAttributeDesc{NormalTextureName,               DefaultNormalTextureAttribId},
    TextureAttributeDesc{OcclusionTextureName,            DefaultOcclusionTextureAttribId},
    TextureAttributeDesc{EmissiveTextureName,             DefaultEmissiveTextureAttribId},
    TextureAttributeDesc{ClearcoatTextureName,            DefaultClearcoatTextureAttribId},
    TextureAttributeDesc{ClearcoatRoughnessTextureName,   DefaultClearcoatRoughnessTextureAttribId},
    TextureAttributeDesc{ClearcoatNormalTextureName,      DefaultClearcoatNormalTextureAttribId},
    TextureAttributeDesc{SheenColorTextureName,           DefaultSheenColorTextureAttribId},
    TextureAttributeDesc{SheenRoughnessTextureName,       DefaultSheenRoughnessTextureAttribId},
    TextureAttributeDesc{AnisotropyTextureName,           DefaultAnisotropyTextureAttribId},
    TextureAttributeDesc{IridescenceTextureName,          DefaultIridescenceTextureAttribId},
    TextureAttributeDesc{IridescenceThicknessTextureName, DefaultIridescenceThicknessTextureAttribId},
    TextureAttributeDesc{TransmissionTextureName,         DefaultTransmissionTextureAttribId},
    TextureAttributeDesc{ThicknessTextureName,            DefaultThicknessTextureAttribId},

    // Specular-glossiness
    TextureAttributeDesc{DiffuseTextureName,            DefaultDiffuseTextureAttribId},
    TextureAttributeDesc{SpecularGlossinessTextureName, DefaultSpecularGlossinessTextureAttibId}
};
// clang-format on


struct Material
{
    enum PBR_WORKFLOW
    {
        PBR_WORKFLOW_METALL_ROUGH = 0,
        PBR_WORKFLOW_SPEC_GLOSS,
        PBR_WORKFLOW_UNLIT
    };

    enum ALPHA_MODE
    {
        ALPHA_MODE_OPAQUE = 0,
        ALPHA_MODE_MASK,
        ALPHA_MODE_BLEND,
        ALPHA_MODE_NUM_MODES
    };

    // Material attributes packed in a shader-friendly format
    struct ShaderAttribs
    {
        float4 BaseColorFactor = float4{1, 1, 1, 1};
        float4 EmissiveFactor  = float4{0, 0, 0, 0};
        float4 SpecularFactor  = float4{1, 1, 1, 1};

        int   Workflow       = PBR_WORKFLOW_METALL_ROUGH;
        int   AlphaMode      = ALPHA_MODE_OPAQUE;
        float AlphaCutoff    = 0.5f;
        float MetallicFactor = 1;

        float RoughnessFactor          = 1;
        float OcclusionFactor          = 1;
        float ClearcoatFactor          = 0;
        float ClearcoatRoughnessFactor = 0;

        // Any user-specific data
        float4 CustomData = float4{0, 0, 0, 0};
    };
    static_assert(sizeof(ShaderAttribs) % 16 == 0, "ShaderAttribs struct must be 16-byte aligned");
    ShaderAttribs Attribs;

    struct SheenShaderAttribs
    {
        float3 ColorFactor     = float3{0, 0, 0};
        float  RoughnessFactor = 0;
    };
    static_assert(sizeof(ShaderAttribs) % 16 == 0, "SheenShaderAttribs struct must be 16-byte aligned");
    std::unique_ptr<SheenShaderAttribs> Sheen;

    struct AnisotropyShaderAttribs
    {
        float Strength   = 0;
        float Rotation   = 0;
        float Padding[2] = {};
    };
    static_assert(sizeof(AnisotropyShaderAttribs) % 16 == 0, "AnisotropyShaderAttribs struct must be 16-byte aligned");
    std::unique_ptr<AnisotropyShaderAttribs> Anisotropy;

    struct IridescenceShaderAttribs
    {
        float Factor           = 0;
        float IOR              = 1.3f;
        float ThicknessMinimum = 100;
        float ThicknessMaximum = 400;
    };
    static_assert(sizeof(IridescenceShaderAttribs) % 16 == 0, "IridescenceShaderAttribs struct must be 16-byte aligned");
    std::unique_ptr<IridescenceShaderAttribs> Iridescence;

    struct TransmissionShaderAttribs
    {
        float Factor     = 0;
        float Padding[3] = {};
    };
    static_assert(sizeof(TransmissionShaderAttribs) % 16 == 0, "TransmissionShaderAttribs struct must be 16-byte aligned");
    std::unique_ptr<TransmissionShaderAttribs> Transmission;

    struct VolumeShaderAttribs
    {
        float3 AttenuationColor = float3{1, 1, 1};
        float  ThicknessFactor  = 0;

        float AttenuationDistance = +FLT_MAX;
        float Padding[3]          = {};
    };
    static_assert(sizeof(VolumeShaderAttribs) % 16 == 0, "VolumeShaderAttribs struct must be 16-byte aligned");
    std::unique_ptr<VolumeShaderAttribs> Volume;

    struct TextureShaderAttribs
    {
        float UVSelector   = -1;
        float TextureSlice = 0;
        float UBias        = 0;
        float VBias        = 0;

        float2x2 UVScaleAndRotation = float2x2::Identity();

        // Atlas UV scale and bias are applied after the UV transform.
        float4 AtlasUVScaleAndBias = float4{1, 1, 0, 0};
    };
    static_assert(sizeof(TextureShaderAttribs) % 16 == 0, "TextureShaderAttribs struct must be 16-byte aligned");

private:
    // Texture indices in Model.Textures array, for each attribute.
    //  _________________            _______________________         __________________
    // |                 |          |                       |       |                   |
    // |   GLTF Model    |          |       Material        |       |       Model       |
    // |                 |          |                       |       |                   |
    // |                 |          |      TextureIds       |       |     Textures      |
    // | "normalTexture" |          | [   |   | 3 |   |   ] |       | [   |   |   |   ] |
    // |      |          |          |          A |          |       |               A   |
    // |      |_ _ _ _ _ |_ _ _2_ _ |_ _ _ _ _ | |_ _ _ _ __|_ _3_ _|_ _ _ _ _ _ _ _|   |
    // |                 |     A    |                       |       |                   |
    // |_________________|     |    |_______________________|       |___________________|
    //                         |
    //                    Defined by
    //              ModeCI.TextureAttributes
    //
    std::unique_ptr<int[]> TextureIds;

    std::unique_ptr<TextureShaderAttribs[]> TextureAttribs;

    Uint32 ActiveTextureAttribs = 0;

    size_t GetActiveTextureAttribPackedIndex(Uint32 Idx) const
    {
        VERIFY_EXPR(IsTextureAttribActive(Idx));
        return PlatformMisc::CountOneBits(ActiveTextureAttribs & ((1u << Idx) - 1u));
    }

    friend MaterialBuilder;

public:
    bool DoubleSided  = false;
    bool HasClearcoat = false;

    // Any user-specific data. One way to set this field is from the
    // MaterialLoadCallback.
    RefCntAutoPtr<IObject> pUserData;

    static constexpr Uint32 MaxTextureAttribs = sizeof(ActiveTextureAttribs) * 8;

public:
    Material() noexcept {}

    // clang-format off
    Material           (Material&&) = default;
    Material& operator=(Material&&) = default;
    // clang-format on

    static constexpr Uint32 InvalidTextureAttribIdx = ~0u;

    Uint32 GetMaxActiveTextureAttribIdx() const
    {
        return ActiveTextureAttribs == 0 ? InvalidTextureAttribIdx : PlatformMisc::GetMSB(ActiveTextureAttribs);
    }

    Uint32 GetNumActiveTextureAttribs() const
    {
        return PlatformMisc::CountOneBits(ActiveTextureAttribs);
    }

    bool IsTextureAttribActive(Uint32 Idx) const
    {
        VERIFY_EXPR(Idx < MaxTextureAttribs);
        return (ActiveTextureAttribs & (1u << Idx)) != 0;
    }

    int GetTextureId(Uint32 Idx) const
    {
        return IsTextureAttribActive(Idx) ? TextureIds[GetActiveTextureAttribPackedIndex(Idx)] : -1;
    }
    void SetTextureId(Uint32 Idx, int TextureId)
    {
        TextureIds[GetActiveTextureAttribPackedIndex(Idx)] = TextureId;
    }

    TextureShaderAttribs& GetTextureAttrib(Uint32 Idx)
    {
        return TextureAttribs[GetActiveTextureAttribPackedIndex(Idx)];
    }
    const TextureShaderAttribs& GetTextureAttrib(Uint32 Idx) const
    {
        static constexpr TextureShaderAttribs DefaultAttribs{};
        return IsTextureAttribActive(Idx) ? TextureAttribs[GetActiveTextureAttribPackedIndex(Idx)] : DefaultAttribs;
    }

    template <typename HandlerType>
    void ProcessActiveTextureAttibs(HandlerType&& Handler) const
    {
        Uint32 ActiveAttribs = ActiveTextureAttribs;
        while (ActiveAttribs != 0)
        {
            const auto Idx         = PlatformMisc::GetLSB(ActiveAttribs);
            const auto PackedIndex = GetActiveTextureAttribPackedIndex(Idx);
            if (!Handler(Idx, TextureAttribs[PackedIndex], TextureIds[PackedIndex]))
                break;
            ActiveAttribs &= ~(1u << Idx);
        }
    }
};


struct Primitive
{
    const Uint32 FirstIndex;
    const Uint32 IndexCount;
    const Uint32 VertexCount;
    const Uint32 MaterialId;

    const BoundBox BB;

    Primitive(Uint32        _FirstIndex,
              Uint32        _IndexCount,
              Uint32        _VertexCount,
              Uint32        _MaterialId,
              const float3& _BBMin,
              const float3& _BBMax) :
        FirstIndex{_FirstIndex},
        IndexCount{_IndexCount},
        VertexCount{_VertexCount},
        MaterialId{_MaterialId},
        BB{_BBMin, _BBMax}
    {
    }

    Primitive(Primitive&&) = default;

    bool HasIndices() const
    {
        return IndexCount > 0;
    }
};

struct Mesh
{
    std::string            Name;
    std::vector<Primitive> Primitives;
    BoundBox               BB;

    // Any user-specific data. One way to set this field is from the
    // MeshLoadCallback.
    RefCntAutoPtr<IObject> pUserData;

    // There may be no primitives in the mesh, in which
    // case the bounding box will be invalid.
    bool IsValidBB() const
    {
        return !Primitives.empty();
    }

    void UpdateBoundingBox()
    {
        if (!Primitives.empty())
        {
            BB = Primitives[0].BB;
            for (size_t prim = 1; prim < Primitives.size(); ++prim)
            {
                const auto& PrimBB{Primitives[prim].BB};
                BB.Min = std::min(BB.Min, PrimBB.Min);
                BB.Max = std::max(BB.Max, PrimBB.Max);
            }
        }
    }
};

struct Node;
struct Skin
{
    std::string              Name;
    const Node*              pSkeletonRoot = nullptr;
    std::vector<float4x4>    InverseBindMatrices;
    std::vector<const Node*> Joints;
};

struct Camera
{
    std::string Name;

    enum class Projection
    {
        Unknown,
        Perspective,
        Orthographic
    } Type = Projection::Unknown;

    struct PerspectiveAttribs
    {
        float AspectRatio;
        float YFov;
        float ZNear;
        float ZFar;
    };
    struct OrthographicAttribs
    {
        float XMag;
        float YMag;
        float ZNear;
        float ZFar;
    };
    union
    {
        PerspectiveAttribs  Perspective = {};
        OrthographicAttribs Orthographic;
    };
};

struct Light
{
    std::string Name;

    enum class TYPE
    {
        UNKNOWN,
        DIRECTIONAL,
        POINT,
        SPOT,
    } Type = TYPE::UNKNOWN;

    float3 Color = float3{1, 1, 1};

    float Intensity = 1;

    // Point and spot lights only.
    //
    // Recommended implementation is as follows:
    //   Attenuation = clamp(1.0 - (Distance / Range)^4, 0, 1) / Distance^2
    float Range = 0;

    // Spot light only
    float InnerConeAngle = 0;
    float OuterConeAngle = 0;
};

struct Node
{
    // Index in Model.LinearNodes array.
    const int Index;

    // Index in ModelTransforms.Skins array.
    int SkinTransformsIndex = -1;

    std::string Name;

    const Node* Parent = nullptr;

    std::vector<const Node*> Children;

    const Mesh*   pMesh   = nullptr;
    const Camera* pCamera = nullptr;
    const Skin*   pSkin   = nullptr;
    const Light*  pLight  = nullptr;

    float3      Translation;
    QuaternionF Rotation;
    float3      Scale  = float3{1, 1, 1};
    float4x4    Matrix = float4x4::Identity();

    explicit Node(int _Index) :
        Index{_Index}
    {}

    inline float4x4 ComputeLocalTransform() const;
};

struct Scene
{
    std::string        Name;
    std::vector<Node*> RootNodes;
    // Linear list of all nodes in the scene.
    std::vector<Node*> LinearNodes;
};

struct AnimationChannel
{
    enum class PATH_TYPE
    {
        TRANSLATION,
        ROTATION,
        SCALE,
        WEIGHTS
    };
    PATH_TYPE const PathType;
    Node* const     pNode;
    Uint32 const    SamplerIndex;

    AnimationChannel(PATH_TYPE _PathType,
                     Node*     _pNode,
                     Uint32    _SamplerIndex) :
        PathType{_PathType},
        pNode{_pNode},
        SamplerIndex{_SamplerIndex}
    {}
};


struct AnimationSampler
{
    enum class INTERPOLATION_TYPE
    {
        LINEAR,
        STEP,
        CUBICSPLINE
    };
    const INTERPOLATION_TYPE Interpolation;

    std::vector<float>  Inputs;
    std::vector<float4> OutputsVec4;

    // Returns the index of the key frame for the given animation time.
    inline size_t FindKeyFrame(float Time) const;

    explicit AnimationSampler(INTERPOLATION_TYPE _Interpolation) :
        Interpolation{_Interpolation}
    {}
};

struct Animation
{
    std::string                   Name;
    std::vector<AnimationSampler> Samplers;
    std::vector<AnimationChannel> Channels;

    float Start = +(std::numeric_limits<float>::max)();
    float End   = -(std::numeric_limits<float>::max)();
};



/// Vertex attribute description.
struct VertexAttributeDesc
{
    /// Attribute name ("POSITION", "NORMAL", "TEXCOORD_0", "TEXCOORD_1", "JOINTS_0", "WEIGHTS_0", etc.).
    const char* Name = nullptr;

    /// Index of the vertex buffer that stores this attribute.
    Uint8 BufferId = 0;

    /// The type of the attribute's components.
    VALUE_TYPE ValueType = VT_UNDEFINED;

    /// The number of components in the attribute.
    Uint8 NumComponents = 0;

    /// Relative offset, in bytes, from the start of the vertex data to the start of the attribute.
    /// If this value is set to 0xFFFFFFFF (the default value), the offset will
    /// be computed automatically by placing the attribute right after the previous one.
    Uint32 RelativeOffset = ~0U;

    /// Default attribute value.
    ///
    /// \remarks    This value is used when the attribute is not present in the source GLTF model.
    ///             The pointer must point to a value of the appropriate type (e.g. float3 for VT_FLOAT32, 3).
    ///             If this value is null, the attribute will be initialized with zeros.
    const void* pDefaultValue = nullptr;

    constexpr VertexAttributeDesc() noexcept {}

    constexpr VertexAttributeDesc(const char* _Name,
                                  Uint8       _BufferId,
                                  VALUE_TYPE  _ValueType,
                                  Uint8       _NumComponents,
                                  Uint32      _RelativeOffset = VertexAttributeDesc{}.RelativeOffset,
                                  const void* _pDefaultValue  = VertexAttributeDesc{}.pDefaultValue) noexcept :
        Name{_Name},
        BufferId{_BufferId},
        ValueType{_ValueType},
        NumComponents{_NumComponents},
        RelativeOffset{_RelativeOffset},
        pDefaultValue{_pDefaultValue}
    {}

    constexpr VertexAttributeDesc(const char* _Name,
                                  Uint8       _BufferId,
                                  VALUE_TYPE  _ValueType,
                                  Uint8       _NumComponents,
                                  const void* _pDefaultValue) noexcept :
        Name{_Name},
        BufferId{_BufferId},
        ValueType{_ValueType},
        NumComponents{_NumComponents},
        pDefaultValue{_pDefaultValue}
    {}
};

static constexpr char PositionAttributeName[]    = "POSITION";
static constexpr char VertexColorAttributeName[] = "COLOR_0";
static constexpr char NormalAttributeName[]      = "NORMAL";
static constexpr char Texcoord0AttributeName[]   = "TEXCOORD_0";
static constexpr char Texcoord1AttributeName[]   = "TEXCOORD_1";
static constexpr char JointsAttributeName[]      = "JOINTS_0";
static constexpr char WeightsAttributeName[]     = "WEIGHTS_0";
static constexpr char TangentAttributeName[]     = "TANGENT";

static constexpr float4 DefaultVertexColor{1, 1, 1, 1};

/// Default vertex attributes.
// clang-format off
static constexpr std::array<VertexAttributeDesc, 8> DefaultVertexAttributes =
    {
        VertexAttributeDesc{PositionAttributeName,  0, VT_FLOAT32, 3},
        VertexAttributeDesc{NormalAttributeName,    0, VT_FLOAT32, 3},
        VertexAttributeDesc{Texcoord0AttributeName, 0, VT_FLOAT32, 2},
        VertexAttributeDesc{Texcoord1AttributeName, 2, VT_FLOAT32, 2}, // Texcoord1 is rarely used, so store it in buffer 2

        VertexAttributeDesc{JointsAttributeName,  1, VT_FLOAT32, 4},
        VertexAttributeDesc{WeightsAttributeName, 1, VT_FLOAT32, 4},

        VertexAttributeDesc{VertexColorAttributeName, 3, VT_FLOAT32, 4, &DefaultVertexColor},
        VertexAttributeDesc{TangentAttributeName,     4, VT_FLOAT32, 3}
    };
// clang-format on

InputLayoutDescX VertexAttributesToInputLayout(const VertexAttributeDesc* pAttributes, size_t NumAttributes);


struct TextureCacheType
{
    std::mutex TexturesMtx;

    std::unordered_map<std::string, RefCntWeakPtr<ITexture>> Textures;
};

/// Model create information
struct ModelCreateInfo
{
    /// File name.
    const char* FileName = nullptr;

    /// Optional texture cache to use when loading the model.
    /// The loader will try to find all the textures in the cache
    /// and add all new textures to the cache.
    TextureCacheType* pTextureCache = nullptr;

    /// Optional resource manager to use when allocating resources for the model.
    ResourceManager* pResourceManager = nullptr;

    using NodeLoadCallbackType = std::function<void(const void* pSrcModel, int SrcNodeIndex, const void* pSrcNode, Node& DstNode)>;
    /// User-provided node loading callback function that will be called for
    /// every node being loaded.
    ///
    /// \param [in]  pSrcModel    - a pointer to the source GLTF model.
    /// \param [in]  SrcNodeIndex - an index of the node in the source GLTF model.
    /// \param [in]  pSrcNode     - a pointer to the source node.
    /// \param [out] DstNode      - reference to the destination node.
    ///
    /// \remarks    The application should cast pSrcNode to the appropriate type
    ///             depending on the loader it is using (e.g. tinygltf::Node*).
    NodeLoadCallbackType NodeLoadCallback = nullptr;

    using MeshLoadCallbackType = std::function<void(const void* pSrcModel, const void* pSrcMesh, Mesh& DstMesh)>;
    /// User-provided mesh loading callback function that will be called for
    /// every mesh being loaded.
    ///
    /// \param [in]  pSrcModel - a pointer to the source GLTF model.
    /// \param [in]  pSrcMesh  - a pointer to the source mesh.
    /// \param [out] DstMesh   - reference to the destination mesh.
    ///
    /// \remarks    The application should cast pSrcMesh to the appropriate type
    ///             depending on the loader it is using (e.g. tinygltf::Mesh*).
    MeshLoadCallbackType MeshLoadCallback = nullptr;

    using PrimitiveLoadCallbackType = std::function<void(const void* pSrcModel, const void* pSrcPrim, Primitive& DstPrim)>;
    /// User-provided primitive loading callback function that will be called for
    /// every primitive being loaded.
    ///
    /// \param [in]  pSrcModel - a pointer to the source model.
    /// \param [in]  pSrcPrim  - a pointer to the source primitive.
    /// \param [out] DstPrim   - reference to the destination primitive.
    ///
    /// \remarks    The application should cast pSrcPrim to the appropriate type
    ///             depending on the loader it is using (e.g. tinygltf::Primitive*).
    PrimitiveLoadCallbackType PrimitiveLoadCallback = nullptr;

    using MaterialLoadCallbackType = std::function<void(const void* pSrcModel, const void* pSrcMat, Material& DstMat)>;
    /// User-provided material loading callback function that will be called for
    /// every material being loaded.
    ///
    /// \param [in]  pSrcModel - a pointer to the source model.
    /// \param [in]  pSrcMat   - a pointer to the source material.
    /// \param [out] DstMat    - reference to the destination material.
    ///
    /// \remarks    The application should cast pSrcMat to the appropriate type
    ///             depending on the loader it is using (e.g. tinygltf::Material*).
    MaterialLoadCallbackType MaterialLoadCallback = nullptr;

    using FileExistsCallbackType = std::function<bool(const char* FilePath)>;
    /// Optional callback function that will be called by the loader to check if the file exists.
    FileExistsCallbackType FileExistsCallback = nullptr;

    using ReadWholeFileCallbackType = std::function<bool(const char* FilePath, std::vector<unsigned char>& Data, std::string& Error)>;
    /// Optional callback function that will be called by the loader to read the whole file.
    ReadWholeFileCallbackType ReadWholeFileCallback = nullptr;

    /// Index data type.
    VALUE_TYPE IndexType = VT_UINT32;

    /// Index buffer bind flags
    BIND_FLAGS IndBufferBindFlags = BIND_INDEX_BUFFER;

    static constexpr Uint32 MaxBuffers = 8;

    /// Vertex buffer bind flags for each buffer slot.
    BIND_FLAGS VertBufferBindFlags[MaxBuffers] = {};

    /// A pointer to the array of NumVertexAttributes vertex attributes defining
    /// the vertex layout.
    ///
    /// \remarks    If null is provided, default vertex attributes will be used (see DefaultVertexAttributes).
    const VertexAttributeDesc* VertexAttributes = nullptr;

    /// The number of elements in the VertexAttributes array.
    Uint32 NumVertexAttributes = 0;

    /// A pointer to the array of NumTextureAttributes texture attributes.
    ///
    /// \remarks    If null is provided, default vertex attributes will be used (see DefaultTextureAttributes).
    const TextureAttributeDesc* TextureAttributes = nullptr;

    /// The number of elements in the TextureAttributes array.
    Uint32 NumTextureAttributes = 0;

    /// Index of the scene to load. If -1, default scene will be loaded.
    Int32 SceneId = -1;

    /// Whether to compute primitive bounding boxes from vertex positions.
    ///
    /// \remarks    By default, primitive bounding boxes are defined by the
    ///             min/max values of the primitive's position accessor in the
    ///             source GLTF model. If this flag is set to true, the bounding
    ///             boxes will be computed from vertex positions instead.
    ///             This may be useful if the source model does not define
    ///             bounding boxes for its primitives or if the bounding boxes
    ///             are imprecise.
    bool ComputeBoundingBoxes = false;

    /// Whether to create stub vertex buffers even if the model
    /// does provide any attribute to store in the buffer.
    ///
    /// \remarks    By default, if the model does not provide any attribute
    ///             to store in the vertex buffer, the buffer will not be
    ///             created. However, an application may still request the
    ///             buffer to be created by setting this flag to true.
    ///             This may be useful if the application uses the same vertex
    ///             layout for all models and wants to avoid checking if the
    ///             buffer is null.
    ///             The buffer will be zero-initialized.
    bool CreateStubVertexBuffers = false;

    ModelCreateInfo() = default;

    explicit ModelCreateInfo(const char*                _FileName,
                             TextureCacheType*          _pTextureCache         = nullptr,
                             ResourceManager*           _pResourceManager      = nullptr,
                             MeshLoadCallbackType       _MeshLoadCallback      = nullptr,
                             MaterialLoadCallbackType   _MaterialLoadCallback  = nullptr,
                             FileExistsCallbackType     _FileExistsCallback    = nullptr,
                             ReadWholeFileCallbackType  _ReadWholeFileCallback = nullptr,
                             const VertexAttributeDesc* _VertexAttributes      = nullptr,
                             Uint32                     _NumVertexAttributes   = 0) :
        // clang-format off
            FileName             {_FileName},
            pTextureCache        {_pTextureCache},
            pResourceManager     {_pResourceManager},
            MeshLoadCallback     {_MeshLoadCallback},
            MaterialLoadCallback {_MaterialLoadCallback},
            FileExistsCallback   {_FileExistsCallback},
            ReadWholeFileCallback{_ReadWholeFileCallback},
            VertexAttributes     {_VertexAttributes},
            NumVertexAttributes  {_NumVertexAttributes}
    // clang-format on
    {
    }
};

struct ModelTransforms
{
    // Transform matrices for each node in the model.
    std::vector<float4x4> NodeLocalMatrices;
    std::vector<float4x4> NodeGlobalMatrices;

    struct SkinTransforms
    {
        std::vector<float4x4> JointMatrices;
    };
    std::vector<SkinTransforms> Skins;

    // Animation transforms for each node in the model.
    // This is an intermediate data to compute transform matrices.
    struct AnimationTransforms
    {
        float3      Translation;
        float3      Scale{1, 1, 1};
        QuaternionF Rotation;
    };
    std::vector<AnimationTransforms> NodeAnimations;
};

struct Model
{
    std::vector<Scene>       Scenes;
    std::vector<Node>        Nodes;
    std::vector<Mesh>        Meshes;
    std::vector<Camera>      Cameras;
    std::vector<Light>       Lights;
    std::vector<Skin>        Skins;
    std::vector<Material>    Materials;
    std::vector<Animation>   Animations;
    std::vector<std::string> Extensions;

    std::vector<RefCntAutoPtr<ISampler>> TextureSamplers;

    // The number of nodes that have skin.
    int SkinTransformsCount = 0;
    int DefaultSceneId      = 0;

    Model(const ModelCreateInfo& CI);

    Model(IRenderDevice*         pDevice,
          IDeviceContext*        pContext,
          const ModelCreateInfo& CI);

    Model() noexcept;

    ~Model();

    /// Prepares the model's GPU resources:
    /// * Uploads pending vertex and index data to the GPU buffers
    /// * Uploads textures to the GPU
    /// * If the model does not use the resource cache, transitions resources to required states
    void PrepareGPUResources(IRenderDevice* pDevice, IDeviceContext* pCtx);

    bool IsGPUDataInitialized() const
    {
        return GPUDataInitialized.load();
    }

    IBuffer* GetVertexBuffer(Uint32 Index, IRenderDevice* pDevice = nullptr, IDeviceContext* pCtx = nullptr) const
    {
        VERIFY_EXPR(Index < GetVertexBufferCount());
        if (VertexData.pAllocation != nullptr)
        {
            return pDevice != nullptr || pCtx != nullptr ?
                VertexData.pAllocation->Update(Index, pDevice, pCtx) :
                VertexData.pAllocation->GetBuffer(Index);
        }
        else
        {
            return VertexData.Buffers[Index];
        }
    }

    IBuffer* GetIndexBuffer(IRenderDevice* pDevice = nullptr, IDeviceContext* pCtx = nullptr) const
    {
        if (IndexData.pAllocation != nullptr)
        {
            return pDevice != nullptr || pCtx != nullptr ?
                IndexData.pAllocation->Update(pDevice, pCtx) :
                IndexData.pAllocation->GetBuffer();
        }
        else
        {
            return IndexData.pBuffer;
        }
    }

    ITexture* GetTexture(Uint32 Index, IRenderDevice* pDevice = nullptr, IDeviceContext* pCtx = nullptr) const
    {
        auto& TexInfo = Textures[Index];

        if (TexInfo.pTexture)
            return TexInfo.pTexture;

        if (TexInfo.pAtlasSuballocation)
        {
            if (auto* pAtlas = TexInfo.pAtlasSuballocation->GetAtlas())
            {
                return pDevice != nullptr || pCtx != nullptr ?
                    pAtlas->Update(pDevice, pCtx) :
                    pAtlas->GetTexture();
            }
            else
            {
                UNEXPECTED("Texture altas can't be null");
            }
        }

        return nullptr;
    }

    const TextureDesc& GetTextureDesc(Uint32 Index) const
    {
        if (Index < Textures.size())
        {
            const auto& TexInfo = Textures[Index];
            if (TexInfo.pTexture)
            {
                return TexInfo.pTexture->GetDesc();
            }
            else if (auto* pAtlas = (TexInfo.pAtlasSuballocation ? TexInfo.pAtlasSuballocation->GetAtlas() : nullptr))
            {
                return pAtlas->GetAtlasDesc();
            }
        }

        static const TextureDesc NullDesc{};
        return NullDesc;
    }

    Uint32 GetFirstIndexLocation() const
    {
        VERIFY(IndexData.IndexSize != 0, "Index size is not initialized");
        if (IndexData.pAllocation)
        {
            const auto Offset = IndexData.pAllocation->GetOffset();
            VERIFY((Offset % IndexData.IndexSize) == 0, "Index data allocation offset is not a multiple of index size (", IndexData.IndexSize, ")");
            return Offset / IndexData.IndexSize;
        }

        return 0;
    }

    Uint32 GetBaseVertex() const
    {
        return VertexData.pAllocation ?
            VertexData.pAllocation->GetStartVertex() :
            0;
    }

    /// Returns an index of the vertex pool in the resource manager.
    ///
    /// \remarks    This index should be passed to the GetVertexPool method of the resource manager.
    Uint32 GetVertexPoolIndex() const
    {
        return VertexData.PoolId;
    }

    /// Returns an index of the index buffer allocator in the resource manager.
    ///
    /// \remarks    This index should be passed to the GetIndexBuffer method of the resource manager.
    Uint32 GetIndexAllocatorIndex() const
    {
        return IndexData.AllocatorId;
    }

    struct ImageData
    {
        int Width         = 0;
        int Height        = 0;
        int NumComponents = 0;
        int ComponentSize = 0;

        TEXTURE_FORMAT    TexFormat = TEX_FORMAT_UNKNOWN;
        IMAGE_FILE_FORMAT FileFormat{};

        // Pixels are tightly packed.
        const void* pData    = nullptr;
        size_t      DataSize = 0;
    };
    Uint32 AddTexture(IRenderDevice*     pDevice,
                      TextureCacheType*  pTextureCache,
                      ResourceManager*   pResourceMgr,
                      const ImageData&   Image,
                      int                GltfSamplerId,
                      const std::string& CacheId);

    Uint32 GetNumVertexAttributes() const { return NumVertexAttributes; }
    Uint32 GetNumTextureAttributes() const { return NumTextureAttributes; }

    const auto& GetVertexAttribute(size_t Idx) const
    {
        VERIFY_EXPR(Idx < GetNumVertexAttributes());
        return VertexAttributes[Idx];
    }

    const auto& GetTextureAttribute(size_t Idx) const
    {
        VERIFY_EXPR(Idx < GetNumTextureAttributes());
        return TextureAttributes[Idx];
    }

    /// Returns the material texture attribute index in Material.ShaderAttribs for
    /// the given texture attribute name, or -1 if the attribute is not defined.
    /// For example, for default attributes:
    ///     "baseColorTexture"         -> 0
    ///     "metallicRoughnessTexture" -> 1
    ///     "normalTexture"            -> 2
    ///
    /// \note This index is NOT the texture index in Textures array. To get this index,
    ///       use Material.TextureIds[TextureAttributeIndex].
    int GetTextureAttributeIndex(const char* Name) const;

    bool CompatibleWithTransforms(const ModelTransforms& Transforms) const;

    void ComputeTransforms(Uint32           SceneIndex,
                           ModelTransforms& Transforms,
                           const float4x4&  RootTransform  = float4x4::Identity(),
                           Int32            AnimationIndex = -1,
                           float            Time           = 0) const;

    BoundBox ComputeBoundingBox(Uint32 SceneIndex, const ModelTransforms& Transforms) const;

    size_t GetTextureCount() const
    {
        return Textures.size();
    }

    size_t GetVertexBufferCount() const
    {
        return VertexData.Strides.size();
    }

    bool IsVertexAttributeEnabled(Uint32 AttribId) const
    {
        return (VertexData.EnabledAttributeFlags & (1u << AttribId)) != 0;
    }

    void InitMaterialTextureAddressingAttribs(Material& Mat, Uint32 TextureIndex);

private:
    friend ModelBuilder;

    void LoadFromFile(IRenderDevice*         pDevice,
                      IDeviceContext*        pContext,
                      const ModelCreateInfo& CI);

    void LoadTextures(IRenderDevice*         pDevice,
                      const tinygltf::Model& gltf_model,
                      const std::string&     BaseDir,
                      TextureCacheType*      pTextureCache,
                      ResourceManager*       pResourceMgr);

    void LoadTextureSamplers(IRenderDevice* pDevice, const tinygltf::Model& gltf_model);
    void LoadMaterials(const tinygltf::Model& gltf_model, const ModelCreateInfo::MaterialLoadCallbackType& MaterialLoadCallback);
    void UpdateAnimation(Uint32 SceneIndex, Uint32 AnimationIndex, float time, ModelTransforms& Transforms) const;

    // Returns the alpha cutoff value for the given texture.
    // TextureIdx is the texture index in the GLTF file and also the Textures array.
    float GetTextureAlphaCutoffValue(int TextureIdx) const;

private:
    std::atomic_bool GPUDataInitialized{false};

    std::unique_ptr<void, STDDeleter<void, IMemoryAllocator>> pAttributesData;

    const VertexAttributeDesc*  VertexAttributes  = nullptr;
    const TextureAttributeDesc* TextureAttributes = nullptr;

    Uint32 NumVertexAttributes  = 0;
    Uint32 NumTextureAttributes = 0;

    struct VertexDataInfo
    {
        std::vector<Uint32>                  Strides;
        std::vector<RefCntAutoPtr<IBuffer>>  Buffers;
        RefCntAutoPtr<IVertexPoolAllocation> pAllocation;
        Uint32                               PoolId                = 0; // Vertex pool index
        Uint32                               EnabledAttributeFlags = 0;
    };
    VertexDataInfo VertexData;

    struct IndexDataInfo
    {
        RefCntAutoPtr<IBuffer>              pBuffer;
        RefCntAutoPtr<IBufferSuballocation> pAllocation;

        Uint32 AllocatorId = 0; // Index buffer allocator index
        Uint32 IndexSize   = 0;
    };
    IndexDataInfo IndexData;

    struct TextureInfo
    {
        RefCntAutoPtr<ITexture>                   pTexture;
        RefCntAutoPtr<ITextureAtlasSuballocation> pAtlasSuballocation;

        explicit operator bool() const
        {
            return pTexture || pAtlasSuballocation;
        }
    };
    std::vector<TextureInfo> Textures;
};

template <typename T>
inline Matrix4x4<T> ComputeNodeLocalMatrix(const Vector3<T>&    Scale,
                                           const Quaternion<T>& Rotation,
                                           const Vector3<T>&    Translation,
                                           const Matrix4x4<T>&  Matrix)
{
    // Translation, rotation, and scale properties and local space transformation are
    // mutually exclusive as per GLTF spec.

    // LocalMatrix = S * R * T * M
    Matrix4x4<T> LocalMatrix = Matrix;

    if (Translation != Vector3<T>{})
        LocalMatrix = Matrix4x4<T>::Translation(Translation) * LocalMatrix;

    if (Rotation != Quaternion<T>{})
        LocalMatrix = Rotation.ToMatrix() * LocalMatrix;

    if (Scale != Vector3<T>{1, 1, 1})
        LocalMatrix = Matrix4x4<T>::Scale(Scale) * LocalMatrix;

    return LocalMatrix;
}

inline float4x4 Node::ComputeLocalTransform() const
{
    return ComputeNodeLocalMatrix(Scale, Rotation, Translation, Matrix);
}

inline size_t AnimationSampler::FindKeyFrame(float Time) const
{
    if (Inputs.size() <= 2)
        return 0;

    const auto input_it = std::lower_bound(Inputs.begin(), Inputs.end(), Time);

    size_t Idx = 0;
    if (input_it == Inputs.begin())
    {
        VERIFY_EXPR(Time <= Inputs.front());
        Idx = 0;
    }
    else if (input_it == Inputs.end())
    {
        VERIFY_EXPR(Time >= Inputs.back());
        Idx = Inputs.size() - 1;
    }
    else
    {
        Idx = static_cast<size_t>(std::distance(Inputs.begin(), input_it));
        VERIFY_EXPR(Idx > 0 && Idx < Inputs.size());
        --Idx;
        VERIFY_EXPR(Time >= Inputs[Idx] && Time <= Inputs[Idx + 1]);
    }

    return Idx;
}

} // namespace GLTF

} // namespace Diligent
