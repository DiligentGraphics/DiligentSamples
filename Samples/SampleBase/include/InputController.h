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

#pragma once

#include "BasicMath.h"
#include "FlagEnum.h"

namespace Diligent
{
    
enum MOUSE_BUTTON_MASK : Uint8
{
    MOUSE_BUTTON_NONE   = 0x00,
    MOUSE_LEFT_BUTTON   = 0x01,
    MOUSE_MIDDLE_BUTTON = 0x02,
    MOUSE_RIGHT_BUTTON  = 0x04,
    MOUSE_WHEEL         = 0x08
};
DEFINE_FLAG_ENUM_OPERATORS(MOUSE_BUTTON_MASK)

enum KEY_STATE_MASK : Uint8
{
    KEY_IS_DOWN_MASK  = 0x01,
    KEY_WAS_DOWN_MASK = 0x80
};
DEFINE_FLAG_ENUM_OPERATORS(KEY_STATE_MASK)

struct MouseState
{
    Float32           DeltaX        = 0;
    Float32           DeltaY        = 0;
    MOUSE_BUTTON_MASK ButtonMask    = MOUSE_BUTTON_NONE;
    Float32           WheelDelta    = 0;
};

enum class InputKeys
{
    MoveLeft = 0,
    MoveRight,
    MoveForward,
    MoveBackward,
    MoveUp,
    MoveDown,
    Reset,
    ControlDown,
    ShiftDown,
    ZoomIn,
    ZoomOut,
    Unknown,
    TotalKeys
};

}

#if PLATFORM_WIN32
    #include "Win32/InputControllerWin32.h"
    namespace Diligent
    {
        using InputController = InputControllerWin32;
    }
#else
    namespace Diligent
    {
        class DummyInputController
        {
        public:
            bool HandleNativeMessage(const void* ) {return false;}

            const MouseState& GetMouseState(){return m_MouseState;}

            KEY_STATE_MASK GetKeyState(InputKeys Key)const
            {
                return static_cast<KEY_STATE_MASK>(0);
            }

        private:
            MouseState m_MouseState;
        };
        using InputController = DummyInputController;
    }
#endif
