/*
 *  Copyright 2023 Diligent Graphics LLC
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

#include <string>

#include "SampleBase.hpp"
#include "TrackballCamera.hpp"
#include "HnRenderer.hpp"

#include "pxr/usd/usd/stage.h"

namespace Diligent
{

namespace USD
{
class HnRenderer;
} // namespace USD

class USDViewer final : public SampleBase
{
public:
    virtual CommandLineStatus ProcessCommandLine(int argc, const char* const* argv) override final;

    virtual void ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs) override final;

    virtual void Initialize(const SampleInitInfo& InitInfo) override final;

    virtual void Render() override final;
    virtual void Update(double CurrTime, double ElapsedTime) override final;

    virtual const Char* GetSampleName() const override final { return "USD Viewer"; }

private:
    void UpdateUI();
    void LoadStage();

private:
    pxr::UsdStageRefPtr m_Stage;

    std::string m_UsdFileName;
    std::string m_UsdPluginRoot;

    RefCntAutoPtr<IBuffer>      m_CameraAttribsCB;
    RefCntAutoPtr<IBuffer>      m_LightAttribsCB;
    RefCntAutoPtr<ITextureView> m_EnvironmentMapSRV;

    RefCntAutoPtr<USD::IHnRenderer> m_Renderer;

    USD::HnDrawAttribs m_DrawAttribs;

    TrackballCamera<float> m_Camera;

    float3 m_LightDirection = normalize(float3{0.5f, 0.6f, -0.2f});
    float4 m_LightColor     = {1, 1, 1, 1};
    float  m_LightIntensity = 3.f;
};

} // namespace Diligent
