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

#define NOMINIMAX
#include <wrl.h>
#include <wrl/client.h>

#include "InputControllerEventHandlerUWP.h"

#ifndef WHEEL_DELTA
#   define  WHEEL_DELTA 120
#endif

using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI;
using namespace Windows::Foundation;
using namespace Microsoft::WRL;
using namespace Windows::Devices::Input;
using namespace Windows::System;

namespace Diligent
{

InputControllerEventHandlerUWP^ InputControllerEventHandlerUWP::Create(_In_ CoreWindow^ window, std::shared_ptr<InputControllerUWP::ControllerState> controllerState)
{
    auto p = ref new InputControllerEventHandlerUWP(window, controllerState);
    return static_cast<InputControllerEventHandlerUWP^>(p);
}

InputControllerEventHandlerUWP::InputControllerEventHandlerUWP(_In_ CoreWindow^ window, std::shared_ptr<InputControllerUWP::ControllerState> controllerState) :
    m_SharedControllerState(std::move(controllerState))
{
    window->PointerPressed +=
        ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &InputControllerEventHandlerUWP::OnPointerPressed);

    window->PointerMoved +=
        ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &InputControllerEventHandlerUWP::OnPointerMoved);

    window->PointerReleased +=
        ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &InputControllerEventHandlerUWP::OnPointerReleased);

    window->PointerExited +=
        ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &InputControllerEventHandlerUWP::OnPointerExited);

    window->KeyDown +=
        ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &InputControllerEventHandlerUWP::OnKeyDown);

    window->KeyUp +=
        ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &InputControllerEventHandlerUWP::OnKeyUp);

    window->PointerWheelChanged +=
        ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &InputControllerEventHandlerUWP::OnPointerWheelChanged);

    // There is a separate handler for mouse only relative mouse movement events.
#if (WINAPI_FAMILY != WINAPI_FAMILY_PHONE_APP)
    MouseDevice::GetForCurrentView()->MouseMoved +=
        ref new TypedEventHandler<MouseDevice^, MouseEventArgs^>(this, &InputControllerEventHandlerUWP::OnMouseMoved);
#endif
}

void InputControllerEventHandlerUWP::ClearState()
{
    std::lock_guard<std::mutex> lock(m_SharedControllerState->mtx);
    auto& mouseState = m_SharedControllerState->mouseState;
    mouseState.WheelDelta = 0;
    for(Uint32 i=0; i < _countof(m_SharedControllerState->keySates); ++i)
    {
        auto& key = m_SharedControllerState->keySates[i];
        if (key & INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN)
            key &= ~INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN;
    }
}

void InputControllerEventHandlerUWP::OnPointerPressed(
    _In_ CoreWindow^ /* sender */,
    _In_ PointerEventArgs^ args
    )
{
    PointerPoint^ point = args->CurrentPoint;
    uint32 pointerID = point->PointerId;
    Point pointerPosition = point->Position;
    PointerPointProperties^ pointProperties = point->Properties;

    std::lock_guard<std::mutex> lock(m_SharedControllerState->mtx);
    auto& mouseState = m_SharedControllerState->mouseState;
    if(pointProperties->IsLeftButtonPressed)
        mouseState.ButtonFlags |= MouseState::BUTTON_FLAG_LEFT;

    if(pointProperties->IsRightButtonPressed)
        mouseState.ButtonFlags |= MouseState::BUTTON_FLAG_RIGHT;

    if(pointProperties->IsMiddleButtonPressed)
        mouseState.ButtonFlags |= MouseState::BUTTON_FLAG_MIDDLE;
}

//----------------------------------------------------------------------

void InputControllerEventHandlerUWP::OnPointerMoved(
    _In_ CoreWindow^ /* sender */,
    _In_ PointerEventArgs^ args
    )
{
    PointerPoint^ point = args->CurrentPoint;
    uint32 pointerID = point->PointerId;
    Point pointerPosition = point->Position;
    PointerPointProperties^ pointProperties = point->Properties;
    auto pointerDevice = point->PointerDevice;

    std::lock_guard<std::mutex> lock(m_SharedControllerState->mtx);
    auto& mouseState = m_SharedControllerState->mouseState;

    if (m_LastMousePosX >= 0 && m_LastMousePosY >= 0)
    {
        mouseState.PosX = static_cast<float>(pointerPosition.X);
        mouseState.PosY = static_cast<float>(pointerPosition.Y);
    }
    m_LastMousePosX = pointerPosition.X;
    m_LastMousePosY = pointerPosition.Y;
}


void InputControllerEventHandlerUWP::OnPointerWheelChanged(
    _In_ CoreWindow^ /* sender */,
    _In_ PointerEventArgs^ args
    )
{
    PointerPoint^ point = args->CurrentPoint;
    uint32 pointerID = point->PointerId;
    Point pointerPosition = point->Position;
    PointerPointProperties^ pointProperties = point->Properties;
    auto pointerDevice = point->PointerDevice;

    std::lock_guard<std::mutex> lock(m_SharedControllerState->mtx);
    auto& mouseState = m_SharedControllerState->mouseState;
    mouseState.WheelDelta = static_cast<float>(pointProperties->MouseWheelDelta) / static_cast<float>(WHEEL_DELTA);
}
//----------------------------------------------------------------------

void InputControllerEventHandlerUWP::OnMouseMoved(
    _In_ MouseDevice^ /* mouseDevice */,
    _In_ MouseEventArgs^ args
    )
{
    //std::lock_guard<std::mutex> lock(m_SharedControllerState->mtx);
    //auto& mouseState = m_SharedControllerState->mouseState;
    //// Handle Mouse Input via dedicated relative movement handler.
    //mouseState.DeltaX = static_cast<float>(args->MouseDelta.X);
    //mouseState.DeltaY = static_cast<float>(args->MouseDelta.Y);
}

//----------------------------------------------------------------------

void InputControllerEventHandlerUWP::OnPointerReleased(
    _In_ CoreWindow^ /* sender */,
    _In_ PointerEventArgs^ args
    )
{
    PointerPoint^ point = args->CurrentPoint;
    uint32 pointerID = point->PointerId;
    Point pointerPosition = point->Position;
    PointerPointProperties^ pointProperties = point->Properties;

    std::lock_guard<std::mutex> lock(m_SharedControllerState->mtx);
    auto& mouseState = m_SharedControllerState->mouseState;

    if(!pointProperties->IsLeftButtonPressed)
        mouseState.ButtonFlags &= ~MouseState::BUTTON_FLAG_LEFT;

    if(!pointProperties->IsRightButtonPressed)
        mouseState.ButtonFlags &= ~MouseState::BUTTON_FLAG_RIGHT;

    if(!pointProperties->IsMiddleButtonPressed)
        mouseState.ButtonFlags &= ~MouseState::BUTTON_FLAG_MIDDLE;
}

//----------------------------------------------------------------------

void InputControllerEventHandlerUWP::OnPointerExited(
    _In_ CoreWindow^ /* sender */,
    _In_ PointerEventArgs^ args
    )
{
    PointerPoint^ point = args->CurrentPoint;
    uint32 pointerID = point->PointerId;
    Point pointerPosition = point->Position;
    PointerPointProperties^ pointProperties = point->Properties;

    std::lock_guard<std::mutex> lock(m_SharedControllerState->mtx);
    auto& mouseState = m_SharedControllerState->mouseState;

    if(!pointProperties->IsLeftButtonPressed)
        mouseState.ButtonFlags &= ~MouseState::BUTTON_FLAG_LEFT;

    if(!pointProperties->IsRightButtonPressed)
        mouseState.ButtonFlags &= ~MouseState::BUTTON_FLAG_RIGHT;

    if(!pointProperties->IsMiddleButtonPressed)
        mouseState.ButtonFlags &= ~MouseState::BUTTON_FLAG_MIDDLE;
}

namespace
{

char VirtualKeyToChar(VirtualKey Key, bool bAltPressed, bool bShiftPressed, bool bCtrlPressed)
{
    int Char = 0;
    if( bShiftPressed )
    {
        if(Key>=VirtualKey::A && Key<=VirtualKey::Z)
            Char = 'A' + ((int)Key - (int)VirtualKey::A);
        else
        {
            switch(Key)
            {
                case VirtualKey::Number0: Char = ')'; break;
                case VirtualKey::Number1: Char = '!'; break;
                case VirtualKey::Number2: Char = '@'; break;
                case VirtualKey::Number3: Char = '#'; break;
                case VirtualKey::Number4: Char = '$'; break;
                case VirtualKey::Number5: Char = '%'; break;
                case VirtualKey::Number6: Char = '^'; break;
                case VirtualKey::Number7: Char = '&'; break;
                case VirtualKey::Number8: Char = '*'; break;
                case VirtualKey::Number9: Char = '('; break;

                case static_cast<VirtualKey>(189): Char = '_'; break;
                case static_cast<VirtualKey>(187): Char = '+'; break;
                case static_cast<VirtualKey>(219): Char = '{'; break;
                case static_cast<VirtualKey>(221): Char = '}'; break;
                case static_cast<VirtualKey>(220): Char = '|'; break;
                case static_cast<VirtualKey>(186): Char = ':'; break;
                case static_cast<VirtualKey>(222): Char = '\"'; break;
                case static_cast<VirtualKey>(188): Char = '<'; break;
                case static_cast<VirtualKey>(190): Char = '>'; break;
                case static_cast<VirtualKey>(191): Char = '?'; break;
            }
        }
    }
    else
    {
        if( Key>=VirtualKey::Number0 && Key<=VirtualKey::Number9 )
            Char = '0' + ((int)Key - (int)VirtualKey::Number0);
        else if( Key>=VirtualKey::NumberPad0 && Key<=VirtualKey::NumberPad9)
            Char = '0' + ((int)Key - (int)VirtualKey::NumberPad0);
        else if(Key>=VirtualKey::A && Key<=VirtualKey::Z)
            Char = 'a' + ((int)Key - (int)VirtualKey::A);
        else
        {
            switch(Key)
            {
                case static_cast<VirtualKey>(189): Char = '-'; break;
                case static_cast<VirtualKey>(187): Char = '='; break;
                case static_cast<VirtualKey>(219): Char = '['; break;
                case static_cast<VirtualKey>(221): Char = ']'; break;
                case static_cast<VirtualKey>(220): Char = '\\'; break;
                case static_cast<VirtualKey>(186): Char = ';'; break;
                case static_cast<VirtualKey>(222): Char = '\''; break;
                case static_cast<VirtualKey>(188): Char = ','; break;
                case static_cast<VirtualKey>(190): Char = '.'; break;
                case static_cast<VirtualKey>(191): Char = '/'; break;
            }
        }
    }
    return static_cast<char>(Char);
}

InputKeys VirtualKeyToInputKey(VirtualKey Key)
{
    if (Key>=VirtualKey::F1 && Key<=VirtualKey::F15)
    {
        //k = TW_KEY_F1 + ((int)Key-(int)VirtualKey::F1);
    }
    else
    {
        switch( Key )
        {
            case VirtualKey::Up:       return InputKeys::MoveForward;
            case VirtualKey::Down:     return InputKeys::MoveBackward;
            case VirtualKey::Left:     return InputKeys::MoveLeft;
            case VirtualKey::Right:    return InputKeys::MoveRight;

            case VirtualKey::Insert:   return InputKeys::Unknown;
            case VirtualKey::Delete:   return InputKeys::Unknown;
            case VirtualKey::Back:     return InputKeys::Unknown;

            case VirtualKey::PageUp:   return InputKeys::MoveUp;
            case VirtualKey::PageDown: return InputKeys::MoveDown;
            case VirtualKey::Home:     return InputKeys::Reset;

            case VirtualKey::End:      return InputKeys::Unknown;
            case VirtualKey::Enter:    return InputKeys::Unknown;
            case VirtualKey::Divide:   return InputKeys::Unknown;
            case VirtualKey::Multiply: return InputKeys::Unknown;

            case VirtualKey::Subtract: return InputKeys::ZoomOut;
            case VirtualKey::Add:      return InputKeys::ZoomIn;

            case VirtualKey::Decimal:  return InputKeys::Unknown;

            case VirtualKey::Shift:    return InputKeys::ShiftDown;
            case VirtualKey::Control:  return InputKeys::ControlDown;
            case VirtualKey::Menu:     return InputKeys::AltDown;

            default:
            {
                auto c = VirtualKeyToChar(Key, false, false, false);
                switch(c)
                {
                    case 'w': return InputKeys::MoveForward;
                    case 's': return InputKeys::MoveBackward;
                    case 'a': return InputKeys::MoveLeft;
                    case 'd': return InputKeys::MoveRight;
                    case 'q': return InputKeys::MoveUp;
                    case 'e': return InputKeys::MoveDown;
                }
            }
        }
    }

    return InputKeys::Unknown;
}

} // namespace

void InputControllerEventHandlerUWP::OnKeyDown(
    _In_ CoreWindow^ /* sender */,
    _In_ KeyEventArgs^ args
    )
{
    auto inputKey = VirtualKeyToInputKey(args->VirtualKey);
    if (inputKey != InputKeys::Unknown && inputKey < InputKeys::TotalKeys)
    {
        std::lock_guard<std::mutex> lock(m_SharedControllerState->mtx);
        auto& keyState = m_SharedControllerState->keySates[static_cast<size_t>(inputKey)];
        keyState &= ~INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN;
        keyState |= INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
    }
}

//----------------------------------------------------------------------

void InputControllerEventHandlerUWP::OnKeyUp(
    _In_ CoreWindow^ /* sender */,
    _In_ KeyEventArgs^ args
    )
{
    auto inputKey = VirtualKeyToInputKey(args->VirtualKey);
    if (inputKey != InputKeys::Unknown && inputKey < InputKeys::TotalKeys)
    {
        std::lock_guard<std::mutex> lock(m_SharedControllerState->mtx);
        auto& keyState = m_SharedControllerState->keySates[static_cast<size_t>(inputKey)];
        keyState &= ~INPUT_KEY_STATE_FLAG_KEY_IS_DOWN;
        keyState |= INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN;
    }
}


//----------------------------------------------------------------------
#if 0
void InputControllerEventHandlerUWP::ShowCursor()
{
    // Turn on mouse cursor.
    // This also disables relative mouse movement tracking.
    auto window = CoreWindow::GetForCurrentThread();
    if (window)
    {
        // Protect case where there isn't a window associated with the current thread.
        // This happens on initialization or when being called from a background thread.
        window->PointerCursor = ref new CoreCursor(CoreCursorType::Arrow, 0);
    }
}

//----------------------------------------------------------------------

void InputControllerEventHandlerUWP::HideCursor()
{
    // Turn mouse cursor off (hidden).
    // This enables relative mouse movement tracking.
    auto window = CoreWindow::GetForCurrentThread();
    if (window)
    {
        // Protect case where there isn't a window associated with the current thread.
        // This happens on initialization or when being called from a background thread.
        window->PointerCursor = nullptr;
    }
}
#endif

}
