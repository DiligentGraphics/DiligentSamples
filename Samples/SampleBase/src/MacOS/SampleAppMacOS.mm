/*     Copyright 2015-2019 Egor Yusov
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

#include <queue>
#include <mutex>
#import <Cocoa/Cocoa.h>
#include "SampleApp.h"
#include "ImGuiImplMacOS.h"

namespace Diligent
{

class SampleAppMacOS final : public SampleApp
{
public:
    SampleAppMacOS()
    {
        m_DeviceType = DeviceType::OpenGL;
    }

    virtual void Initialize(void* view)override final
    {
        m_DeviceType = view == nullptr ? DeviceType::OpenGL : DeviceType::Vulkan;
        InitializeDiligentEngine(view);
        m_TheSample->SetUIScale(2);
        const auto& SCDesc = m_pSwapChain->GetDesc();
        m_pImGui.reset(new ImGuiImplMacOS(m_pDevice, SCDesc.ColorBufferFormat, SCDesc.DepthBufferFormat, SCDesc.Width, SCDesc.Height));
        InitializeSample();
    }

    virtual void Render()override
    {
        std::lock_guard<std::mutex> lock(AppMutex);

        m_pImmediateContext->SetRenderTargets(0, nullptr, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        SampleApp::Render();
    }

    virtual void Update(double CurrTime, double ElapsedTime)override
    {
        std::lock_guard<std::mutex> lock(AppMutex);
        SampleApp::Update(CurrTime, ElapsedTime);
    }

    virtual void WindowResize(int width, int height)override
    {
        std::lock_guard<std::mutex> lock(AppMutex);
        SampleApp::WindowResize(width, height);
        const auto& SCDesc = m_pSwapChain->GetDesc();
        static_cast<ImGuiImplMacOS*>(m_pImGui.get())->SetDisplaySize(SCDesc.Width, SCDesc.Height);
    }

    virtual void Present()override
    {
        std::lock_guard<std::mutex> lock(AppMutex);
        SampleApp::Present();
    }

    virtual void HandleOSXEvent(void* _event, void* _view)override final
    {
        auto* event = (NSEvent*)_event;

        if (event.type != NSEventTypeLeftMouseDown   && event.type != NSEventTypeRightMouseDown    &&
            event.type != NSEventTypeLeftMouseUp     && event.type != NSEventTypeRightMouseUp      &&
            event.type != NSEventTypeOtherMouseDown  && event.type != NSEventTypeOtherMouseUp      &&
            event.type != NSEventTypeMouseMoved      &&
            event.type != NSEventTypeLeftMouseDragged&& event.type != NSEventTypeRightMouseDragged &&
            event.type != NSEventTypeKeyDown         && event.type != NSEventTypeKeyUp             &&
            event.type != NSEventTypeFlagsChanged    && event.type != NSEventTypeScrollWheel)
            return;

        std::lock_guard<std::mutex> lock(AppMutex);
        auto& inputController = m_TheSample->GetInputController();

        auto* view  = (NSView*)_view;
        if (!static_cast<ImGuiImplMacOS*>(m_pImGui.get())->HandleOSXEvent(event, view))
        {
            if (event.type == NSEventTypeLeftMouseDown || event.type == NSEventTypeRightMouseDown)
            {
                auto ControllerEvent = event.type == NSEventTypeLeftMouseDown ?
                            InputController::MouseButtonEvent::LMB_Pressed :
                            InputController::MouseButtonEvent::RMB_Pressed;
                inputController.OnMouseButtonEvent(ControllerEvent);
            }

            if (event.type == NSEventTypeLeftMouseUp || event.type == NSEventTypeRightMouseUp)
            {
                auto ControllerEvent = event.type == NSEventTypeLeftMouseUp  ?
                    InputController::MouseButtonEvent::LMB_Released :
                    InputController::MouseButtonEvent::RMB_Released;
                inputController.OnMouseButtonEvent(ControllerEvent);
            }

            if (event.type == NSEventTypeLeftMouseDown    || event.type == NSEventTypeRightMouseDown ||
                event.type == NSEventTypeLeftMouseUp      || event.type == NSEventTypeRightMouseUp   ||
                event.type == NSEventTypeMouseMoved       ||
                event.type == NSEventTypeLeftMouseDragged || event.type == NSEventTypeRightMouseDragged)
            {
                NSRect viewRectPoints = [view bounds];
                NSRect viewRectPixels = [view convertRectToBacking:viewRectPoints];
                NSPoint curPoint = [view convertPoint:[event locationInWindow] fromView:nil];
                curPoint = [view convertPointToBacking:curPoint];
                inputController.OnMouseMove(curPoint.x, viewRectPixels.size.height-1 - curPoint.y);
            }

            if (event.type == NSEventTypeKeyDown ||
                event.type == NSEventTypeKeyUp)
            {
                int key = 0;
                unichar c = [[event charactersIgnoringModifiers] characterAtIndex:0];
                switch(c)
                {
                    case NSLeftArrowFunctionKey:  key = 260; break;
                    case NSRightArrowFunctionKey: key = 262; break;
                    case 0x7F:                    key = '\b'; break;
                    default:                      key = c;
                }
                if (event.type == NSEventTypeKeyDown)
                    inputController.OnKeyPressed(key);
                else
                    inputController.OnKeyReleased(key);
            }
        }

        if (event.type ==  NSEventTypeFlagsChanged)
        {
            auto modifierFlags = [event modifierFlags];
            {
                inputController.OnFlagsChanged(modifierFlags & NSEventModifierFlagShift,
                                               modifierFlags & NSEventModifierFlagControl,
                                               modifierFlags & NSEventModifierFlagOption);
            }
        }
    }

private:
    // Render functions are called from high-priority Display Link thread,
    // so all methods must be protected by mutex
    std::mutex AppMutex;
};

NativeAppBase* CreateApplication()
{
    return new SampleAppMacOS;
}

}
