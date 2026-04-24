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

#include "Tutorial30_Immersive.hpp"

#include "RenderDevice.h"
#include "Tutorial30_RenderEngine.hpp"

namespace Diligent
{

Tutorial30_Immersive::Tutorial30_Immersive(void* pLayerRenderer)
{
    Tutorial30_RenderEngine& Engine = Tutorial30_RenderEngine::Get();
    m_Session                       = std::make_unique<CompositorServicesSession>(pLayerRenderer,
                                                            Engine.GetDevice(),
                                                            Engine.GetImmediateContext());
}

void Tutorial30_Immersive::RenderFrame()
{
    Tutorial30_Scene& Scene = Tutorial30_RenderEngine::Get().GetScene();

    m_Session->RenderFrame(
        [&Scene] { Scene.Update(); },
        [this](void* pDrawable) { RenderDrawable(pDrawable); });
}

void Tutorial30_Immersive::RenderDrawable(void* pDrawable)
{
    Tutorial30_RenderEngine& Engine = Tutorial30_RenderEngine::Get();

    IDeviceContext*   pImmediateContext = Engine.GetImmediateContext();
    IRenderDevice*    pDevice           = Engine.GetDevice();
    Tutorial30_Scene& Scene             = Engine.GetScene();

    const Uint32 ViewCount = m_Session->GetViewCount(pDrawable);

    for (Uint32 ViewIdx = 0; ViewIdx < ViewCount; ++ViewIdx)
    {
        RefCntAutoPtr<ITexture> pColor = m_Session->GetColorSwapchainImage(pDrawable, ViewIdx);
        RefCntAutoPtr<ITexture> pDepth = m_Session->GetDepthSwapchainImage(pDrawable, ViewIdx);
        if (pColor == nullptr || pDepth == nullptr)
            continue;

        const float4x4 View     = m_Session->GetViewMatrix(pDrawable, ViewIdx);
        const float4x4 Proj     = m_Session->GetProjectionMatrix(pDrawable, ViewIdx, 0.1f, 100.0f);
        const float4x4 ViewProj = View * Proj;

        Scene.Render(pImmediateContext,
                     pColor->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),
                     pDepth->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL),
                     ViewProj);
    }

    m_Session->PresentDrawable(pDrawable);
    pImmediateContext->Flush();
    pImmediateContext->FinishFrame();
    pDevice->ReleaseStaleResources();
}

} // namespace Diligent
