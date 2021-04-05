/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include "AnimationUtilities.hpp"
#include "BasicMath.hpp"

#include <cassert>

#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/log.h"

using namespace ozz::math;

namespace Diligent
{
    bool LoadSkeleton(const char* pFileName, ozz::animation::Skeleton* pSkeleton)
    {
        assert(pFileName && pSkeleton);
        ozz::log::Out() << "Loading skeleton archive " << pFileName << "."
                        << std::endl;
        ozz::io::File file(pFileName, "rb");
        if (!file.opened())
        {
            ozz::log::Err() << "Failed to open skeleton file " << pFileName << "."
                            << std::endl;
            return false;
        }
        ozz::io::IArchive archive(&file);
        if (!archive.TestTag<ozz::animation::Skeleton>())
        {
            ozz::log::Err() << "Failed to load skeleton instance from file "
                            << pFileName << "." << std::endl;
            return false;
        }

        // Once the tag is validated, reading cannot fail.
        archive >> *pSkeleton;

        return true;
    }

    bool LoadAnimation(const char* pFileName, ozz::animation::Animation* pAnimation)
    {
        assert(pFileName && pAnimation);
        ozz::log::Out() << "Loading animation archive: " << pFileName << "."
                        << std::endl;
        ozz::io::File file(pFileName, "rb");
        if (!file.opened())
        {
            ozz::log::Err() << "Failed to open animation file " << pFileName << "."
                            << std::endl;
            return false;
        }
        ozz::io::IArchive archive(&file);
        if (!archive.TestTag<ozz::animation::Animation>())
        {
            ozz::log::Err() << "Failed to load animation instance from file "
                            << pFileName << "." << std::endl;
            return false;
        }

        // Once the tag is validated, reading cannot fail.
        archive >> *pAnimation;

        return true;
    }

    RefCntAutoPtr<IBuffer> CreateJointVertexBuffer(IRenderDevice* pDevice)
    {
        // Layout of this structure matches the one we defined in the pipeline state
        struct Vertex
        {
            float3 pos;
            float3 normal;
            float4 color;
        };

        const float kInter = .2f;
        const int   kNumSlices          = 20;
        const int   kNumPointsPerCircle = kNumSlices + 1;
        const int   kNumPointsYZ        = kNumPointsPerCircle;
        const int   kNumPointsXY        = kNumPointsPerCircle + kNumPointsPerCircle / 4;
        const int   kNumPointsXZ        = kNumPointsPerCircle;
        const int   kNumPoints          = kNumPointsXY + kNumPointsXZ + kNumPointsYZ;
        const float kRadius             = kInter; // Radius multiplier.
        const float4 Red                = {1.f, 0.75f, 0.75f, 1.f};
        const float4 Green              = {0.75f, 1.f, 0.75f, 1.f};
        const float4 Blue               = {0.75f, 0.75f, 1.f, 1.f};
                
        Vertex JointVerts[kNumPoints];

        int Index = 0;
        const float k2PI  = 2.f * PI_F;
        // YZ plan.
        for (int i = 0; i < kNumPointsYZ; ++i)            
        {
            float Angle = i * k2PI / kNumSlices;
            float SinA  = sinf(Angle);
            float CosA  = cosf(Angle);
            Vertex& NewVertex = JointVerts[Index++];
            NewVertex.pos     = float3(0.f, CosA * kRadius, SinA * kRadius);
            NewVertex.normal  = float3(0.f, CosA, SinA);
            NewVertex.color   = Red;
        }

        // XY plan.
        for (int i = 0; i < kNumPointsXY; ++i)
        {
            float   Angle     = i * k2PI / kNumSlices;
            float   SinA      = sinf(Angle);
            float   CosA      = cosf(Angle);
            Vertex& NewVertex = JointVerts[Index++];
            NewVertex.pos     = float3(SinA * kRadius, CosA * kRadius, 0.f);
            NewVertex.normal  = float3(SinA, CosA, 0.f);
            NewVertex.color   = Blue;
        }

        // XZ plan.
        for (int i = 0; i < kNumPointsXZ; ++i)
        {
            float   Angle     = i * k2PI / kNumSlices;
            float   SinA      = sinf(Angle);
            float   CosA      = cosf(Angle);
            Vertex& NewVertex = JointVerts[Index++];
            NewVertex.pos     = float3(CosA * kRadius, 0.f, -SinA * kRadius);
            NewVertex.normal  = float3(CosA, 0.f, -SinA);
            NewVertex.color   = Green;
        }

        BufferDesc VertBuffDesc;
        VertBuffDesc.Name          = "Joint vertex buffer";
        VertBuffDesc.Usage         = USAGE_IMMUTABLE;
        VertBuffDesc.BindFlags     = BIND_VERTEX_BUFFER;
        VertBuffDesc.uiSizeInBytes = sizeof(JointVerts);
        BufferData VBData;
        VBData.pData    = JointVerts;
        VBData.DataSize = sizeof(JointVerts);
        RefCntAutoPtr<IBuffer> pJointVertexBuffer;

        pDevice->CreateBuffer(VertBuffDesc, &VBData, &pJointVertexBuffer);

        return pJointVertexBuffer;
    }

    RefCntAutoPtr<IBuffer> CreateBoneVertexBuffer(IRenderDevice* pDevice)
    {
        // Layout of this structure matches the one we defined in the pipeline state
        struct Vertex
        {
            float3 pos;
            float3 normal;
            float4 color;
        };

        const float kInter = .2f;
        // clang-format off
        const float3 pos[6] =
        { 
            float3{1.f, 0.f, 0.f}, float3{kInter, .1f, .1f},
            float3{kInter, .1f, -.1f}, float3{kInter, -.1f, -.1f},
            float3{kInter, -.1f, .1f}, float3{0.f, 0.f, 0.f},
        };

        const float3 normals[8] =
        {
            normalize(cross(pos[2] - pos[1], pos[2] - pos[0])),
            normalize(cross(pos[1] - pos[2], pos[1] - pos[5])),
            normalize(cross(pos[3] - pos[2], pos[3] - pos[0])),
            normalize(cross(pos[2] - pos[3], pos[2] - pos[5])),
            normalize(cross(pos[4] - pos[3], pos[4] - pos[0])),
            normalize(cross(pos[3] - pos[4], pos[3] - pos[5])),
            normalize(cross(pos[1] - pos[4], pos[1] - pos[0])),
            normalize(cross(pos[4] - pos[1], pos[4] - pos[5]))
        };

        const float4 white = {1.f, 1.f, 1.f, 1.f};

        Vertex BoneVerts[] =
        {
            {pos[0], normals[0], white}, {pos[2], normals[0], white},
            {pos[1], normals[0], white}, {pos[5], normals[1], white},
            {pos[1], normals[1], white}, {pos[2], normals[1], white},
            {pos[0], normals[2], white}, {pos[3], normals[2], white},
            {pos[2], normals[2], white}, {pos[5], normals[3], white},
            {pos[2], normals[3], white}, {pos[3], normals[3], white},
            {pos[0], normals[4], white}, {pos[4], normals[4], white},
            {pos[3], normals[4], white}, {pos[5], normals[5], white},
            {pos[3], normals[5], white}, {pos[4], normals[5], white},
            {pos[0], normals[6], white}, {pos[1], normals[6], white},
            {pos[4], normals[6], white}, {pos[5], normals[7], white},
            {pos[4], normals[7], white}, {pos[1], normals[7], white}
        };

        // clang-format on
        BufferDesc VertBuffDesc;
        VertBuffDesc.Name          = "Bone vertex buffer";
        VertBuffDesc.Usage         = USAGE_IMMUTABLE;
        VertBuffDesc.BindFlags     = BIND_VERTEX_BUFFER;
        VertBuffDesc.uiSizeInBytes = sizeof(BoneVerts);
        BufferData VBData;
        VBData.pData    = BoneVerts;
        VBData.DataSize = sizeof(BoneVerts);
        RefCntAutoPtr<IBuffer> pBoneVertexBuffer;

        pDevice->CreateBuffer(VertBuffDesc, &VBData, &pBoneVertexBuffer);

        return pBoneVertexBuffer;
    }

    void FillInstanceBuffer(const ozz::animation::Skeleton& Skeleton,
                           ozz::span<const Float4x4> ModelMatrices,
                           ozz::span<Float4x4>       JointMatrices,
                           ozz::span<Float4x4>       BoneMatrices)
    {
        const int NumJoints = Skeleton.num_joints();
        const ozz::span<const int16_t>& Parents   = Skeleton.joint_parents();

        int Instance = 0;
        for (int i = 0; i < NumJoints; ++i)
        {
            const int16_t ParentIdx = Parents[i];

            // Root isn't rendered.
            if (ParentIdx == ozz::animation::Skeleton::kNoParent)
            {
                continue;
            }

            // Selects joint matrices.
            const Float4x4& ParentMatr  = ModelMatrices[ParentIdx];
            const Float4x4& CurrentMatr = ModelMatrices[i];

            const SimdFloat4 BoneDir = CurrentMatr.cols[3] - ParentMatr.cols[3];
            const SimdFloat4 BoneLen = SplatX(Length3(BoneDir));

            // Use the parent and child world matrices to create a bone world
            // matrix which will place it between the two joints
            // Using Gramm Schmidt process'
            float      Dot1 = GetX(Dot3(ParentMatr.cols[2], BoneDir));
            float      Dot2 = GetX(Dot3(ParentMatr.cols[0], BoneDir));
            SimdFloat4 Binormal = std::abs(Dot1) < std::abs(Dot2) ? ParentMatr.cols[2] : ParentMatr.cols[0];

            Float4x4& BoneMatr = BoneMatrices[Instance];
            BoneMatr.cols[0] = BoneDir;
            BoneMatr.cols[1] = _mm_mul_ps(Normalize3(Cross3(Binormal, BoneDir)), BoneLen);
            BoneMatr.cols[2] = _mm_mul_ps(Normalize3(Cross3(BoneDir, BoneMatr.cols[1])), BoneLen);
            BoneMatr.cols[3] = ParentMatr.cols[3];

            Float4x4& JointMatr = JointMatrices[Instance];
            JointMatr           = Scale(CurrentMatr, BoneLen);

            Instance++;
        }
    }

}