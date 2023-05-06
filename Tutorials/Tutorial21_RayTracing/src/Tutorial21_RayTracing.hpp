/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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
#include "FirstPersonCamera.hpp"

namespace Diligent
{

namespace HLSL
{
#include "../assets/structures.fxh"
}

class Tutorial21_RayTracing final : public SampleBase
{
public:
    virtual void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override final;
    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "Tutorial21: Ray tracing"; }

    virtual void WindowResize(Uint32 Width, Uint32 Height) override final;

private:
    void CreateRayTracingPSO();
    void CreateGraphicsPSO();
    void CreateCubeBLAS();
    void CreateProceduralBLAS();
    void UpdateTLAS();
    void CreateSBT();
    void LoadTextures();
    void UpdateUI();

    static constexpr int NumTextures = 4;
    static constexpr int NumCubes    = 4;

    RefCntAutoPtr<IBuffer> m_CubeAttribsCB;
    RefCntAutoPtr<IBuffer> m_BoxAttribsCB;
    RefCntAutoPtr<IBuffer> m_ConstantsCB;

    RefCntAutoPtr<IPipelineState>         m_pRayTracingPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pRayTracingSRB;

    RefCntAutoPtr<IPipelineState>         m_pImageBlitPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_pImageBlitSRB;

    RefCntAutoPtr<IBottomLevelAS>      m_pCubeBLAS;
    RefCntAutoPtr<IBottomLevelAS>      m_pProceduralBLAS;
    RefCntAutoPtr<ITopLevelAS>         m_pTLAS;
    RefCntAutoPtr<IBuffer>             m_InstanceBuffer;
    RefCntAutoPtr<IBuffer>             m_ScratchBuffer;
    RefCntAutoPtr<IShaderBindingTable> m_pSBT;

    Uint32          m_MaxRecursionDepth     = 8;
    const double    m_MaxAnimationTimeDelta = 1.0 / 60.0;
    float           m_AnimationTime         = 0.0f;
    HLSL::Constants m_Constants             = {};
    bool            m_EnableCubes[NumCubes] = {true, true, true, true};
    bool            m_Animate               = true;
    float           m_DispersionFactor      = 0.1f;

    FirstPersonCamera m_Camera;

    TEXTURE_FORMAT          m_ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM;
    RefCntAutoPtr<ITexture> m_pColorRT;
};

} // namespace Diligent
