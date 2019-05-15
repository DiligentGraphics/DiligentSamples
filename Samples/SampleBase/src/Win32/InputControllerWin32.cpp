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

#ifndef NOMINMAX
#   define NOMINMAX
#endif
#include "Windows.h"

namespace Diligent
{

InputKeys MapCameraKeyWnd(UINT nKey)
{
    switch( nKey )
    {
        case VK_CONTROL:
            return InputKeys::ControlDown;
        
        case VK_SHIFT:
            return InputKeys::ShiftDown;

        case VK_LEFT:
        case 'A':
            return InputKeys::MoveLeft;
        
        case VK_RIGHT:
        case 'D':
            return InputKeys::MoveRight;
        
        case VK_UP:
        case 'W':
            return InputKeys::MoveForward;
        
        case VK_DOWN:
        case 'S':
            return InputKeys::MoveBackward;
        
        case VK_PRIOR:
        case 'E':
            return InputKeys::MoveUp;        // pgup
        
        case VK_NEXT:
        case 'Q':
            return InputKeys::MoveDown;      // pgdn

        case VK_HOME:
            return InputKeys::Reset;

        case VK_ADD:
            return InputKeys::ZoomIn;

        case VK_SUBTRACT:
            return InputKeys::ZoomOut;

        default:
            return InputKeys::Unknown;
    }
}

InputControllerWin32::InputControllerWin32(bool SmoothMouseMotion) : 
    m_SmoothMouseMotion(SmoothMouseMotion)
{
    POINT MousePosition;
    GetCursorPos( &MousePosition );
    m_LastMousePosX = MousePosition.x;
    m_LastMousePosY = MousePosition.y;
}

bool InputControllerWin32::IsKeyDown( Uint8 key )
{
    return( ( key & KEY_IS_DOWN_MASK ) == KEY_IS_DOWN_MASK );
}
bool InputControllerWin32::WasKeyDown( Uint8 key )
{
    return( ( key & KEY_WAS_DOWN_MASK ) == KEY_WAS_DOWN_MASK );
}

const MouseState& InputControllerWin32::GetMouseState()
{
    UpdateMouseDelta();
    return m_MouseState;
}

bool InputControllerWin32::HandleNativeMessage(const void *MsgData)
{
    m_MouseState.WheelDelta = 0;

    struct WindowMessageData
    {
        HWND   hWnd;
        UINT   message;
        WPARAM wParam;
        LPARAM lParam;
    };
    const WindowMessageData& WndMsg = *reinterpret_cast<const WindowMessageData*>(MsgData);
    
    auto hWnd   = WndMsg.hWnd;
    auto uMsg   = WndMsg.message;
    auto wParam = WndMsg.wParam;
    auto lParam = WndMsg.lParam;


    bool MsgHandled = false;
    switch( uMsg )
    {
        case WM_KEYDOWN:
        {
            // Map this key to a InputKeys enum and update the
            // state of m_aKeys[] by adding the KEY_WAS_DOWN_MASK|KEY_IS_DOWN_MASK mask
            // only if the key is not down
            auto mappedKey = MapCameraKeyWnd( ( UINT )wParam );
            if( mappedKey < InputKeys::Unknown )
            {
                auto &Key = m_Keys[static_cast<Int32>(mappedKey)];
                if( !IsKeyDown( Key ) )
                {
                    Key = KEY_WAS_DOWN_MASK | KEY_IS_DOWN_MASK;
                    ++m_NumKeysDown;
                }
            }
            MsgHandled = true;
            break;
        }

        case WM_KEYUP:
        {
            // Map this key to a InputKeys enum and update the
            // state of m_aKeys[] by removing the KEY_IS_DOWN_MASK mask.
            auto mappedKey = MapCameraKeyWnd( ( UINT )wParam );
            if( mappedKey < InputKeys::Unknown )
            {
                auto &Key = m_Keys[static_cast<Int32>(mappedKey)];
                if( IsKeyDown(Key) )
                {
                    Key &= ~KEY_IS_DOWN_MASK;
                    --m_NumKeysDown;
                }
            }
            MsgHandled = true;
            break;
        }

        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
        case WM_LBUTTONDBLCLK:
            {
                // Update member var state
                if( ( uMsg == WM_LBUTTONDOWN || uMsg == WM_LBUTTONDBLCLK ) )
                {
                    m_MouseState.ButtonMask |= MOUSE_LEFT_BUTTON;
                }
                if( ( uMsg == WM_MBUTTONDOWN || uMsg == WM_MBUTTONDBLCLK ) )
                {
                    m_MouseState.ButtonMask |= MOUSE_MIDDLE_BUTTON;
                }
                if( ( uMsg == WM_RBUTTONDOWN || uMsg == WM_RBUTTONDBLCLK )  )
                {
                    m_MouseState.ButtonMask |= MOUSE_RIGHT_BUTTON;
                }

                // Capture the mouse, so if the mouse button is 
                // released outside the window, we'll get the WM_LBUTTONUP message
                SetCapture( hWnd );
                POINT LastMousePosition;
                GetCursorPos( &LastMousePosition );
                m_LastMousePosX = LastMousePosition.x;
                m_LastMousePosY = LastMousePosition.y;
                
                MsgHandled = true;
                break;
            }

        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
        case WM_LBUTTONUP:
            {
                // Update member var state
                if( uMsg == WM_LBUTTONUP )
                {
                    m_MouseState.ButtonMask &= ~MOUSE_LEFT_BUTTON;
                }
                if( uMsg == WM_MBUTTONUP )
                {
                    m_MouseState.ButtonMask &= ~MOUSE_MIDDLE_BUTTON;
                }
                if( uMsg == WM_RBUTTONUP )
                {
                    m_MouseState.ButtonMask &= ~MOUSE_RIGHT_BUTTON;
                }

                // Release the capture if no mouse buttons down
                if( (m_MouseState.ButtonMask & (MOUSE_LEFT_BUTTON | MOUSE_MIDDLE_BUTTON | MOUSE_RIGHT_BUTTON)) == 0 )
                {
                    ReleaseCapture();
                }

                MsgHandled = true;
                break;
            }

        case WM_CAPTURECHANGED:
        {
            if( ( HWND )lParam != hWnd )
            {
                if( ( m_MouseState.ButtonMask & MOUSE_LEFT_BUTTON ) ||
                    ( m_MouseState.ButtonMask & MOUSE_MIDDLE_BUTTON ) ||
                    ( m_MouseState.ButtonMask & MOUSE_RIGHT_BUTTON ) )
                {
                    m_MouseState.ButtonMask &= ~MOUSE_LEFT_BUTTON;
                    m_MouseState.ButtonMask &= ~MOUSE_MIDDLE_BUTTON;
                    m_MouseState.ButtonMask &= ~MOUSE_RIGHT_BUTTON;
                    ReleaseCapture();
                }
            }

            MsgHandled = true;
            break;
        }

        case WM_MOUSEWHEEL:
            // Update member var state
            m_MouseState.WheelDelta = (float)((short)HIWORD(wParam)) / (float)WHEEL_DELTA;
            MsgHandled = true;
            break;
    }

    return MsgHandled;
}

void InputControllerWin32::UpdateMouseDelta()
{
    POINT ptCurMousePos;

    // Get current position of mouse
    GetCursorPos( &ptCurMousePos );

    // Calc how far it's moved since last frame
    Int32 CurMouseDeltaX = ptCurMousePos.x - m_LastMousePosX;
    Int32 CurMouseDeltaY = ptCurMousePos.y - m_LastMousePosY;

    m_LastMousePosX = ptCurMousePos.x;
    m_LastMousePosY = ptCurMousePos.y;

    /*if( m_bResetCursorAfterMove )
    {
        // Set position of camera to center of desktop, 
        // so it always has room to move.  This is very useful
        // if the cursor is hidden.  If this isn't done and cursor is hidden, 
        // then invisible cursor will hit the edge of the screen 
        // and the user can't tell what happened
        POINT ptCenter;

        // Get the center of the current monitor
        MONITORINFO mi;
        mi.cbSize = sizeof( MONITORINFO );
        DXUTGetMonitorInfo( DXUTMonitorFromWindow( DXUTGetHWND(), MONITOR_DEFAULTTONEAREST ), &mi );
        ptCenter.x = ( mi.rcMonitor.left + mi.rcMonitor.right ) / 2;
        ptCenter.y = ( mi.rcMonitor.top + mi.rcMonitor.bottom ) / 2;
        SetCursorPos( ptCenter.x, ptCenter.y );
        m_ptLastMousePosition = ptCenter;
    }*/

    // Smooth the relative mouse data over a few frames so it isn't 
    // jerky when moving slowly at low frame rates.
    float fFramesToSmoothMouseData = 2.f;
    float fPercentOfNew = m_SmoothMouseMotion ? 1.0f / fFramesToSmoothMouseData : 1.0f;
    float fPercentOfOld = 1.0f - fPercentOfNew;
    m_MouseState.DeltaX = m_MouseState.DeltaX * fPercentOfOld + static_cast<float>(CurMouseDeltaX) * fPercentOfNew;
    m_MouseState.DeltaY = m_MouseState.DeltaY * fPercentOfOld + static_cast<float>(CurMouseDeltaY) * fPercentOfNew;
}

}
