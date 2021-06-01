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

#include "SampleBase.hpp"
#include "BasicMath.hpp"

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/containers/vector.h"

#include "../../Common/src/PlaybackController.hpp"

namespace Diligent
{

class Animation00_Playback final : public SampleBase
{
public:
    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "Animation00: Playback"; }

private:
    bool InitAnimation();

    void CreateSkeletonPSO();
    void RenderSkeleton();

    void CreateInstanceBuffer();

    void CreatePlanePSO();
    void RenderPlane();

    RefCntAutoPtr<IPipelineState>         m_pPlanePSO;
    RefCntAutoPtr<IPipelineState>         m_pJointPSO;
    RefCntAutoPtr<IPipelineState>         m_pBonePSO;

    RefCntAutoPtr<IShaderResourceBinding> m_pPlaneSRB;
    RefCntAutoPtr<IShaderResourceBinding> m_pSkeletonSRB;

    RefCntAutoPtr<IBuffer>                m_BoneVertexBuffer;
    RefCntAutoPtr<IBuffer>                m_BoneInstanceBuffer;
    RefCntAutoPtr<IBuffer>                m_JointVertexBuffer;
    RefCntAutoPtr<IBuffer>                m_JointInstanceBuffer;

    RefCntAutoPtr<IBuffer>                m_VSConstants;
    float4x4                              m_WorldViewProjMatrix;

    // Runtime skeleton.
    ozz::animation::Skeleton  m_Skeleton;
    // Runtime animation.
    ozz::animation::Animation m_Animation;
    // Sampling cache.
    ozz::animation::SamplingCache m_Cache;
    // Buffer of local transforms as sampled from animation_.
    ozz::vector<ozz::math::SoaTransform> m_Locals;
    // Buffer of model space matrices.
    ozz::vector<ozz::math::Float4x4> m_Models;

    // Buffer of bone matrices.
    ozz::vector<ozz::math::Float4x4> m_Bones;
    // Buffer of joint matrices.
    ozz::vector<ozz::math::Float4x4> m_Joints;

    PlaybackController m_PlaybackController;
};

} // namespace Diligent
