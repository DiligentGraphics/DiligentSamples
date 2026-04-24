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

#include "Tutorial30_Preview.hpp"

#include <cmath>

#include "Tutorial30_RenderEngine.hpp"
#include "EngineFactoryMtl.h"
#include "NativeWindow.h"
#include "DebugUtilities.hpp"

namespace Diligent
{

namespace
{

float4x4 ReverseZProjectionRH(float FovY, float AspectRatio, float NearZ, float FarZ)
{
    // Right-handed reverse-Z projection to D3D clip space (z in [0, 1]).
    // Camera looks down -Z in view space so view-space depth is negative.
    //   view.z = -NearZ -> NDC z = 1 (reverse-Z near)
    //   view.z = -FarZ  -> NDC z = 0 (reverse-Z far)
    //
    // This matches the convention used by `cp_drawable_compute_projection`
    // with `cp_axis_direction_convention_right_up_back`, which is the one
    // used by the immersive renderer on visionOS.
    const float YScale = 1.0f / std::tan(FovY * 0.5f);
    const float XScale = YScale / AspectRatio;

    // clang-format off
    float4x4 Proj;
    Proj._11 = XScale; Proj._12 = 0.0f;   Proj._13 = 0.0f;                             Proj._14 = 0.0f;
    Proj._21 = 0.0f;   Proj._22 = YScale; Proj._23 = 0.0f;                             Proj._24 = 0.0f;
    Proj._31 = 0.0f;   Proj._32 = 0.0f;   Proj._33 = NearZ / (FarZ - NearZ);           Proj._34 = -1.0f;
    Proj._41 = 0.0f;   Proj._42 = 0.0f;   Proj._43 = NearZ * FarZ / (FarZ - NearZ);    Proj._44 = 0.0f;
    // clang-format on
    return Proj;
}

float4x4 LookAtRH(const float3& Eye, const float3& Target, const float3& WorldUp)
{
    // Right-handed view matrix: camera looks down -Z in view space.
    const float3 F = normalize(Eye - Target);
    const float3 R = normalize(cross(WorldUp, F));
    const float3 U = cross(F, R);

    return float4x4::Translation(-Eye) * float4x4::ViewFromBasis(R, U, F);
}

} // namespace

Tutorial30_Preview::Tutorial30_Preview(void* pCAMetalLayer, Uint32 WidthPx, Uint32 HeightPx)
{
    VERIFY_EXPR(pCAMetalLayer != nullptr);

    Tutorial30_RenderEngine& Engine      = Tutorial30_RenderEngine::Get();
    IEngineFactoryMtl*       pFactoryMtl = static_cast<IEngineFactoryMtl*>(Engine.GetEngineFactory());

    SwapChainDesc SCDesc;
    SCDesc.ColorBufferFormat = TEX_FORMAT_BGRA8_UNORM_SRGB;
    SCDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
    SCDesc.Width             = WidthPx;
    SCDesc.Height            = HeightPx;

    NativeWindow Window{pCAMetalLayer};
    pFactoryMtl->CreateSwapChainMtl(Engine.GetDevice(), Engine.GetImmediateContext(),
                                    SCDesc, Window, &m_pSwapChain);
    m_pSwapChain->Resize(WidthPx, HeightPx);
}

void Tutorial30_Preview::Resize(Uint32 WidthPx, Uint32 HeightPx)
{
    if (m_pSwapChain == nullptr)
        return;
    const SwapChainDesc& Desc = m_pSwapChain->GetDesc();
    if (Desc.Width == WidthPx && Desc.Height == HeightPx)
        return;
    m_pSwapChain->Resize(WidthPx, HeightPx);
}

void Tutorial30_Preview::RenderFrame()
{
    if (m_pSwapChain == nullptr)
        return;

    Tutorial30_RenderEngine& Engine = Tutorial30_RenderEngine::Get();
    Tutorial30_Scene&        Scene  = Engine.GetScene();

    Scene.Update();
    Scene.Render(Engine.GetImmediateContext(),
                 m_pSwapChain->GetCurrentBackBufferRTV(),
                 m_pSwapChain->GetDepthBufferDSV(),
                 ComputeViewProj());

    m_pSwapChain->Present();
}

float4x4 Tutorial30_Preview::ComputeViewProj()
{
    m_CameraAngle += 0.01f;

    const float3   Eye{std::sin(m_CameraAngle) * 3.0f, 0.4f, std::cos(m_CameraAngle) * 3.0f - 2.0f};
    const float4x4 View = LookAtRH(Eye, {0.0f, -0.4f, -2.0f}, {0.0f, 1.0f, 0.0f});

    const SwapChainDesc& SCDesc      = m_pSwapChain->GetDesc();
    const float          AspectRatio = static_cast<float>(SCDesc.Width) / static_cast<float>(SCDesc.Height);
    const float4x4       Proj        = ReverseZProjectionRH(PI_F / 3.0f, AspectRatio, 0.1f, 100.0f);

    return View * Proj;
}

} // namespace Diligent
