/*
 *  Copyright 2019-2023 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT OF ANY PROPRIETARY RIGHTS.
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


#include "SampleApp.hpp"
#include "RenderDeviceGLES.h"
#include "ImGuiImplEmscripten.hpp"
#include <emscripten.h>
#include <emscripten/html5.h>

namespace Diligent
{

class SampleAppEmscripten : public SampleApp
{
public:
    SampleAppEmscripten()
    {
        m_DeviceType = RENDER_DEVICE_TYPE_GLES;
    }

    void OnWindowCreated(const char* CanvasID, int32_t WindowWidth, int32_t WindowHeight) override
    {
        try
        {
            NativeWindow Window{CanvasID};
            InitializeDiligentEngine(&Window);

            const auto& SCDesc = m_pSwapChain->GetDesc();
            m_pImGui           = ImGuiImplEmscripten::Create(ImGuiDiligentCreateInfo{m_pDevice, SCDesc});
            InitializeSample();
        }
        catch (...)
        {
            LOG_ERROR("Failed to initialize Diligent Engine.");
        }
    }

    void OnMouseEvent(int32_t EventType, const EmscriptenMouseEvent* Event) override
    {
        if (!static_cast<ImGuiImplEmscripten*>(m_pImGui.get())->OnMouseEvent(EventType, Event))
        {
            auto& InputController = m_TheSample->GetInputController();

            switch (EventType)
            {
                case EMSCRIPTEN_EVENT_MOUSEDOWN:
                case EMSCRIPTEN_EVENT_MOUSEUP:
                {
                    bool IsPressed = (EventType == EMSCRIPTEN_EVENT_MOUSEDOWN);
                    InputController.OnMouseButtonEvent(static_cast<InputController::MouseButton>(Event->button), IsPressed);
                    break;
                }
                case EMSCRIPTEN_EVENT_MOUSEMOVE:
                {
                    InputController.OnMouseMove(Event->targetX, Event->targetY);
                    break;
                }
                default:
                    break;
            }
        }
    }

    void OnWheelEvent(int32_t EventType, const EmscriptenWheelEvent* Event) override
    {
        if (!static_cast<ImGuiImplEmscripten*>(m_pImGui.get())->OnWheelEvent(EventType, Event))
        {
            auto& InputController = m_TheSample->GetInputController();
            InputController.OnMouseWheel(static_cast<float>(-Event->deltaY * 0.01));
        }
    }

    void OnKeyEvent(int32_t EventType, const EmscriptenKeyboardEvent* Event) override
    {
        if (!static_cast<ImGuiImplEmscripten*>(m_pImGui.get())->OnKeyEvent(EventType, Event))
        {
            auto& inputController = m_TheSample->GetInputController();

            switch (EventType)
            {
                case EMSCRIPTEN_EVENT_KEYDOWN:
                {
                    inputController.OnKeyPressed(Event->which);
                    break;
                }
                case EMSCRIPTEN_EVENT_KEYUP:
                {
                    inputController.OnKeyReleased(Event->which);
                    break;
                }
                default:
                    break;
            }
        }
    }
};

NativeAppBase* CreateApplication()
{
    return new SampleAppEmscripten;
}

} // namespace Diligent
