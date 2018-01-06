/*     Copyright 2015-2016 Egor Yusov
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

//  ---------------------------------------------------------------------------
//
//  @file       TwEventWinUWP.cpp
//  @brief      Helper: 
//              translate and re-send mouse and keyboard events 
//              from Windows Runtime message proc to AntTweakBar
//  
//  @author     Egor Yusov
//  @date       2015/06/30
//  @note       This file is not part of the AntTweakBar library because
//              it is not recommended to pack Windows Runtime extensions 
//              into a static library
//  ---------------------------------------------------------------------------
#include "pch.h"
#include "pch2.h"

#include "TwEventUWP.h"

#include <AntTweakBar.h>


// Mouse wheel support
#if !defined WHEEL_DELTA
#define      WHEEL_DELTA 120
#endif    // WHEEL_DELTA



using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI;
using namespace Windows::Foundation;
using namespace Microsoft::WRL;
using namespace DirectX;
using namespace Windows::Devices::Input;
using namespace Windows::System;

TwEventUWPHelper^ TwEventUWPHelper::Create(_In_ CoreWindow^ window)
{
    auto p = ref new TwEventUWPHelper(window);
    return static_cast<TwEventUWPHelper^>(p);
}

TwEventUWPHelper::TwEventUWPHelper(_In_ CoreWindow^ window) : 
    s_PrevKeyDown(0),
    s_PrevKeyDownMod(0),
    s_PrevKeyDownHandled(0),
    m_bShiftPressed(false),
    m_bCtrlPressed(false),
    m_bAltPressed(false),
    m_bLMBPressed(false),
    m_bRMBPressed(false),
    m_bMMBPressed(false),
    s_WheelPos(0)
{
    window->PointerPressed +=
        ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &TwEventUWPHelper::OnPointerPressed);

    window->PointerMoved +=
        ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &TwEventUWPHelper::OnPointerMoved);

    window->PointerReleased +=
        ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &TwEventUWPHelper::OnPointerReleased);

    window->PointerExited +=
        ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &TwEventUWPHelper::OnPointerExited);

    window->KeyDown +=
        ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &TwEventUWPHelper::OnKeyDown);

    window->KeyUp +=
        ref new TypedEventHandler<CoreWindow^, KeyEventArgs^>(this, &TwEventUWPHelper::OnKeyUp);

    window->PointerWheelChanged +=
        ref new TypedEventHandler<CoreWindow^, PointerEventArgs^>(this, &TwEventUWPHelper::OnPointerWheelChanged);

    // There is a separate handler for mouse only relative mouse movement events.
#if (WINAPI_FAMILY != WINAPI_FAMILY_PHONE_APP)
    MouseDevice::GetForCurrentView()->MouseMoved +=
        ref new TypedEventHandler<MouseDevice^, MouseEventArgs^>(this, &TwEventUWPHelper::OnMouseMoved);
#endif
}

void TwEventUWPHelper::OnPointerPressed(
    _In_ CoreWindow^ /* sender */,
    _In_ PointerEventArgs^ args
    )
{
    PointerPoint^ point = args->CurrentPoint;
    uint32 pointerID = point->PointerId;
    Point pointerPosition = point->Position;
    PointerPointProperties^ pointProperties = point->Properties;
    auto pointerDevice = point->PointerDevice;
    auto pointerDeviceType = pointerDevice->PointerDeviceType;

    XMFLOAT2 position = XMFLOAT2(pointerPosition.X, pointerPosition.Y);

#ifdef TwEventUWPHelper_TRACE
    DebugTrace(L"%-7s (%d) at (%4.0f, %4.0f)", L"Pressed", pointerID, position.x, position.y);
#endif

#if (WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
    TwMouseMotion((short)position.x, (short)position.y);
#endif


    m_bLMBPressed = pointProperties->IsLeftButtonPressed;
    if( m_bLMBPressed )
    {
        auto handled = TwMouseButton(TW_MOUSE_PRESSED, TW_MOUSE_LEFT);
    }

    m_bRMBPressed = pointProperties->IsRightButtonPressed;
    if(m_bRMBPressed)
    {
        auto handled = TwMouseButton(TW_MOUSE_PRESSED, TW_MOUSE_RIGHT);
    }

    m_bMMBPressed = pointProperties->IsMiddleButtonPressed;
    if(m_bMMBPressed)
    {
        auto handled = TwMouseButton(TW_MOUSE_PRESSED, TW_MOUSE_MIDDLE);
    }
}

//----------------------------------------------------------------------

void TwEventUWPHelper::OnPointerMoved(
    _In_ CoreWindow^ /* sender */,
    _In_ PointerEventArgs^ args
    )
{
    PointerPoint^ point = args->CurrentPoint;
    uint32 pointerID = point->PointerId;
    Point pointerPosition = point->Position;
    PointerPointProperties^ pointProperties = point->Properties;
    auto pointerDevice = point->PointerDevice;

    XMFLOAT2 position = XMFLOAT2(pointerPosition.X, pointerPosition.Y);     // convert to allow math

#ifdef TwEventUWPHelper_TRACE
    DebugTrace(L"%-7s (%d) at (%4.0f, %4.0f)", L"Moved", pointerID, position.x, position.y);
#endif

    auto handled = TwMouseMotion((short)position.x, (short)position.y);
}

void TwEventUWPHelper::OnPointerWheelChanged(
    _In_ CoreWindow^ /* sender */,
    _In_ PointerEventArgs^ args
    )
{
    PointerPoint^ point = args->CurrentPoint;
    uint32 pointerID = point->PointerId;
    Point pointerPosition = point->Position;
    PointerPointProperties^ pointProperties = point->Properties;
    auto pointerDevice = point->PointerDevice;

    s_WheelPos += pointProperties->MouseWheelDelta/WHEEL_DELTA;
    auto handled = TwMouseWheel(s_WheelPos);
}
//----------------------------------------------------------------------

void TwEventUWPHelper::OnMouseMoved(
    _In_ MouseDevice^ /* mouseDevice */,
    _In_ MouseEventArgs^ args
    )
{
    // Handle Mouse Input via dedicated relative movement handler.
    XMFLOAT2 mouseDelta;
    mouseDelta.x = static_cast<float>(args->MouseDelta.X);
    mouseDelta.y = static_cast<float>(args->MouseDelta.Y);

    //auto handled = TwMouseMotion((short)position.x, (short)position.y);
}

//----------------------------------------------------------------------

void TwEventUWPHelper::OnPointerReleased(
    _In_ CoreWindow^ /* sender */,
    _In_ PointerEventArgs^ args
    )
{
    PointerPoint^ point = args->CurrentPoint;
    uint32 pointerID = point->PointerId;
    Point pointerPosition = point->Position;
    PointerPointProperties^ pointProperties = point->Properties;

    XMFLOAT2 position = XMFLOAT2(pointerPosition.X, pointerPosition.Y);

#ifdef TwEventUWPHelper_TRACE
    DebugTrace(L"%-7s (%d) at (%4.0f, %4.0f)\n", L"Release", pointerID, position.x, position.y);
#endif

#if (WINAPI_FAMILY == WINAPI_FAMILY_PHONE_APP)
    TwMouseMotion((short)position.x, (short)position.y);
#endif

    if( m_bLMBPressed && !pointProperties->IsLeftButtonPressed )
    {
        auto handled = TwMouseButton(TW_MOUSE_RELEASED, TW_MOUSE_LEFT);
    }
    
    if( m_bRMBPressed && !pointProperties->IsRightButtonPressed )
    {
        auto handled = TwMouseButton(TW_MOUSE_RELEASED, TW_MOUSE_RIGHT);
    }

    if( m_bMMBPressed && !pointProperties->IsMiddleButtonPressed )
    {
        auto handled = TwMouseButton(TW_MOUSE_RELEASED, TW_MOUSE_MIDDLE);
    }

    m_bLMBPressed = pointProperties->IsLeftButtonPressed;
    m_bRMBPressed = pointProperties->IsRightButtonPressed;
    m_bMMBPressed = pointProperties->IsMiddleButtonPressed;
}

//----------------------------------------------------------------------

void TwEventUWPHelper::OnPointerExited(
    _In_ CoreWindow^ /* sender */,
    _In_ PointerEventArgs^ args
    )
{
    PointerPoint^ point = args->CurrentPoint;
    uint32 pointerID = point->PointerId;
    Point pointerPosition = point->Position;
    PointerPointProperties^ pointProperties = point->Properties;

    XMFLOAT2 position  = XMFLOAT2(pointerPosition.X, pointerPosition.Y);

#ifdef TwEventUWPHelper_TRACE
    DebugTrace(L"%-7s (%d) at (%4.0f, %4.0f)\n", L"Exit", pointerID, position.x, position.y);
#endif

    if( m_bLMBPressed  )
    {
        auto handled = TwMouseButton(TW_MOUSE_RELEASED, TW_MOUSE_LEFT);
    }
    
    if( m_bRMBPressed )
    {
        auto handled = TwMouseButton(TW_MOUSE_RELEASED, TW_MOUSE_RIGHT);
    }

    if( m_bMMBPressed )
    {
        auto handled = TwMouseButton(TW_MOUSE_RELEASED, TW_MOUSE_MIDDLE);
    }

    m_bLMBPressed = false;
    m_bRMBPressed = false;
    m_bMMBPressed = false;
}

//----------------------------------------------------------------------
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

void TwEventUWPHelper::OnKeyDown(
    _In_ CoreWindow^ /* sender */,
    _In_ KeyEventArgs^ args
    )
{
    Windows::System::VirtualKey Key;
    Key = args->VirtualKey;

    int handled = 0;
    int kmod = 0;
    int testkp = 0;  // What is it?
    int k = 0;
            
    if( m_bShiftPressed )
        kmod |= TW_KMOD_SHIFT;
    if( m_bCtrlPressed )
    {
        kmod |= TW_KMOD_CTRL;
        //testkp = 1;
    }
    if( m_bAltPressed )
    {
        kmod |= TW_KMOD_ALT;
        //testkp = 1;
    }
    testkp = 1;

    if( Key>=VirtualKey::F1 && Key<=VirtualKey::F15 )
        k = TW_KEY_F1 + ((int)Key-(int)VirtualKey::F1);
    else
    {
        switch( Key )
        {
        case VirtualKey::Up:
            k = TW_KEY_UP;
            break;
        case VirtualKey::Down:
            k = TW_KEY_DOWN;
            break;
        case VirtualKey::Left:
            k = TW_KEY_LEFT;
            break;
        case VirtualKey::Right:
            k = TW_KEY_RIGHT;
            break;
        case VirtualKey::Insert:
            k = TW_KEY_INSERT;
            break;
        case VirtualKey::Delete:
            k = TW_KEY_DELETE;
            break;
        case VirtualKey::Back:
            k = TW_KEY_BACKSPACE;
            break;
        case VirtualKey::PageUp:
            k = TW_KEY_PAGE_UP;
            break;
        case VirtualKey::PageDown:
            k = TW_KEY_PAGE_DOWN;
            break;
        case VirtualKey::Home:
            k = TW_KEY_HOME;
            break;
        case VirtualKey::End:
            k = TW_KEY_END;
            break;
        case VirtualKey::Enter:
            k = TW_KEY_RETURN;
            break;
        case VirtualKey::Divide:
            if( testkp )
                k = '/';
            break;
        case VirtualKey::Multiply:
            if( testkp )
                k = '*';
            break;
        case VirtualKey::Subtract:
            if( testkp )
                k = '-';
            break;
        case VirtualKey::Add:
            if( testkp )
                k = '+';
            break;
        case VirtualKey::Decimal:
            if( testkp )
                k = '.';
            break;
        case VirtualKey::Shift:
            m_bShiftPressed = true;
            break;
        case VirtualKey::Control:
            m_bCtrlPressed = true;
            break;
        case VirtualKey::Menu:
            m_bAltPressed = true;
            break;

        default:
            k = VirtualKeyToChar(Key, m_bAltPressed, m_bShiftPressed, m_bCtrlPressed);
            //if( (kmod&TW_KMOD_CTRL) && (kmod&TW_KMOD_ALT) )
                //   k = MapVirtualKey( (UINT)wParam, 2 ) & 0x0000FFFF;
        }
    }

    if( k!=0 )
        handled = TwKeyPressed(k, kmod);
    else
    {
#if 0
        // if the key will be handled at next WM_CHAR report this event as handled
        int key = (int)(wParam&0xff);
        if( kmod&TW_KMOD_CTRL && key>0 && key<27 )
            key += 'a'-1;
        if( key>0 && key<256 )
            handled = TwKeyTest(key, kmod);
#endif
    }

#if 0
    case WM_CHAR:
    case WM_SYSCHAR:
        {
            int key = (int)(wParam&0xff);
            int kmod = 0;

            if( GetAsyncKeyState(VK_SHIFT)<0 )
                kmod |= TW_KMOD_SHIFT;
            if( GetAsyncKeyState(VK_CONTROL)<0 )
            {
                kmod |= TW_KMOD_CTRL;
                if( key>0 && key<27 )
                    key += 'a'-1;
            }
            if( GetAsyncKeyState(VK_MENU)<0 )
                kmod |= TW_KMOD_ALT;
            if( key>0 && key<256 )
                handled = TwKeyPressed(key, kmod);
        }
        break;
#endif

    //s_PrevKeyDown = wParam;
    s_PrevKeyDownMod = kmod;
    s_PrevKeyDownHandled = handled;
}

//----------------------------------------------------------------------

void TwEventUWPHelper::OnKeyUp(
    _In_ CoreWindow^ /* sender */,
    _In_ KeyEventArgs^ args
    )
{
    Windows::System::VirtualKey Key;
    Key = args->VirtualKey;

    if( Key == VirtualKey::Shift )
        m_bShiftPressed = false;
    if( Key == VirtualKey::Control )
        m_bCtrlPressed = false;
    if( Key == VirtualKey::Menu )
        m_bAltPressed = false;

    int kmod = 0;
    if( m_bShiftPressed )
        kmod |= TW_KMOD_SHIFT;
    if( m_bCtrlPressed )
        kmod |= TW_KMOD_CTRL;
    if( m_bAltPressed )
        kmod |= TW_KMOD_ALT;
#if 0
    // if the key has been handled at previous WM_KEYDOWN report this event as handled
    if( s_PrevKeyDown==wParam && s_PrevKeyDownMod==kmod )
        handled = s_PrevKeyDownHandled;
    else 
    {
        // if the key would have been handled, report this event as handled
        int key = (int)(wParam&0xff);
        if( kmod&TW_KMOD_CTRL && key>0 && key<27 )
            key += 'a'-1;
        if( key>0 && key<256 )
            handled = TwKeyTest(key, kmod);
    }
#endif

    // reset previous keydown
    s_PrevKeyDown = 0;
    s_PrevKeyDownMod = 0;
    s_PrevKeyDownHandled = 0;
}


//----------------------------------------------------------------------

void TwEventUWPHelper::ShowCursor()
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

void TwEventUWPHelper::HideCursor()
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


#if 0
#define WIN32_LEAN_AND_MEAN // Exclude rarely-used stuff from Windows headers
#include <windows.h>

// TwEventWin returns zero if msg has not been handled, 
// and a non-zero value if it has been handled by the AntTweakBar library.
int TW_CALL TwEventWin(void *wnd, unsigned int msg, unsigned PARAM_INT _W64 wParam, PARAM_INT _W64 lParam)
{
    int handled = 0;

    switch( msg ) 
    {
    case WM_MOUSEMOVE:
        // send signed! mouse coordinates
        handled = TwMouseMotion((short)LOWORD(lParam), (short)HIWORD(lParam));
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
        SetCapture(wnd);
        handled = TwMouseButton(TW_MOUSE_PRESSED, TW_MOUSE_LEFT);
        break;
    case WM_LBUTTONUP:
        ReleaseCapture();
        handled = TwMouseButton(TW_MOUSE_RELEASED, TW_MOUSE_LEFT);
        break;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONDBLCLK:
        SetCapture(wnd);
        handled = TwMouseButton(TW_MOUSE_PRESSED, TW_MOUSE_MIDDLE);
        break;
    case WM_MBUTTONUP:
        ReleaseCapture();
        handled = TwMouseButton(TW_MOUSE_RELEASED, TW_MOUSE_MIDDLE);
        break;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONDBLCLK:
        SetCapture(wnd);
        handled = TwMouseButton(TW_MOUSE_PRESSED, TW_MOUSE_RIGHT);
        break;
    case WM_RBUTTONUP:
        ReleaseCapture();
        handled = TwMouseButton(TW_MOUSE_RELEASED, TW_MOUSE_RIGHT);
        break;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        {

        }
        break;
    case WM_SIZE:
        // tell the new size to AntTweakBar
        TwWindowSize(LOWORD(lParam), HIWORD(lParam));
        // do not set 'handled', WM_SIZE may be also processed by the calling application
        break;
    }

    if( handled )
        // Event has been handled by AntTweakBar, so we invalidate the window 
        // content to send a WM_PAINT which will redraw the tweak bar(s).
        InvalidateRect(wnd, NULL, FALSE);

    return handled;
}

#endif
