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

#include <random>

#include "Buildings.hpp"
#include "MapHelper.hpp"
#include "PlatformMisc.hpp"

namespace Diligent
{
namespace HLSL
{
#include "../assets/Structures.fxh"
}

namespace
{

float2 Hash22(const float2 p)
{
    auto p3 = float3{Frac(p.x * 0.1031f), Frac(p.y * 0.1030f), Frac(p.x * 0.0973f)};
    p3 -= float3{dot(p3, float3{p3.y, p3.z, p3.x} + float3{19.19f})};
    return float2{Frac((p3.x + p3.y) * p3.z), Frac((p3.x * p3.z) * p3.y)};
}

struct Vertex
{
    float3 Pos;
    float3 Norm;
    float3 UVW;

    bool HasNormals() const
    {
        return dot(Norm, Norm) > 0.99f;
    }
};
using IndexType = Uint32;

enum class TexLayerType
{
    Wall                    = 0,
    WallAndRightNeonLine    = 1,
    WallAndTopNeonLine      = 2,
    Windows                 = 3,
    WindowsAndRightNeonLine = 4,
    WindowsAndTopNeonLine   = 5,
    Count                   = 5, // ignore 'Wall'
};


struct Building
{
    float2 Center = {0.f, 0.f};
    float  Radius = 0.f;
    float  Height = 0.f;

    static float2 GenCenter(int2 iPos)
    {
        const auto iCenter = iPos.Recast<float>();
        float2     Offset  = Hash22(iCenter * float2{2.56135f} + float2{0.8234f}) * 0.5f;
        return iCenter + Offset;
    }

    static float GenHeight(int2 iPos)
    {
        return Hash22(iPos.Recast<float>() * float2{3.87324f} + float2{0.83257f}).x * 11.0f + 6.0f;
    }
};

void CreateBuilding(std::mt19937&           RndDev,
                    const float2            Center,
                    const float             MaxRadius,
                    float                   MaxHeight,
                    const Uint32            BaseTexIndex,
                    const Uint32            TexArraySize,
                    std::vector<Vertex>&    Vertices,
                    std::vector<IndexType>& Indices)
{
    enum class BuildingShape
    {
        PRISM = 0,              // extruded 2D edges, flat roof
        PRISM_PYRAMID,          // prism with pyramid-shaped roof
        PRISM_SECTIONS,         // single section - extruded 2D edges, each section has different scale
        PRISM_SECTIONS_OFFSET,  // single section - extruded 2D edges, each section has different scale and offset
        PRISM_ROTATED_SECTIONS, // single section - extruded 2D edges, each section has different scale and rotation
        TWIST,                  // twisted prism, variable scale of sections
        COUNT
    };

    enum
    {
        WINDOWS     = 1 << 0, // random windows with random color
        BIG_WINDOWS = 1 << 1, // windows for pyramid roof (not supported yet)
        ANY_WINDOWS = WINDOWS | BIG_WINDOWS,
        NEON_LEFT   = 1 << 2, // neon line in the left edge
        NEON_RIGHT  = 1 << 3, // neon line in the right edge
        NEON_BOTTOM = 1 << 4, // neon line in the bottom edge
        NEON_TOP    = 1 << 5, // neon line in the top edge
        ANY_NEON    = NEON_LEFT | NEON_RIGHT | NEON_BOTTOM | NEON_TOP,
        ALL_MASK    = WINDOWS | BIG_WINDOWS | ANY_NEON,
    };

    struct Corner
    {
        float2 Pos;
        Uint32 TexType = 0;

        Corner() {}
        Corner(float X, float Y, Uint32 Tex) :
            Pos{X, Y}, TexType{Tex} {}
    };

    // generate XZ cross section for building
    std::vector<Corner> Corners;
    BuildingShape       ShapeId = BuildingShape::COUNT;
    {
        std::uniform_int_distribution<int> ShapeIdDistrib{0, static_cast<int>(BuildingShape::COUNT) - 1};
        ShapeId = static_cast<BuildingShape>(ShapeIdDistrib(RndDev));

        const bool RandomCorners = (ShapeId == BuildingShape::TWIST);
        if (RandomCorners)
        {
            std::uniform_int_distribution<Uint32> NumEdgesDistrib{8u, 16u};
            std::uniform_real_distribution<float> RndRadius{0.1f, 0.99f};
            const Uint32                          NumEdges = NumEdgesDistrib(RndDev);

            std::vector<float2> AngleAndRadius;
            for (Uint32 e = 0; e < NumEdges; ++e)
            {
                float Angle  = (static_cast<float>(e) / NumEdges) * PI_F * 2.0f;
                float Radius = RndRadius(RndDev);
                AngleAndRadius.emplace_back(Angle, Radius);
            }

            for (Uint32 e = 0; e < NumEdges; ++e)
            {
                float  PrevRadius = AngleAndRadius[e == 0 ? NumEdges - 1 : e - 1].y;
                float  Radius     = AngleAndRadius[e].y;
                float  NextRadius = AngleAndRadius[e + 1 == NumEdges ? 0 : e + 1].y;
                float  Angle      = AngleAndRadius[e].x;
                float2 Pos        = float2{std::cos(Angle), std::sin(Angle)} * Radius;

                if (Radius > PrevRadius - 0.1 &&
                    Radius > NextRadius - 0.1 &&
                    Radius > 0.3)
                {
                    float  Angle1 = Angle - (0.3f / NumEdges);
                    float  Angle2 = Angle + (0.3f / NumEdges);
                    float2 Pos1   = float2{std::cos(Angle1), std::sin(Angle1)} * lerp(Radius, PrevRadius, 0.2f);
                    float2 Pos2   = float2{std::cos(Angle2), std::sin(Angle2)} * lerp(Radius, NextRadius, 0.2f);

                    Corners.emplace_back(Pos1.x, Pos1.y, ANY_WINDOWS);
                    Corners.emplace_back(Pos.x, Pos.y, ANY_WINDOWS | NEON_RIGHT);
                    Corners.emplace_back(Pos2.x, Pos2.y, ANY_WINDOWS | NEON_LEFT);
                }
                else
                {
                    Corners.emplace_back(Pos.x, Pos.y, ANY_WINDOWS);
                }
            }

            MaxHeight = std::max(MaxHeight + 6.f, 18.f);
        }
        else
        {
            std::uniform_real_distribution<float> ShapeTypeDistrib{0.0f, 1.0f};
            float                                 Type = ShapeTypeDistrib(RndDev);

            if (Type < 0.25f && !(ShapeId == BuildingShape::PRISM_SECTIONS_OFFSET || ShapeId == BuildingShape::PRISM_ROTATED_SECTIONS))
            {
                // cross
                Corners.emplace_back(+0.5f, +2.0f, ANY_WINDOWS | NEON_RIGHT);
                Corners.emplace_back(-0.5f, +2.0f, ANY_WINDOWS | NEON_TOP);
                Corners.emplace_back(-0.5f, +0.5f, ANY_WINDOWS | NEON_LEFT);
                Corners.emplace_back(-2.0f, +0.5f, ANY_WINDOWS | NEON_RIGHT);
                Corners.emplace_back(-2.0f, -0.5f, ANY_WINDOWS | NEON_TOP);
                Corners.emplace_back(-0.5f, -0.5f, ANY_WINDOWS | NEON_LEFT);
                Corners.emplace_back(-0.5f, -2.0f, ANY_WINDOWS | NEON_RIGHT);
                Corners.emplace_back(+0.5f, -2.0f, ANY_WINDOWS | NEON_TOP);
                Corners.emplace_back(+0.5f, -0.5f, ANY_WINDOWS | NEON_LEFT);
                Corners.emplace_back(+2.0f, -0.5f, ANY_WINDOWS | NEON_RIGHT);
                Corners.emplace_back(+2.0f, +0.5f, ANY_WINDOWS | NEON_TOP);
                Corners.emplace_back(+0.5f, +0.5f, ANY_WINDOWS | NEON_LEFT);
            }
            else if (Type < 0.85f)
            {
                // quad
                Corners.emplace_back(-1.f, -1.f, ANY_WINDOWS | NEON_RIGHT | NEON_TOP);
                Corners.emplace_back(+1.f, -1.f, ANY_WINDOWS | NEON_RIGHT | NEON_TOP);
                Corners.emplace_back(+1.f, +1.f, ANY_WINDOWS | NEON_RIGHT | NEON_TOP);
                Corners.emplace_back(-1.f, +1.f, ANY_WINDOWS | NEON_RIGHT | NEON_TOP);
            }
            else
            {
                // pentagon
                Corners.emplace_back(+0.0f, +1.0f, ANY_WINDOWS | NEON_RIGHT | NEON_TOP);
                Corners.emplace_back(-1.0f, +0.2f, ANY_WINDOWS | NEON_RIGHT | NEON_TOP);
                Corners.emplace_back(-0.7f, -1.0f, ANY_WINDOWS | NEON_RIGHT | NEON_TOP);
                Corners.emplace_back(+0.7f, -1.0f, ANY_WINDOWS | NEON_RIGHT | NEON_TOP);
                Corners.emplace_back(+1.0f, +0.2f, ANY_WINDOWS | NEON_RIGHT | NEON_TOP);
            }
        }
    }


    // Generate building sections
    struct Section
    {
        float  Scale1 = 1.f;
        float  Scale2 = 1.f;
        float  Height = 0.f;
        float  Angle1 = 0.f;
        float  Angle2 = 0.f;
        float2 CenterOffset;
        Uint32 SupportedTex = 0;
        Uint32 TexIndex     = 0;
    };
    std::vector<Section> Sections;
    {
        std::uniform_int_distribution<Uint32> NumSectionsDistrib{2u, 7u};
        std::uniform_int_distribution<Uint32> RndTexIndex{0u, 32u};
        std::uniform_real_distribution<float> RndAngle{0.0f, PI_F * 0.5f};
        std::uniform_real_distribution<float> RndScale{0.9f, 1.1f};

        switch (ShapeId)
        {
            case BuildingShape::PRISM:
            {
                Section sc;
                sc.TexIndex = BaseTexIndex + RndTexIndex(RndDev);

                sc.Height       = MaxHeight;
                sc.SupportedTex = ALL_MASK;
                Sections.push_back(sc);

                sc.Scale2       = 0.f;
                sc.Height       = 0.f;
                sc.SupportedTex = 0;
                Sections.push_back(sc);
                break;
            }
            case BuildingShape::PRISM_PYRAMID:
            {
                Section sc;
                sc.TexIndex = BaseTexIndex + RndTexIndex(RndDev);

                sc.Height       = MaxHeight;
                sc.SupportedTex = ALL_MASK;
                Sections.push_back(sc);

                sc.Scale2       = 0.f;
                sc.Height       = 2.f;
                sc.SupportedTex = 0;
                Sections.push_back(sc);
                break;
            }
            case BuildingShape::PRISM_SECTIONS:
            {
                const auto NumSections = std::min(NumSectionsDistrib(RndDev), static_cast<Uint32>(MaxHeight * 0.25f));
                for (Uint32 s = 0; s < NumSections; ++s)
                {
                    Section sc;
                    sc.TexIndex = BaseTexIndex + RndTexIndex(RndDev);

                    sc.Scale1       = 0.f;
                    sc.Scale2       = RndScale(RndDev);
                    sc.SupportedTex = 0;
                    Sections.push_back(sc);

                    sc.Scale1       = sc.Scale2;
                    sc.Height       = MaxHeight / NumSections;
                    sc.SupportedTex = ALL_MASK;
                    Sections.push_back(sc);

                    sc.Scale2       = 0.f;
                    sc.Height       = 0.f;
                    sc.SupportedTex = 0;
                    Sections.push_back(sc);
                }
                break;
            }
            case BuildingShape::PRISM_SECTIONS_OFFSET:
            {
                const auto NumSections  = std::min(NumSectionsDistrib(RndDev), static_cast<Uint32>(MaxHeight * 0.25f));
                float2     CenterOffset = {};
                for (Uint32 s = 0; s < NumSections; ++s)
                {
                    Section sc;
                    float   a = RndAngle(RndDev) * 4.f;
                    CenterOffset += float2{std::cos(a), std::sin(a)} * 0.5f;
                    sc.TexIndex = BaseTexIndex + RndTexIndex(RndDev);

                    sc.Scale1       = 0.f;
                    sc.Scale2       = RndScale(RndDev);
                    sc.CenterOffset = CenterOffset;
                    sc.SupportedTex = 0;
                    Sections.push_back(sc);

                    sc.Scale1       = sc.Scale2;
                    sc.Height       = MaxHeight / NumSections;
                    sc.SupportedTex = ALL_MASK;
                    Sections.push_back(sc);

                    sc.Scale2       = 0.f;
                    sc.Height       = 0.f;
                    sc.SupportedTex = 0;
                    Sections.push_back(sc);
                }
                break;
            }
            case BuildingShape::PRISM_ROTATED_SECTIONS:
            {
                const auto NumSections = std::min(NumSectionsDistrib(RndDev), static_cast<Uint32>(MaxHeight * 0.25f));
                for (Uint32 s = 0; s < NumSections; ++s)
                {
                    Section sc;
                    sc.TexIndex = BaseTexIndex + RndTexIndex(RndDev);

                    sc.Scale1       = 0.f;
                    sc.Scale2       = RndScale(RndDev);
                    sc.Angle1       = RndAngle(RndDev);
                    sc.Angle2       = sc.Angle1;
                    sc.SupportedTex = 0;
                    Sections.push_back(sc);

                    sc.Scale1       = sc.Scale2;
                    sc.Height       = MaxHeight / NumSections;
                    sc.SupportedTex = ALL_MASK;
                    Sections.push_back(sc);

                    sc.Scale2       = 0.f;
                    sc.Height       = 0.f;
                    sc.SupportedTex = 0;
                    Sections.push_back(sc);
                }
                break;
            }
            case BuildingShape::TWIST:
            {
                const Uint32 NumSections   = static_cast<Uint32>(MaxHeight * 10.f);
                const float  AngleDir      = NumSectionsDistrib(RndDev) & 1 ? 1.0f : -1.0f;
                const float  RotationScale = PI_F * MaxHeight * 0.04f * AngleDir;

                for (Uint32 s = 0; s < NumSections; ++s)
                {
                    Section     sc;
                    const float y1 = static_cast<float>(s) / NumSections;
                    const float y2 = static_cast<float>(s + 1) / NumSections;

                    sc.TexIndex     = BaseTexIndex + RndTexIndex(RndDev);
                    sc.Angle1       = y1 * RotationScale;
                    sc.Scale1       = sin(y1 * 2.14f + 1.0f);
                    sc.Angle2       = y2 * RotationScale;
                    sc.Scale2       = sin(y2 * 2.14f + 1.0f);
                    sc.Height       = MaxHeight / NumSections;
                    sc.SupportedTex = ALL_MASK;

                    Sections.push_back(sc);
                }
                break;
            }
            default:
                UNEXPECTED("Unexpected shape type");
        }
    }


    // Generate vertices & indices
    const auto NumCornerPoints = Corners.size() * 2;

    const auto AddFloor = [&](float Height, float Scale, float2 CenterOffset, float Rotation, Uint32 TexMask, Uint32 TexIndex, bool AddIndices) //
    {
        const float2x2 RotationMat = float2x2::Rotation(Rotation);

        for (Uint32 e = 0; e < Corners.size(); ++e)
        {
            Vertex v;
            v.Pos.x = Corners[e].Pos.x * MaxRadius * Scale;
            v.Pos.z = Corners[e].Pos.y * MaxRadius * Scale;
            v.Pos.y = Height;

            float2 p = float2{v.Pos.x, v.Pos.z} * RotationMat;
            v.Pos.x  = p.x + Center.x + CenterOffset.x;
            v.Pos.z  = p.y + Center.y + CenterOffset.y;

            // Same position, but different normal and uv
            Vertices.push_back(v);
            Vertices.push_back(v);
        }

        if (AddIndices)
        {
            for (Uint32 e = 0; e < NumCornerPoints; e += 2)
            {
                // First and last vertice produce the last quad
                const auto Left   = e + 1;
                const auto Right  = (e + 2 == NumCornerPoints) ? 0 : e + 2;
                const auto Top    = static_cast<Uint32>(Vertices.size() - NumCornerPoints);
                const auto Bottom = static_cast<Uint32>(Top - NumCornerPoints);

                Indices.push_back(Bottom + Left);
                Indices.push_back(Top + Left);
                Indices.push_back(Bottom + Right);

                Indices.push_back(Top + Left);
                Indices.push_back(Top + Right);
                Indices.push_back(Bottom + Right);

                auto& LB = Vertices[Bottom + Left];
                auto& LT = Vertices[Top + Left];
                auto& RB = Vertices[Bottom + Right];
                auto& RT = Vertices[Top + Right];

                // Calculate normals
                {
                    float3 n = normalize(cross(RB.Pos - LB.Pos, LT.Pos - LB.Pos));
                    if (std::isnan(n.x))
                        n = normalize(cross(RB.Pos - RT.Pos, LT.Pos - RT.Pos));
                    n.y = -n.y;

                    VERIFY_EXPR(!std::isnan(n.x));
                    LB.Norm = LT.Norm = RB.Norm = RT.Norm = n;
                }

                // Calculate UV
                {
                    const Uint32 TexType    = Corners[Right / 2].TexType & TexMask;
                    const float  UVScale    = 0.5f / MaxRadius;
                    const float  USize      = std::min(2.f, std::max(0.f, length(LB.Pos - RB.Pos)) * UVScale);
                    const float  VSize      = std::max(0.f, length(LB.Pos - LT.Pos)) * UVScale;
                    const bool   HasWindows = (TexType & ANY_WINDOWS) != 0;

                    TexLayerType TexLayer = HasWindows ? TexLayerType::Windows : TexLayerType::Wall;
                    if (TexType & (NEON_LEFT | NEON_RIGHT))
                        TexLayer = HasWindows ? TexLayerType::WindowsAndRightNeonLine : TexLayerType::WallAndRightNeonLine;
                    else if (TexType & (NEON_BOTTOM | NEON_TOP))
                        TexLayer = HasWindows ? TexLayerType::WindowsAndTopNeonLine : TexLayerType::WallAndTopNeonLine;

                    float Layer = 0.f;
                    if (TexLayer != TexLayerType::Wall)
                    {
                        VERIFY_EXPR((TexArraySize - 1) % static_cast<Uint32>(TexLayerType::Count) == 0);

                        Uint32 Slice = static_cast<Uint32>(TexLayer) + static_cast<Uint32>(TexLayerType::Count) * TexIndex;
                        Slice        = (Slice - 1) % (TexArraySize - 1) + 1;
                        Layer        = static_cast<float>(Slice);

                        TexLayerType TexType2 = static_cast<TexLayerType>((Slice - 1) % static_cast<Uint32>(TexLayerType::Count) + 1);
                        VERIFY_EXPR(TexLayer == TexType2);
                    }

                    // clang-format off
                    LB.UVW = float3{1.0f - USize, 1.0f - VSize, Layer};
                    RB.UVW = float3{1.0f,         1.0f - VSize, Layer};
                    LT.UVW = float3{1.0f - USize, 1.0f,         Layer};
                    RT.UVW = float3{1.0f,         1.0f,         Layer};
                    // clang-format on

                    // left to right
                    if (TexType & NEON_LEFT)
                    {
                        LB.UVW.x = LT.UVW.x = 1.0f;
                        RB.UVW.x = RT.UVW.x = 1.0f - USize;
                    }

                    // bottom to top
                    if (TexType & NEON_BOTTOM)
                    {
                        LB.UVW.y = RB.UVW.y = 1.0f;
                        LT.UVW.y = RT.UVW.y = 1.0f - VSize;
                    }
                }
            }
        }
    };


    float BuildingHeight = 0.0f;
    for (auto& sc : Sections)
    {
        AddFloor(BuildingHeight, sc.Scale1, sc.CenterOffset, sc.Angle1, sc.SupportedTex, sc.TexIndex, false);
        BuildingHeight += sc.Height;
        AddFloor(BuildingHeight, sc.Scale2, sc.CenterOffset, sc.Angle2, sc.SupportedTex, sc.TexIndex, true);
    }
}

} // namespace

void Buildings::CreateResources(IDeviceContext* pContext)
{
    const bool   HasTransferCtx  = PlatformMisc::CountOneBits(m_ImmediateContextMask) > 1;
    const Uint32 NumUniqueSlices = 1u + static_cast<Uint32>(TexLayerType::Count) * (HasTransferCtx ? 60 : 8);

    const int             GridSize = m_DistributionGridSize;
    std::vector<Building> TempCityGrid;
    TempCityGrid.resize(static_cast<size_t>(GridSize) * static_cast<size_t>(GridSize));

    for (int y = 0; y < GridSize; ++y)
    {
        for (int x = 0; x < GridSize; ++x)
        {
            Building&  b       = TempCityGrid[x + y * GridSize];
            const int2 iCenter = {x, y};

            b.Center = Building::GenCenter(iCenter);
            b.Height = Building::GenHeight(iCenter);
        }
    }

    std::vector<Building> CityGrid;
    for (int y = 0; y < GridSize; ++y)
    {
        for (int x = 0; x < GridSize; ++x)
        {
            Building& b       = TempCityGrid[x + y * GridSize];
            float     MinDist = std::numeric_limits<float>::max();

            for (int dy = -1; dy <= 1; ++dy)
            {
                for (int dx = -1; dx <= 1; ++dx)
                {
                    const int2 iCenter = {x + dx, y + dy};
                    if (iCenter.x >= 0 && iCenter.y >= 0 && iCenter.x < GridSize && iCenter.y < GridSize)
                    {
                        const Building& Other = TempCityGrid[iCenter.x + iCenter.y * GridSize];
                        if (&b != &Other)
                        {
                            float dist = length(b.Center - Other.Center);
                            MinDist    = std::min(MinDist, dist);
                        }
                    }
                }
            }

            if (MinDist > 0.94f)
            {
                b.Radius = std::min(MinDist, 1.f) * 0.25f;
                CityGrid.push_back(b);
            }
        }
    }

    VERIFY_EXPR(CityGrid.size() > 0);

    // Use default seed to produce consistent distributions
    std::mt19937 RndDev;

    std::vector<Vertex>    Vertices;
    std::vector<IndexType> Indices;
    const float            Scale = m_DistributionScale;
    for (Uint32 i = 0; i < CityGrid.size(); ++i)
    {
        auto& b = CityGrid[i];
        CreateBuilding(RndDev, b.Center * Scale, b.Radius * Scale, b.Height, i, NumUniqueSlices, Vertices, Indices);
    }

    // Create vertex & index buffers for opaque geometry
    {
        BufferDesc BuffDesc;
        BuffDesc.Name      = "Buildings opaque VB";
        BuffDesc.Size      = static_cast<Uint64>(Vertices.size() * sizeof(Vertices[0]));
        BuffDesc.BindFlags = BIND_VERTEX_BUFFER;
        BuffDesc.Usage     = USAGE_IMMUTABLE;
        BufferData BuffData{Vertices.data(), BuffDesc.Size, pContext};
        m_Device->CreateBuffer(BuffDesc, &BuffData, &m_OpaqueVB);

        BuffDesc.Name      = "Buildings opaque IB";
        BuffDesc.Size      = static_cast<Uint64>(Indices.size() * sizeof(Indices[0]));
        BuffDesc.BindFlags = BIND_INDEX_BUFFER;
        BuffData           = BufferData{Indices.data(), BuffDesc.Size, pContext};
        m_Device->CreateBuffer(BuffDesc, &BuffData, &m_OpaqueIB);

        const StateTransitionDesc Barriers[] = {
            {m_OpaqueVB, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_VERTEX_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
            {m_OpaqueIB, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_INDEX_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE} //
        };
        pContext->TransitionResourceStates(_countof(Barriers), Barriers);
    }

    // Create diffuse texture atlas for opaque geometry
    {
        TextureDesc TexDesc;
        TexDesc.Name      = "Buildings texture atlas";
        TexDesc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
        TexDesc.Format    = TEX_FORMAT_RGBA8_UNORM;
        TexDesc.BindFlags = BIND_SHADER_RESOURCE;
        TexDesc.ArraySize = NumUniqueSlices;

#if PLATFORM_ANDROID || PLATFORM_IOS
        TexDesc.MipLevels = 8;
#else
        TexDesc.MipLevels = 9;
#endif
        TexDesc.Width  = 1u << TexDesc.MipLevels;
        TexDesc.Height = 1u << TexDesc.MipLevels;

        TexDesc.ImmediateContextMask = m_ImmediateContextMask;
        m_Device->CreateTexture(TexDesc, nullptr, &m_OpaqueTexAtlas);

        m_OpaqueTexAtlasDefaultState = RESOURCE_STATE_COPY_DEST;

        // DirectX 12 requires resource to be in COMMON state for transition between graphics and transfer queue.
        if (m_Device->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_D3D12)
            m_OpaqueTexAtlasDefaultState = RESOURCE_STATE_COMMON;

        const StateTransitionDesc Barrier = {m_OpaqueTexAtlas, RESOURCE_STATE_UNKNOWN, m_OpaqueTexAtlasDefaultState};
        pContext->TransitionResourceStates(1, &Barrier);

        // Resource is used in multiple contexts, so disable automatic resource transitions.
        m_OpaqueTexAtlas->SetState(RESOURCE_STATE_UNKNOWN);

#if USE_STAGING_TEXTURE
        TexDesc.Name           = "Buildings staging texture atlas";
        TexDesc.BindFlags      = BIND_NONE;
        TexDesc.Usage          = USAGE_STAGING;
        TexDesc.CPUAccessFlags = CPU_ACCESS_WRITE;
        m_Device->CreateTexture(TexDesc, nullptr, &m_OpaqueTexAtlasStaging);

        VERIFY_EXPR((m_OpaqueTexAtlasStaging->GetState() & RESOURCE_STATE_COPY_SOURCE) != 0);
        m_OpaqueTexAtlasStaging->SetState(RESOURCE_STATE_UNKNOWN);
#endif

        Uint32 SliceSize = 0;
        for (Uint32 Mip = 0; Mip < TexDesc.MipLevels; ++Mip)
            SliceSize += std::max(1u, TexDesc.Width >> Mip) * std::max(1u, TexDesc.Height >> Mip);

        m_OpaqueTexAtlasPixels.resize(size_t{SliceSize} * size_t{TexDesc.ArraySize});
        m_GenTexTask.Pixels.resize(SliceSize);
        m_OpaqueTexAtlasSliceSize = SliceSize * 4;

        // Initialize content
        GenerateOpaqueTexture();
        Uint32 Unused;
        UpdateAtlas(pContext, ~0u, Unused);
        pContext->Flush();

        // Begin texture generation in async thread
        {
            m_GenTexTask.ArraySlice = 0;
            m_GenTexTask.Time       = CurrentTime;

            const auto OldStatus = m_GenTexTask.Status.exchange(TaskStatus::NewTask, std::memory_order_release);
            VERIFY_EXPR(OldStatus == TaskStatus::Initial);
        }
    }

    m_DrawOpaqueSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_OpaqueTexAtlas")->Set(m_OpaqueTexAtlas->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
}

void Buildings::Initialize(IRenderDevice* pDevice, IBuffer* pDrawConstants, Uint64 ImmediateContextMask)
{
    m_Device               = pDevice;
    m_DrawConstants        = pDrawConstants;
    m_ImmediateContextMask = ImmediateContextMask;

#if USE_STAGING_TEXTURE
    FenceDesc FenceCI;
    FenceCI.Name = "Upload complete fence";
    FenceCI.Type = FENCE_TYPE_CPU_WAIT_ONLY;
    m_Device->CreateFence(FenceCI, &m_UploadCompleteFence);
#endif
}

void Buildings::CreatePSO(const ScenePSOCreateAttribs& Attr)
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    PSOCreateInfo.PSODesc.Name         = "Draw Building PSO";
    PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    PSOCreateInfo.GraphicsPipeline.NumRenderTargets             = 1;
    PSOCreateInfo.GraphicsPipeline.RTVFormats[0]                = Attr.ColorTargetFormat;
    PSOCreateInfo.GraphicsPipeline.DSVFormat                    = Attr.DepthTargetFormat;
    PSOCreateInfo.GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = True;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.pShaderSourceStreamFactory = Attr.pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc       = {"Draw Building VS", SHADER_TYPE_VERTEX, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "DrawBuilding.vsh";
        m_Device->CreateShader(ShaderCI, &pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc       = {"Draw Building PS", SHADER_TYPE_PIXEL, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "DrawBuilding.psh";
        m_Device->CreateShader(ShaderCI, &pPS);
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    const LayoutElement LayoutElems[] =
        {
            LayoutElement{0, 0, 3, VT_FLOAT32, False},
            LayoutElement{1, 0, 3, VT_FLOAT32, False},
            LayoutElement{2, 0, 3, VT_FLOAT32, False} //
        };
    PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

#if PLATFORM_ANDROID
    const SamplerDesc SamLinearUVClampWWrapDesc //
        {
            FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
            TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP //
        };
#else
    const SamplerDesc SamLinearUVClampWWrapDesc //
        {
            FILTER_TYPE_ANISOTROPIC, FILTER_TYPE_ANISOTROPIC, FILTER_TYPE_ANISOTROPIC,
            TEXTURE_ADDRESS_MIRROR, TEXTURE_ADDRESS_WRAP, TEXTURE_ADDRESS_WRAP, 0.f, 8 //
        };
#endif
    const ImmutableSamplerDesc ImtblSamplers[] = //
        {
            {SHADER_TYPE_PIXEL, "g_OpaqueTexAtlas", SamLinearUVClampWWrapDesc} //
        };
    PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
    PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    PSOCreateInfo.PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    m_Device->CreateGraphicsPipelineState(PSOCreateInfo, &m_DrawOpaquePSO);
    m_DrawOpaquePSO->CreateShaderResourceBinding(&m_DrawOpaqueSRB);

    m_DrawOpaqueSRB->GetVariableByName(SHADER_TYPE_VERTEX, "DrawConstantsCB")->Set(m_DrawConstants);
    m_DrawOpaqueSRB->GetVariableByName(SHADER_TYPE_PIXEL, "DrawConstantsCB")->Set(m_DrawConstants);
}

void Buildings::Draw(IDeviceContext* pContext)
{
    pContext->BeginDebugGroup("Draw buildings");

    pContext->SetPipelineState(m_DrawOpaquePSO);

    // m_OpaqueTexAtlas can not be transitioned here because it is in UNKNOWN state.
    // Other resources are in constant state and do not require transitions.
    pContext->CommitShaderResources(m_DrawOpaqueSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    // Vertex and index buffers are immutable and do not require transitions.
    IBuffer* VBs[] = {m_OpaqueVB};
    pContext->SetVertexBuffers(0, _countof(VBs), VBs, nullptr, RESOURCE_STATE_TRANSITION_MODE_VERIFY, SET_VERTEX_BUFFERS_FLAG_RESET);
    pContext->SetIndexBuffer(m_OpaqueIB, 0, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    DrawIndexedAttribs drawAttribs;
    drawAttribs.NumIndices = static_cast<Uint32>(m_OpaqueIB->GetDesc().Size / sizeof(IndexType));
    drawAttribs.IndexType  = VT_UINT32;
    drawAttribs.Flags      = DRAW_FLAG_VERIFY_ALL;
    pContext->DrawIndexed(drawAttribs);

    pContext->EndDebugGroup(); // Draw buildings
}

void Buildings::BeforeDraw(IDeviceContext* pContext, const SceneDrawAttribs& Attr)
{
    // Update constants
    {
        const float Center = -m_DistributionGridSize * m_DistributionScale * 0.5f;

        MapHelper<HLSL::DrawConstants> ConstData(pContext, m_DrawConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        ConstData->ModelViewProj = (float4x4::Translation(Center, 0.f, Center) * Attr.ViewProj).Transpose();
        ConstData->NormalMat     = float4x4::Identity();
        ConstData->LightDir      = Attr.LightDir;
        ConstData->AmbientLight  = Attr.AmbientLight;
    }

    // Resources must be manually transitioned to required state.
    // Vulkan:     correct pipeline barrier must contain pixel shader stages, which are not supported in transfer context.
    // DirectX 12: the texture is used as a pixel shader resource and must be transitioned in graphics context.
    const StateTransitionDesc Barriers[] = {
        {m_OpaqueTexAtlas, m_OpaqueTexAtlasDefaultState, RESOURCE_STATE_SHADER_RESOURCE},
        {m_DrawConstants, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE} //
    };
    pContext->TransitionResourceStates(_countof(Barriers), Barriers);
}

void Buildings::AfterDraw(IDeviceContext* pContext)
{
    // Resources must be manually transitioned to required state.
    const StateTransitionDesc Barrier{m_OpaqueTexAtlas, RESOURCE_STATE_SHADER_RESOURCE, m_OpaqueTexAtlasDefaultState};
    pContext->TransitionResourceStates(1, &Barrier);
}

// Alpha component - brightness of self-emission
static constexpr Uint32 WallColor = F4Color_To_RGBA8Unorm({0.225f, 0.125f, 0.025f, 0.0f});

static constexpr float WindowEmission = 0.04f;
static constexpr float NeonEmission   = 0.16f;

static constexpr Uint32 WindowColors[] = {
    F4Color_To_RGBA8Unorm({0.98f, 0.92f, 0.51f, WindowEmission}),
    WallColor,
    WallColor,
    F4Color_To_RGBA8Unorm({0.77f, 1.00f, 0.97f, WindowEmission}),
    F4Color_To_RGBA8Unorm({1.00f, 0.87f, 0.66f, WindowEmission}),
    WallColor,
    F4Color_To_RGBA8Unorm({1.00f, 0.64f, 0.99f, WindowEmission}),
    WallColor,
    F4Color_To_RGBA8Unorm({0.95f, 0.95f, 0.95f, WindowEmission}),
    F4Color_To_RGBA8Unorm({0.87f, 0.99f, 0.61f, WindowEmission}),
    WallColor,
    WallColor //
};

static constexpr Uint32 NeonColors[] = {
    F4Color_To_RGBA8Unorm({0.900f, 0.376f, 0.940f, NeonEmission}),
    F4Color_To_RGBA8Unorm({1.000f, 0.200f, 0.200f, NeonEmission}),
    F4Color_To_RGBA8Unorm({0.250f, 0.930f, 0.950f, NeonEmission}),
    F4Color_To_RGBA8Unorm({0.970f, 0.470f, 0.168f, NeonEmission}),
    F4Color_To_RGBA8Unorm({0.208f, 0.953f, 0.188f, NeonEmission}) //
};

static constexpr Uint32 WindowSizePxX          = 8;
static constexpr Uint32 WindowSizePxY          = 4;
static constexpr Uint32 WindowWithBorderSizePx = 16;

static constexpr Uint32 NeonLineSize       = 16;
static constexpr Uint32 NeonLineBorder1    = 12;
static constexpr Uint32 NeonLineBorder2    = 4;
static constexpr Uint32 NeonLineWithBorder = NeonLineBorder1 + NeonLineSize + NeonLineBorder2;


static void GenWallTexture(Uint32* Pixels, const Uint32 W, const Uint32 H, const Uint32 Hash)
{
    for (Uint32 y = 0; y < H; ++y)
    {
        for (Uint32 x = 0; x < W; ++x)
        {
            Pixels[x + y * W] = WallColor;
        }
    }
}

static void GenWallAndRightNeonLineTexture(Uint32* Pixels, const Uint32 W, const Uint32 H, const Uint32 Hash, const Uint32 Hash2)
{
    GenWallTexture(Pixels, W, H, Hash);

    Uint32 ColIndex = Hash2 ^ (Hash2 >> 4);
    ColIndex        = ColIndex % _countof(NeonColors);

    for (Uint32 y = 0; y < H; ++y)
    {
        for (Uint32 x = W - NeonLineWithBorder; x < W; ++x)
        {
            const Uint32 lx   = x - (W - NeonLineWithBorder);
            const auto   col  = (lx >= NeonLineBorder1 && lx < NeonLineBorder1 + NeonLineSize) ? NeonColors[ColIndex] : WallColor;
            Pixels[x + y * W] = col;
        }
    }
}

static void GenWallAndTopNeonLineTexture(Uint32* Pixels, const Uint32 W, const Uint32 H, const Uint32 Hash, const Uint32 Hash2)
{
    GenWallTexture(Pixels, W, H, Hash);

    Uint32 ColIndex = Hash2 ^ (Hash2 >> 4);
    ColIndex        = ColIndex % _countof(NeonColors);

    for (Uint32 y = H - NeonLineWithBorder; y < H; ++y)
    {
        for (Uint32 x = 0; x < W; ++x)
        {
            const Uint32 ly   = y - (H - NeonLineWithBorder);
            const auto   col  = (ly >= NeonLineBorder1 && ly < NeonLineBorder1 + NeonLineSize) ? NeonColors[ColIndex] : WallColor;
            Pixels[x + y * W] = col;
        }
    }
}

inline Uint32 Combine(Uint32 Lhs, Uint32 Rhs)
{
    return Lhs ^ ((Rhs << 8) | (Rhs >> 8));
}

static void GenWindowsTexture(Uint32* Pixels, const Uint32 W, const Uint32 H, const Uint32 Hash)
{
    const Uint32 WndOffsetX = (WindowWithBorderSizePx - WindowSizePxX) / 2;
    const Uint32 WndOffsetY = (WindowWithBorderSizePx - WindowSizePxY) / 2;

    for (Uint32 y = 0; y < H; ++y)
    {
        for (Uint32 x = 0; x < W; ++x)
        {
            Uint32 col = WallColor;

            Uint32 lx = x % WindowWithBorderSizePx;
            Uint32 ly = y % WindowWithBorderSizePx;

            if ((lx >= WndOffsetX && lx < WindowSizePxX + WndOffsetX) &&
                (ly >= WndOffsetY && ly < WindowSizePxY + WndOffsetY))
            {
                Uint32 ColIndex = Combine(0u, (x / WindowWithBorderSizePx) * 0x5a2);
                ColIndex        = Combine(ColIndex, (y / WindowWithBorderSizePx) * 0x9e3);
                ColIndex        = Combine(ColIndex, Hash * 0x681);

                col = WindowColors[ColIndex % _countof(WindowColors)];
            }

            Pixels[x + y * W] = col;
        }
    }
}

static void GenWindowsAndRightNeonLineTexture(Uint32* Pixels, const Uint32 W, const Uint32 H, const Uint32 Hash, const Uint32 Hash2)
{
    GenWindowsTexture(Pixels, W, H, Hash);

    Uint32 ColIndex = Hash2 ^ (Hash2 >> 4);
    ColIndex        = ColIndex % _countof(NeonColors);

    for (Uint32 y = 0; y < H; ++y)
    {
        for (Uint32 x = W - NeonLineWithBorder; x < W; ++x)
        {
            const Uint32 lx   = x - (W - NeonLineWithBorder);
            const auto   col  = (lx >= NeonLineBorder1 && lx < NeonLineBorder1 + NeonLineSize) ? NeonColors[ColIndex] : WallColor;
            Pixels[x + y * W] = col;
        }
    }
}

static void GenWindowsAndTopNeonLineTexture(Uint32* Pixels, const Uint32 W, const Uint32 H, const Uint32 Hash, const Uint32 Hash2)
{
    GenWindowsTexture(Pixels, W, H, Hash);

    Uint32 ColIndex = Hash2 ^ (Hash2 >> 4);
    ColIndex        = ColIndex % _countof(NeonColors);

    for (Uint32 y = H - NeonLineWithBorder; y < H; ++y)
    {
        for (Uint32 x = 0; x < W; ++x)
        {
            const Uint32 ly   = y - (H - NeonLineWithBorder);
            const auto   col  = (ly >= NeonLineBorder1 && ly < NeonLineBorder1 + NeonLineSize) ? NeonColors[ColIndex] : WallColor;
            Pixels[x + y * W] = col;
        }
    }
}

static void GenMipmap(const Uint32* SrcPixels, const Uint32 SrcW, const Uint32 SrcH, Uint32* DstPixels, const Uint32 DstW, const Uint32 DstH)
{
    VERIFY_EXPR(SrcW >= 2 && SrcH >= 2);

    for (Uint32 y = 0; y < DstH; ++y)
    {
        for (Uint32 x = 0; x < DstW; ++x)
        {
            float4 c0  = RGBA8Unorm_To_F4Color(SrcPixels[(x * 2 + 0) + (y * 2 + 0) * SrcW]);
            float4 c1  = RGBA8Unorm_To_F4Color(SrcPixels[(x * 2 + 1) + (y * 2 + 0) * SrcW]);
            float4 c2  = RGBA8Unorm_To_F4Color(SrcPixels[(x * 2 + 0) + (y * 2 + 1) * SrcW]);
            float4 c3  = RGBA8Unorm_To_F4Color(SrcPixels[(x * 2 + 1) + (y * 2 + 1) * SrcW]);
            float4 col = (c0 + c1 + c2 + c3) * 0.25f;

            // disable self-emission
            Uint32 NumEmissionPix = (c0.a > 0.f) + (c1.a > 0.f) + (c2.a > 0.f) + (c3.a > 0.f);
            if (NumEmissionPix <= 2)
                col.a = 0.f;

            DstPixels[x + y * DstW] = F4Color_To_RGBA8Unorm(col);
        }
    }
}

static void GenTexture(Uint32* Pixels, Uint32 Width, Uint32 Height, Uint32 Slice, Uint32 CurrTime)
{
    const Uint32 Hash  = ((Slice * 0xacd) << (CurrTime & 2)) ^ (CurrTime * 0x4c44);
    const Uint32 Hash2 = Slice * 0x79b3;

    TexLayerType TexType = TexLayerType::Wall;
    if (Slice > 0)
        TexType = static_cast<TexLayerType>((Slice - 1) % static_cast<Uint32>(TexLayerType::Count) + 1);

    switch (TexType)
    {
        // clang-format off
        case TexLayerType::Wall:                    GenWallTexture(Pixels, Width, Height, Hash);                           break;
        case TexLayerType::WallAndRightNeonLine:    GenWallAndRightNeonLineTexture(Pixels, Width, Height, Hash, Hash2);    break;
        case TexLayerType::WallAndTopNeonLine:      GenWallAndTopNeonLineTexture(Pixels, Width, Height, Hash, Hash2);      break;
        case TexLayerType::Windows:                 GenWindowsTexture(Pixels, Width, Height, Hash);                        break;
        case TexLayerType::WindowsAndRightNeonLine: GenWindowsAndRightNeonLineTexture(Pixels, Width, Height, Hash, Hash2); break;
        case TexLayerType::WindowsAndTopNeonLine:   GenWindowsAndTopNeonLineTexture(Pixels, Width, Height, Hash, Hash2);   break;
        // clang-format on
        default:
            UNEXPECTED("Unknown texture layer type");
    }
}


void Buildings::UpdateAtlas(IDeviceContext* pContext, Uint32 RequiredTransferRateMb, Uint32& ActualTransferRateMb)
{
    if (RequiredTransferRateMb == 0)
        return;

    const auto& TexDesc = m_OpaqueTexAtlas->GetDesc();

    // Try to read a new texture
    for (Uint32 i = 0; i < 100; ++i)
    {
        TaskStatus Expected = TaskStatus::TexReady;
        if (m_GenTexTask.Status.compare_exchange_weak(Expected, TaskStatus::CopyTex, std::memory_order_acquire, std::memory_order_relaxed))
        {
            Uint32 Offset = (m_OpaqueTexAtlasSliceSize / 4) * m_GenTexTask.ArraySlice;
            memcpy(&m_OpaqueTexAtlasPixels[Offset], m_GenTexTask.Pixels.data(), m_OpaqueTexAtlasSliceSize);

            // Update task parameters
            m_GenTexTask.ArraySlice = (m_GenTexTask.ArraySlice + 1) % TexDesc.ArraySize;
            m_GenTexTask.Time       = CurrentTime;

            const auto OldStatus = m_GenTexTask.Status.exchange(TaskStatus::NewTask, std::memory_order_release);
            VERIFY_EXPR(OldStatus == TaskStatus::CopyTex);
            break;
        }
    }

#if USE_STAGING_TEXTURE
    m_UploadCompleteFence->Wait(m_UploadCompleteFenceValue);
#endif

    pContext->BeginDebugGroup("Update textures");

    // Resources must be manually transitioned to required state.
    // Vulkan:     allowed any state which is supported by transfer queue.
    // DirectX 12: resource transition from copy to graphics/compute queue requires resource to be in COMMON state.
    if (m_OpaqueTexAtlasDefaultState != RESOURCE_STATE_COPY_DEST)
    {
        const StateTransitionDesc Barrier{m_OpaqueTexAtlas, m_OpaqueTexAtlasDefaultState, RESOURCE_STATE_COPY_DEST};
        pContext->TransitionResourceStates(1, &Barrier);
    }

    Uint32       CopiedCpuToGpu = 0;
    const Uint32 FirstSlice     = m_m_OpaqueTexAtlasOffset;

    // Each frame we copy pixels from CPU side to GPU side.
    for (Uint32 SliceInd = 0; SliceInd < TexDesc.ArraySize; ++SliceInd)
    {
        Uint32 Slice  = (FirstSlice + SliceInd) % TexDesc.ArraySize;
        Uint32 Offset = (m_OpaqueTexAtlasSliceSize / 4) * Slice;
        for (Uint32 Mipmap = 0; Mipmap < TexDesc.MipLevels; ++Mipmap)
        {
            const auto W = std::max(1u, TexDesc.Width >> Mipmap);
            const auto H = std::max(1u, TexDesc.Height >> Mipmap);

#if USE_STAGING_TEXTURE
            MappedTextureSubresource SubRes;
            pContext->MapTextureSubresource(m_OpaqueTexAtlasStaging, Mipmap, Slice, MAP_WRITE, MAP_FLAG_DO_NOT_WAIT | MAP_FLAG_DISCARD | MAP_FLAG_NO_OVERWRITE, nullptr, SubRes);
            memcpy(SubRes.pData, &m_OpaqueTexAtlasPixels[Offset], W * H * 4);
            pContext->UnmapTextureSubresource(m_OpaqueTexAtlasStaging, Mipmap, Slice);

            CopyTextureAttribs Attribs;
            Attribs.pSrcTexture = m_OpaqueTexAtlasStaging;
            Attribs.SrcMipLevel = Mipmap;
            Attribs.SrcSlice    = Slice;
            Attribs.pDstTexture = m_OpaqueTexAtlas;
            Attribs.DstMipLevel = Mipmap;
            Attribs.DstSlice    = Slice;
            pContext->CopyTexture(Attribs);
#else
            TextureSubResData SubRes;
            SubRes.Stride = Uint64{W} * 4u;
            SubRes.pData = &m_OpaqueTexAtlasPixels[Offset];
            Box Region{0u, W, 0u, H};
            pContext->UpdateTexture(m_OpaqueTexAtlas, Mipmap, Slice, Region, SubRes, RESOURCE_STATE_TRANSITION_MODE_NONE, RESOURCE_STATE_TRANSITION_MODE_NONE);
#endif
            CopiedCpuToGpu += W * H * 4;
            Offset += W * H;
        }

        m_m_OpaqueTexAtlasOffset = Slice;

        ActualTransferRateMb = (CopiedCpuToGpu >> 20) + (CopiedCpuToGpu >> 21); // round bytes to Mb
        if (ActualTransferRateMb >= RequiredTransferRateMb)
            break;
    }

    // Resources must be manually transitioned to required states.
    // Vulkan:     any state supported by transfer queue is allowed.
    // DirectX 12: resource transition from graphics/compute to copy queue requires resource to be in COMMON state.
    if (m_OpaqueTexAtlasDefaultState != RESOURCE_STATE_COPY_DEST)
    {
        const StateTransitionDesc Barrier{m_OpaqueTexAtlas, RESOURCE_STATE_COPY_DEST, m_OpaqueTexAtlasDefaultState};
        pContext->TransitionResourceStates(1, &Barrier);
    }

    pContext->EndDebugGroup();

#if USE_STAGING_TEXTURE
    pContext->EnqueueSignal(m_UploadCompleteFence, ++m_UploadCompleteFenceValue);
#endif
}

Buildings::Buildings()
{
    m_GenTexThreadLooping.store(true);
    m_GenTexThread = std::thread{&Buildings::ThreadProc, this};
}

Buildings::~Buildings()
{
    m_GenTexThreadLooping.store(false);
    m_GenTexThread.join();
}

void Buildings::ThreadProc()
{
    while (m_GenTexThreadLooping.load())
    {
        for (Uint32 i = 0; i < 100; ++i)
        {
            // Check the task status.
            // If task status is 'NewTask' then change status to GenTex,
            // invalidate CPU cache to make changes from another thread visible to current thread.
            TaskStatus Expected = TaskStatus::NewTask;
            if (m_GenTexTask.Status.compare_exchange_weak(Expected, TaskStatus::GenTex, std::memory_order_acquire, std::memory_order_relaxed))
            {
                const auto& TexDesc   = m_OpaqueTexAtlas->GetDesc();
                const auto  Slice     = m_GenTexTask.ArraySlice;
                Uint32      SrcOffset = 0;
                GenTexture(&m_GenTexTask.Pixels[SrcOffset], TexDesc.Width, TexDesc.Height, Slice, m_GenTexTask.Time);

                for (Uint32 Mipmap = 1; Mipmap < TexDesc.MipLevels; ++Mipmap)
                {
                    const Uint32* SrcPixels = &m_GenTexTask.Pixels[SrcOffset];
                    const auto    SrcW      = std::max(1u, TexDesc.Width >> (Mipmap - 1));
                    const auto    SrcH      = std::max(1u, TexDesc.Height >> (Mipmap - 1));
                    const Uint32  DstOffset = SrcOffset + SrcW * SrcH;
                    Uint32*       DstPixels = &m_GenTexTask.Pixels[DstOffset];
                    const auto    DstW      = std::max(1u, TexDesc.Width >> Mipmap);
                    const auto    DstH      = std::max(1u, TexDesc.Height >> Mipmap);

                    GenMipmap(SrcPixels, SrcW, SrcH, DstPixels, DstW, DstH);
                    SrcOffset = DstOffset;
                }

                // Change status to 'TexReady' and flush CPU cache to make local changes visible for other threads.
                const auto OldStatus = m_GenTexTask.Status.exchange(TaskStatus::TexReady, std::memory_order_release);
                VERIFY_EXPR(OldStatus == TaskStatus::GenTex);

                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::microseconds{1});
    }
}

void Buildings::GenerateOpaqueTexture()
{
    const auto& TexDesc = m_OpaqueTexAtlas->GetDesc();

    for (Uint32 Slice = 0; Slice < TexDesc.ArraySize; ++Slice)
    {
        Uint32 SrcOffset = (m_OpaqueTexAtlasSliceSize / 4) * Slice;
        GenTexture(&m_OpaqueTexAtlasPixels[SrcOffset], TexDesc.Width, TexDesc.Height, Slice, 0u);

        for (Uint32 Mipmap = 1; Mipmap < TexDesc.MipLevels; ++Mipmap)
        {
            const Uint32* SrcPixels = &m_OpaqueTexAtlasPixels[SrcOffset];
            const auto    SrcW      = std::max(1u, TexDesc.Width >> (Mipmap - 1));
            const auto    SrcH      = std::max(1u, TexDesc.Height >> (Mipmap - 1));
            const Uint32  DstOffset = SrcOffset + SrcW * SrcH;
            Uint32*       DstPixels = &m_OpaqueTexAtlasPixels[DstOffset];
            const auto    DstW      = std::max(1u, TexDesc.Width >> Mipmap);
            const auto    DstH      = std::max(1u, TexDesc.Height >> Mipmap);

            GenMipmap(SrcPixels, SrcW, SrcH, DstPixels, DstW, DstH);
            SrcOffset = DstOffset;
        }
    }
}

} // namespace Diligent
