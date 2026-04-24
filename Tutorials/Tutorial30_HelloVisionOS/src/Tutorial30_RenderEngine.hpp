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

#include <memory>

#include "RefCntAutoPtr.hpp"
#include "EngineFactory.h"
#include "RenderDevice.h"
#include "DeviceContext.h"

#include "Tutorial30_Scene.hpp"

namespace Diligent
{

// Process-wide rendering engine shared by the immersive renderer and the SwiftUI preview.
class Tutorial30_RenderEngine
{
public:
    // Returns the lazily-initialized process-wide engine.
    static Tutorial30_RenderEngine& Get();

    // clang-format off
    Tutorial30_RenderEngine           (const Tutorial30_RenderEngine&)  = delete;
    Tutorial30_RenderEngine           (      Tutorial30_RenderEngine&&) = delete;
    Tutorial30_RenderEngine& operator=(const Tutorial30_RenderEngine&)  = delete;
    Tutorial30_RenderEngine& operator=(      Tutorial30_RenderEngine&&) = delete;
    // clang-format on

    IEngineFactory*   GetEngineFactory() const { return m_pEngineFactory; }
    IRenderDevice*    GetDevice() const { return m_pDevice; }
    IDeviceContext*   GetImmediateContext() const { return m_pImmediateContext; }
    Tutorial30_Scene& GetScene() { return *m_Scene; }

private:
    Tutorial30_RenderEngine();
    ~Tutorial30_RenderEngine();

    RefCntAutoPtr<IEngineFactory>     m_pEngineFactory;
    RefCntAutoPtr<IRenderDevice>      m_pDevice;
    RefCntAutoPtr<IDeviceContext>     m_pImmediateContext;
    std::unique_ptr<Tutorial30_Scene> m_Scene;
};

} // namespace Diligent
