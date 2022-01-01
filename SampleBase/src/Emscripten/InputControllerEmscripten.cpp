/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#include <emscripten/key_codes.h>
#include "InputController.hpp"

namespace Diligent
{

void InputControllerEmscripten::OnMouseMove(int32_t MouseX, int32_t MouseY)
{
    m_MouseState.PosX = static_cast<float>(MouseX);
    m_MouseState.PosY = static_cast<float>(MouseY);
}

void InputControllerEmscripten::OnMouseButtonEvent(MouseButton Button, bool IsPressed)
{
    switch (Button)
    {
        case MouseButtonLeft:
            m_MouseState.ButtonFlags = IsPressed ? (m_MouseState.ButtonFlags | MouseState::BUTTON_FLAG_LEFT) : (m_MouseState.ButtonFlags & ~MouseState::BUTTON_FLAG_LEFT);
            break;
        case MouseButtonRight:
            m_MouseState.ButtonFlags = IsPressed ? (m_MouseState.ButtonFlags | MouseState::BUTTON_FLAG_RIGHT) : (m_MouseState.ButtonFlags & ~MouseState::BUTTON_FLAG_RIGHT);
            break;
        default:
            break;
    }
}

void InputControllerEmscripten::OnMouseWheel(float WheelDelta)
{
    m_MouseState.WheelDelta = WheelDelta;
}


void InputControllerEmscripten::ProcessKeyEvent(int32_t KeyCode, bool IsKeyPressed)
{
    auto UpdateKeyState = [&](InputKeys Key) {
        auto& KeyState = m_Keys[static_cast<size_t>(Key)];
        if (IsKeyPressed)
        {
            KeyState &= ~INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN;
            KeyState |= INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
        }
        else
        {
            KeyState &= ~INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
            KeyState |= INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN;
        }
    };

    switch (KeyCode)
    {
        case DOM_VK_W:
            UpdateKeyState(InputKeys::MoveForward);
            break;
        case DOM_VK_S:
            UpdateKeyState(InputKeys::MoveBackward);
            break;
        case DOM_VK_A:
            UpdateKeyState(InputKeys::MoveLeft);
            break;
        case DOM_VK_D:
            UpdateKeyState(InputKeys::MoveRight);
            break;
        case DOM_VK_Q:
            UpdateKeyState(InputKeys::MoveDown);
            break;
        case DOM_VK_E:
            UpdateKeyState(InputKeys::MoveUp);
            break;
        case DOM_VK_HOME:
            UpdateKeyState(InputKeys::Reset);
            break;
        case DOM_VK_SUBTRACT:
            UpdateKeyState(InputKeys::ZoomOut);
            break;
        case DOM_VK_ADD:
            UpdateKeyState(InputKeys::ZoomIn);
            break;
    }
}

void InputControllerEmscripten::OnKeyPressed(int32_t KeyCode)
{
    ProcessKeyEvent(KeyCode, true);
}

void InputControllerEmscripten::OnKeyReleased(int32_t KeyCode)
{
    ProcessKeyEvent(KeyCode, false);
}

} // namespace Diligent
