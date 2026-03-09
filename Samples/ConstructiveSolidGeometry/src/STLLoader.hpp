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

#include <cstdint>
#include <cstring>
#include <vector>
#include <unordered_map>

#include "BasicMath.hpp"
#include "FileWrapper.hpp"

namespace Diligent
{

struct STLVertex
{
    float3 Position;
    float3 Normal;
};

struct STLMesh
{
    std::vector<STLVertex> Vertices;
    std::vector<uint32_t>  Indices;

    float3 BBoxMin = float3{+1e30f, +1e30f, +1e30f};
    float3 BBoxMax = float3{-1e30f, -1e30f, -1e30f};
};

inline bool LoadSTL(const char* FilePath, STLMesh& OutMesh)
{
    std::vector<Uint8> FileData;
    if (!FileWrapper::ReadWholeFile(FilePath, FileData))
        return false;

    const uint8_t* pData    = FileData.data();
    const size_t   DataSize = FileData.size();

    // Binary STL: 80-byte header + 4-byte triangle count + N * 50 bytes
    if (DataSize < 84)
        return false;

    uint32_t TriangleCount = 0;
    std::memcpy(&TriangleCount, pData + 80, sizeof(uint32_t));
    if (TriangleCount == 0)
        return false;

    const size_t ExpectedSize = 84 + static_cast<size_t>(TriangleCount) * 50;
    if (DataSize < ExpectedSize)
        return false;

        // Each triangle: 12 bytes normal + 36 bytes vertices (3x12) + 2 bytes attribute = 50 bytes
#pragma pack(push, 1)
    struct STLTriangle
    {
        float3   Normal;
        float3   V0;
        float3   V1;
        float3   V2;
        uint16_t AttributeByteCount;
    };
#pragma pack(pop)
    static_assert(sizeof(STLTriangle) == 50, "STLTriangle must be 50 bytes");

    const STLTriangle* Triangles = reinterpret_cast<const STLTriangle*>(pData + 84);

    std::unordered_map<float3, uint32_t> VertexMap;
    std::vector<float3>                  Positions;
    std::vector<float3>                  AccumNormals;

    OutMesh.Indices.clear();
    OutMesh.Indices.reserve(TriangleCount * 3);

    auto AddVertex = [&](const float3& Pos, const float3& FaceNormal) -> uint32_t {
        auto It = VertexMap.find(Pos);
        if (It != VertexMap.end())
        {
            AccumNormals[It->second] += FaceNormal;
            return It->second;
        }
        else
        {
            uint32_t Idx = static_cast<uint32_t>(Positions.size());
            Positions.push_back(Pos);
            AccumNormals.push_back(FaceNormal);
            VertexMap[Pos] = Idx;
            return Idx;
        }
    };

    for (uint32_t i = 0; i < TriangleCount; ++i)
    {
        const auto& Tri = Triangles[i];

        // Compute face normal from vertices (more reliable than STL file normal)
        float3 FN = normalize(cross(Tri.V1 - Tri.V0, Tri.V2 - Tri.V0));

        // STL uses CCW vertex order (right-hand rule), but DiligentEngine
        // defaults to CW = front face. Swap V1/V2 to fix SV_IsFrontFace.
        OutMesh.Indices.push_back(AddVertex(Tri.V0, FN));
        OutMesh.Indices.push_back(AddVertex(Tri.V2, FN));
        OutMesh.Indices.push_back(AddVertex(Tri.V1, FN));
    }

    // Build final vertex buffer with normalized normals
    uint32_t VertexCount = static_cast<uint32_t>(Positions.size());
    OutMesh.Vertices.resize(VertexCount);

    for (uint32_t i = 0; i < VertexCount; ++i)
    {
        OutMesh.Vertices[i].Position = Positions[i];
        OutMesh.Vertices[i].Normal   = normalize(AccumNormals[i]);

        OutMesh.BBoxMin = (std::min)(OutMesh.BBoxMin, Positions[i]);
        OutMesh.BBoxMax = (std::max)(OutMesh.BBoxMax, Positions[i]);
    }

    return true;
}

} // namespace Diligent
