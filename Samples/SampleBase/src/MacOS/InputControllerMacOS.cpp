/*     Copyright 2015-2019 Egor Yusov
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

#include "InputController.h"
#include <algorithm>

namespace Diligent
{

void InputControllerMacOS::OnMouseButtonEvent(MouseButtonEvent Event)
{
    switch (Event)
    {
        case MouseButtonEvent::LMB_Pressed:
            m_MouseState.ButtonFlags |= MouseState::BUTTON_FLAG_LEFT;
            break;

        case MouseButtonEvent::LMB_Released:
            m_MouseState.ButtonFlags &= ~MouseState::BUTTON_FLAG_LEFT;
            break;

        case MouseButtonEvent::RMB_Pressed:
            m_MouseState.ButtonFlags |= MouseState::BUTTON_FLAG_RIGHT;
            break;

        case MouseButtonEvent::RMB_Released:
            m_MouseState.ButtonFlags &= ~MouseState::BUTTON_FLAG_RIGHT;
            break;

        default:
            break;
    }
}

void InputControllerMacOS::ClearState()
{
    for(Uint32 i=0; i < static_cast<Uint32>(InputKeys::TotalKeys); ++i)
    {
        auto& key = m_Keys[i];
        if (key & INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN)
        {
            key &= ~INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN;
        }
    }
}

void InputControllerMacOS::OnKeyPressed(int key)
{
    ProcessKeyEvent(key, true);
}

void InputControllerMacOS::OnKeyReleased(int key)
{
    ProcessKeyEvent(key, false);
}

void InputControllerMacOS::ProcessKeyEvent(int key, bool IsKeyPressed)
{
    auto UpdateKeyState = [&](InputKeys Key)
    {
        auto& KeyState = m_Keys[static_cast<size_t>(Key)];
        if(IsKeyPressed)
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

    switch(key)
    {
        case 'w':
        case 63232:
        case 264:
            UpdateKeyState(InputKeys::MoveForward);
            break;

        case 's':
        case 63233:
        case 258:
            UpdateKeyState(InputKeys::MoveBackward);
            break;

        case 'a':
        case 260:
            UpdateKeyState(InputKeys::MoveLeft);
            break;

        case 'd':
        case 262:
            UpdateKeyState(InputKeys::MoveRight);
            break;

        case 'e':
        case 265:
            UpdateKeyState(InputKeys::MoveDown);
            break;

        case 'q':
        case 259:
            UpdateKeyState(InputKeys::MoveUp);
            break;

        case 263:
            UpdateKeyState(InputKeys::Reset);
            break;

        case 269:
            UpdateKeyState(InputKeys::ZoomOut);
            break;

        case 270:
            UpdateKeyState(InputKeys::ZoomIn);
            break;
    }
}

}
