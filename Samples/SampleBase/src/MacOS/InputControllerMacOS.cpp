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
    switch(key)
    {
        case 'w':
        case 63232:
        case 264:
            m_Keys[static_cast<size_t>(InputKeys::MoveForward)] = INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
            break;

        case 's':
        case 63233:
        case 258:
            m_Keys[static_cast<size_t>(InputKeys::MoveBackward)] = INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
            break;

        case 'a':
        case 260:
            m_Keys[static_cast<size_t>(InputKeys::MoveLeft)] = INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
            break;

        case 'd':
        case 262:
            m_Keys[static_cast<size_t>(InputKeys::MoveRight)] = INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
            break;

        case 'e':
        case 265:
            m_Keys[static_cast<size_t>(InputKeys::MoveDown)] = INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
            break;

        case 'q':
        case 259:
            m_Keys[static_cast<size_t>(InputKeys::MoveUp)] = INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
            break;

        case 263:
            m_Keys[static_cast<size_t>(InputKeys::Reset)] = INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
            break;

        case 269:
            m_Keys[static_cast<size_t>(InputKeys::ZoomOut)] = INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
            break;

        case 270:
            m_Keys[static_cast<size_t>(InputKeys::ZoomIn)] = INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
            break;
    }
}

}
