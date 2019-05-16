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

#include "BasicTypes.h"
#include "FlagEnum.h"

namespace Diligent
{

struct MouseState
{
    enum BUTTON_FLAGS : Uint8
    {
        BUTTON_FLAG_NONE    = 0x00,
        BUTTON_FLAG_LEFT    = 0x01,
        BUTTON_FLAG_MIDDLE  = 0x02,
        BUTTON_FLAG_RIGHT   = 0x04,
        BUTTON_FLAG_WHEEL   = 0x08
    };

    Float32      DeltaX        = 0;
    Float32      DeltaY        = 0;
    BUTTON_FLAGS ButtonFlags   = BUTTON_FLAG_NONE;
    Float32      WheelDelta    = 0;
};
DEFINE_FLAG_ENUM_OPERATORS(MouseState::BUTTON_FLAGS)


enum class InputKeys
{
    Unknown = 0,
    MoveLeft,
    MoveRight,
    MoveForward,
    MoveBackward,
    MoveUp,
    MoveDown,
    Reset,
    ControlDown,
    ShiftDown,
    AltDown,
    ZoomIn,
    ZoomOut,
    TotalKeys
};

enum INPUT_KEY_STATE_FLAGS : Uint8
{
    INPUT_KEY_STATE_FLAG_KEY_NONE     = 0x00,
    INPUT_KEY_STATE_FLAG_KEY_IS_DOWN  = 0x01,
    INPUT_KEY_STATE_FLAG_KEY_WAS_DOWN = 0x80
};
DEFINE_FLAG_ENUM_OPERATORS(INPUT_KEY_STATE_FLAGS)


}

#if PLATFORM_WIN32
    #include "Win32/InputControllerWin32.h"
    namespace Diligent
    {
        using InputController = InputControllerWin32;
    }
#elif PLATFORM_UNIVERSAL_WINDOWS
    #include "UWP/InputControllerUWP.h"
    namespace Diligent
    {
        using InputController = InputControllerUWP;
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
