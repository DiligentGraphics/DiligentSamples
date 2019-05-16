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

namespace Diligent
{
    
class InputControllerIOS
{
public:
    const MouseState& GetMouseState()
    {
        return m_MouseState;
    }

    INPUT_KEY_STATE_FLAGS GetKeyState(InputKeys Key)const
    {
        return m_Keys[static_cast<size_t>(Key)];
    }
    
    enum class MouseButtonEvent
    {
        LMB_Pressed,
        LMB_Released,
        RMB_Pressed,
        RMB_Released
    };
    void OnMouseButtonEvent(MouseButtonEvent Event);
    
    void OnMouseMove(float MouseX, float MouseY)
    {
        m_MouseState.PosX = MouseX;
        m_MouseState.PosY = MouseY;
    }
    
    void ClearState();
    
private:
    INPUT_KEY_STATE_FLAGS m_Keys[static_cast<size_t>(InputKeys::TotalKeys)] = {};
    MouseState m_MouseState;
};

}
