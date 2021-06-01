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

#pragma once

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/maths/simd_math.h"

#include "Buffer.h"
#include "RenderDevice.h"
#include "RefCntAutoPtr.hpp"


namespace Diligent
{
    // Loads a skeleton from an ozz archive file named pFileName.
    // This function will fail and return false if the file cannot be opened or if
    // it is not a valid ozz skeleton archive. A valid skeleton archive can be
    // produced with ozz tools (fbx2ozz) or using ozz skeleton serialization API.
    // pFileName and pSkeleton must be non-nullptr.
    bool LoadSkeleton(const char* pFileName, ozz::animation::Skeleton* pSkeleton);

    // Loads an animation from an ozz archive file named pFileName.
    // This function will fail and return false if the file cannot be opened or if
    // it is not a valid ozz animation archive. A valid animation archive can be
    // produced with ozz tools (fbx2ozz) or using ozz animation serialization API.
    // pFileName and pAnimation must be non-nullptr.
    bool LoadAnimation(const char* pFileName, ozz::animation::Animation* pAnimation);

    RefCntAutoPtr<IBuffer> CreateJointVertexBuffer(IRenderDevice* pDevice);

    RefCntAutoPtr<IBuffer> CreateBoneVertexBuffer(IRenderDevice* pDevice);

    void FillInstanceBuffer(const ozz::animation::Skeleton&            Skeleton,
                           ozz::span<const ozz::math::Float4x4> ModelMatrices,
                           ozz::span<ozz::math::Float4x4> JointMatrices,
                           ozz::span<ozz::math::Float4x4> BoneMatrices);
    
} // namespace Diligent
