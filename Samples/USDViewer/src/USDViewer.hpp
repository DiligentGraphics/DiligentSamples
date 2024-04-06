/*
 *  Copyright 2023-2024 Diligent Graphics LLC
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
#include "BasicMath.hpp"
#include "RenderStateCache.hpp"

#include "HnRenderDelegate.hpp"
#include "Tasks/HnTaskManager.hpp"
#include "Tasks/HnRenderRprimsTask.hpp"
#include "Tasks/HnPostProcessTask.hpp"
#include "Tasks/HnBeginFrameTask.hpp"

#include "pxr/usd/usd/stage.h"
#include "pxr/usd/usdGeom/camera.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/renderIndex.h"
#include "pxr/imaging/hd/engine.h"
#include "pxr/usdImaging/usdImaging/delegate.h"


namespace Diligent
{

namespace USD
{
class HnRenderBuffer;
class HnCamera;
class HnLight;
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
    void PopulateSceneTree(const pxr::UsdPrim& Prim);
    void SetSelectedPrim(const pxr::SdfPath& SelectedPrimId);
    void EditSelectedPrimTransform();
    void UpdateCamera();
    void UpdateModelsList(const std::string& Dir);

private:
    RenderDeviceWithCache_N m_DeviceWithCache;

    bool m_EnableHotShaderReload = false;

    struct StageInfo
    {
        // Declaration order matters as the objects must be destroyed in the specific order!

        pxr::UsdStageRefPtr Stage;

        std::unique_ptr<USD::HnRenderDelegate>   RenderDelegate;
        std::unique_ptr<pxr::HdRenderIndex>      RenderIndex;
        std::unique_ptr<pxr::UsdImagingDelegate> ImagingDelegate;
        std::unique_ptr<USD::HnTaskManager>      TaskManager;

        pxr::SdfPath       CameraId;
        pxr::UsdGeomCamera Camera{};

        USD::HnRenderBuffer* FinalColorTarget = nullptr;

        pxr::SdfPath SelectedPrimId;

        float4x4 RootTransform = float4x4::Identity();

        int  DebugViewMode = 0;
        int  RenderMode    = 0;
        bool UseShadows    = true;

        struct AnimationInfo
        {
            double TimeCodesPerSecond = 0;

            float Time      = 0;
            float StartTime = 0;
            float EndTime   = 0;

            bool Play = false;
        } Animation;

        explicit operator bool() const
        {
            return Stage && RenderDelegate && RenderIndex && ImagingDelegate && TaskManager;
        }
    };
    StageInfo m_Stage;

    pxr::HdEngine m_Engine;

    USD::HnRenderRprimsTaskParams m_RenderParams;
    USD::HnPostProcessTaskParams  m_PostProcessParams;
    USD::HnBeginFrameTaskParams   m_FrameParams;

    Uint32 m_SSRSettingsDisplayMode = 0;

    struct ModelInfo
    {
        std::string Name;
        std::string Path;
    };
    std::vector<ModelInfo>   m_Models;
    std::vector<const char*> m_ModelNames;

    int m_SelectedModel = 0;

    std::string m_UsdFileName;

    bool   m_UseIndexPool    = true;
    bool   m_UseVertexPool   = true;
    Uint32 m_TextureAtlasDim = 2048;

    USD::HN_MATERIAL_TEXTURES_BINDING_MODE m_BindingMode = USD::HN_MATERIAL_TEXTURES_BINDING_MODE_LEGACY;

    RefCntAutoPtr<ITextureView> m_EnvironmentMapSRV;

    TrackballCamera<float> m_Camera;
    float4x4               m_CameraView;
    float4x4               m_CameraProj;

    struct RenderStats
    {
        Uint32 NumDrawCommands      = 0;
        Uint32 NumMultiDrawCommands = 0;
        Uint32 NumPSOChanges        = 0;
        Uint32 NumSRBChanges        = 0;
        Uint32 NumVBChanges         = 0;
        Uint32 NumIBChanges         = 0;
        Uint32 NumBufferMaps        = 0;
        Uint32 NumBufferUpdates     = 0;
        Uint32 NumTriangles         = 0;
        Uint32 NumLines             = 0;
        Uint32 NumPoints            = 0;

        float TaskRunTime = 0;
    };
    RenderStats m_Stats;

    enum class SelectionMode
    {
        OnClick,
        OnHover,
        Count
    };
    SelectionMode m_SelectMode = SelectionMode::OnClick;
    MouseState    m_PrevMouse;
    bool          m_IsSelecting               = false;
    bool          m_ScrolllToSelectedTreeItem = false;
};

} // namespace Diligent
