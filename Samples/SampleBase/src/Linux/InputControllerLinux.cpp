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
#include <X11/Xlib.h>
#include <X11/Xutil.h>

// Undef symbols defined by XLib
#ifdef Bool
# undef Bool
#endif
#ifdef True
#   undef True
#endif
#ifdef False
#   undef False
#endif

#include <xcb/xcb.h>
//#include "xcb_keysyms/xcb_keysyms.h"

#include "InputController.h"
#include "DebugUtilities.h"

namespace Diligent
{


void InputControllerLinux::ClearState()
{
    for(Uint32 i=0; i < static_cast<Uint32>(InputKeys::TotalKeys); ++i)
    {
        auto& KeyState = m_Keys[i];
        if (KeyState & INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN)
        {
            KeyState &= ~INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN;
        }
    }
}

int InputControllerLinux::HandleXKeyEvevnt(void* xevent)
{
    auto* event = reinterpret_cast<XEvent*>(xevent);

    KeySym keysym;
    constexpr size_t buff_sz = 80;
    char buffer[buff_sz];
        
    int num_char = XLookupString((XKeyEvent *)event, buffer, buff_sz, &keysym, 0);

    VERIFY_EXPR(event->type == KeyPress || event->type == KeyRelease);
    
    int handled = 0;

    auto UpdateKeyState = [&](InputKeys Key)
    {
        auto& KeyState = m_Keys[static_cast<size_t>(Key)];
        if (event->type == KeyPress)
        {
            KeyState &= ~INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN;
            KeyState |= INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
        }
        else
        {
            KeyState &= ~INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
            KeyState |= INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN;
        }
        handled = 1;
    };

    if (event->xkey.state & ControlMask)
        UpdateKeyState(InputKeys::ControlDown);
    if (event->xkey.state & ShiftMask)
        UpdateKeyState(InputKeys::ShiftDown);
    if (event->xkey.state & Mod1Mask)
        UpdateKeyState(InputKeys::AltDown);

    switch (keysym)
    {
        case XK_Control_L:
        case XK_Control_R:
            UpdateKeyState(InputKeys::ControlDown);
        break;

        case XK_Shift_L:
        case XK_Shift_R:
            UpdateKeyState(InputKeys::ShiftDown);
        break;

        case XK_Alt_L:
        case XK_Alt_R:
            UpdateKeyState(InputKeys::AltDown);
        break;

        case XK_Up:
        case 'w':
            UpdateKeyState(InputKeys::MoveForward);
        break;

        case XK_Down:
        case 's':
            UpdateKeyState(InputKeys::MoveBackward);
        break;

        case XK_Right:
        case 'd':
            UpdateKeyState(InputKeys::MoveRight);
        break;

        case XK_Left:
        case 'a':
            UpdateKeyState(InputKeys::MoveLeft);
        break;

        case XK_Home:
            UpdateKeyState(InputKeys::Reset);
        break;

        case XK_Page_Up:
        case 'e':
            UpdateKeyState(InputKeys::MoveUp);
        break;

        case XK_Page_Down:
        case 'q':
            UpdateKeyState(InputKeys::MoveDown);
        break;

        case XK_plus:
            UpdateKeyState(InputKeys::ZoomIn);
        break;

        case XK_minus:
            UpdateKeyState(InputKeys::ZoomOut);
        break;

#ifdef XK_KP_Home
        case XK_KP_Home:
            UpdateKeyState(InputKeys::Reset);
        break;
#endif

#ifdef XK_KP_Up
        case XK_KP_Up:
            UpdateKeyState(InputKeys::MoveForward);
        break;

        case XK_KP_Down:
            UpdateKeyState(InputKeys::MoveBackward);
        break;

        case XK_KP_Right:
            UpdateKeyState(InputKeys::MoveRight);
        break;

        case XK_KP_Left:
            UpdateKeyState(InputKeys::MoveLeft);
        break;

        case XK_KP_Page_Up:
            UpdateKeyState(InputKeys::MoveUp);
        break;

        case XK_KP_Page_Down:
            UpdateKeyState(InputKeys::MoveDown);
        break;  
#endif

    }  

    return handled;
}

int InputControllerLinux::HandleXEvent(void* xevent)
{
    auto* event = reinterpret_cast<XEvent*>(xevent);
    switch (event->type)
    {
        case KeyPress:
        {
            return HandleXKeyEvevnt(xevent);
        }

        case KeyRelease:
        {
            return HandleXKeyEvevnt(xevent);
        }

        case ButtonPress:
        {
            auto* xbe = reinterpret_cast<XButtonEvent *>(event);
            if (xbe->button == Button1)
                m_MouseState.ButtonFlags |= MouseState::BUTTON_FLAG_LEFT;
            if (xbe->button == Button2)
                m_MouseState.ButtonFlags |= MouseState::BUTTON_FLAG_MIDDLE;
            if (xbe->button == Button3)
                m_MouseState.ButtonFlags |= MouseState::BUTTON_FLAG_RIGHT;
            return 1;
        }

        case ButtonRelease: 
        {
            auto* xbe = reinterpret_cast<XButtonEvent *>(event);
            if (xbe->button == Button1)
                m_MouseState.ButtonFlags &= ~MouseState::BUTTON_FLAG_LEFT;
            if (xbe->button == Button2)
                m_MouseState.ButtonFlags &= ~MouseState::BUTTON_FLAG_MIDDLE;
            if (xbe->button == Button3)
                m_MouseState.ButtonFlags &= ~MouseState::BUTTON_FLAG_RIGHT;
            return 1;
        }

        case MotionNotify:
        {
            auto* xme = (XMotionEvent *)event;
            m_MouseState.PosX = static_cast<float>(xme->x);
            m_MouseState.PosY = static_cast<float>(xme->y);
            return 1;
        }

        default:  
            break;
    }

    return 0;
}

int InputControllerLinux::HandleXCBEvent(void* xcb_event)
{
    auto* event = reinterpret_cast<xcb_generic_event_t*>(xcb_event);

    return 0;
}

}
