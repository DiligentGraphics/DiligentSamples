/*
 *  Copyright 2019-2023 Diligent Graphics LLC
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
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

#include <algorithm>

#include "BasicMath.hpp"
#include "InputController.hpp"
#include "DebugUtilities.hpp"

namespace Diligent
{

template <typename T = float>
class TrackballCamera
{
public:
    void Update(InputController& Controller)
    {
        const auto& Mouse = Controller.GetMouseState();

        T MouseDeltaX = 0;
        T MouseDeltaY = 0;
        if (m_LastMouseState.PosX >= 0 && m_LastMouseState.PosY >= 0 &&
            m_LastMouseState.ButtonFlags != MouseState::BUTTON_FLAG_NONE)
        {
            MouseDeltaX = Mouse.PosX - m_LastMouseState.PosX;
            MouseDeltaY = Mouse.PosY - m_LastMouseState.PosY;
        }
        m_LastMouseState = Mouse;

        auto fYawDelta   = MouseDeltaX * m_RotationSpeed;
        auto fPitchDelta = MouseDeltaY * m_RotationSpeed;
        if (Mouse.ButtonFlags & MouseState::BUTTON_FLAG_LEFT)
        {
            m_Yaw += (m_IsLeftHanded ? -fYawDelta : +fYawDelta);
            m_Pitch += fPitchDelta;
            m_Pitch = std::max(m_Pitch, -static_cast<T>(PI / 2.0));
            m_Pitch = std::min(m_Pitch, +static_cast<T>(PI / 2.0));
        }

        m_PrimaryRotation =
            Quaternion<T>::RotationFromAxisAngle(Vector3<T>{1, 0, 0}, -m_Pitch) *
            Quaternion<T>::RotationFromAxisAngle(Vector3<T>{0, 1, 0}, -m_Yaw) *
            m_ExtraRotation;

        if (Mouse.ButtonFlags & MouseState::BUTTON_FLAG_RIGHT)
        {
            auto CameraView  = m_PrimaryRotation.ToMatrix();
            auto CameraWorld = CameraView.Transpose();

            auto CameraRight = Vector3<T>::MakeVector(CameraWorld[0]);
            auto CameraUp    = Vector3<T>::MakeVector(CameraWorld[1]);
            m_SecondaryRotation =
                Quaternion<T>::RotationFromAxisAngle(CameraRight, -fPitchDelta) *
                Quaternion<T>::RotationFromAxisAngle(CameraUp, m_IsLeftHanded ? +fYawDelta : -fYawDelta) *
                m_SecondaryRotation;
        }

        m_Dist -= Mouse.WheelDelta * m_ZoomSpeed;
        m_Dist = clamp(m_Dist, m_MinDist, m_MaxDist);

        if ((Controller.GetKeyState(InputKeys::Reset) & INPUT_KEY_STATE_FLAG_KEY_IS_DOWN) != 0)
            ResetDefaults();
    }

    void SetRotation(T Yaw, T Pitch)
    {
        m_Yaw   = Yaw;
        m_Pitch = Pitch;
    }

    void SetSecondaryRotation(const Quaternion<T>& Rotation)
    {
        m_SecondaryRotation = Rotation;
    }

    void SetExtraRotation(const Quaternion<T>& Rotation)
    {
        m_ExtraRotation = Rotation;
    }

    void SetDist(T Dist)
    {
        VERIFY_EXPR(Dist >= 0);
        m_Dist = Dist;
    }

    void SetDefaultRotation(T Yaw, T Pitch)
    {
        m_DefaultYaw   = Yaw;
        m_DefaultPitch = Pitch;
    }

    void SetDefaultSecondaryRotation(const Quaternion<T>& Rotation)
    {
        m_DefaultSecondaryRotation = Rotation;
    }

    void SetDefaultDistance(T Dist)
    {
        VERIFY_EXPR(Dist >= 0);
        m_DefaultDist = Dist;
    }

    void ResetDefaults()
    {
        m_Yaw   = m_DefaultYaw;
        m_Pitch = m_DefaultPitch;
        m_Dist  = m_DefaultDist;

        m_SecondaryRotation = m_DefaultSecondaryRotation;
    }

    void SetDistRange(T MinDist, T MaxDist)
    {
        VERIFY_EXPR(MinDist >= 0 && MaxDist >= 0 && MaxDist >= MinDist);
        m_MinDist = MinDist;
        m_MaxDist = MaxDist;
    }

    T GetDist() const
    {
        return m_Dist;
    }

    T GetMinDist() const
    {
        return m_MinDist;
    }

    T GetMaxDist() const
    {
        return m_MaxDist;
    }

    void SetRotationSpeed(T RotationSpeed)
    {
        m_RotationSpeed = RotationSpeed;
    }

    void SetZoomSpeed(T ZoomSpeed)
    {
        m_ZoomSpeed = ZoomSpeed;
    }

    void SetLeftHanded(bool IsLeftHanded)
    {
        m_IsLeftHanded = IsLeftHanded;
    }

    const auto& GetRotation() const
    {
        return m_PrimaryRotation;
    }

    const auto& GetSecondaryRotation() const
    {
        return m_SecondaryRotation;
    }

protected:
    MouseState m_LastMouseState;

    T m_Yaw   = 0;
    T m_Pitch = 0;
    T m_Dist  = 1;

    T m_DefaultYaw   = 0;
    T m_DefaultPitch = 0;
    T m_DefaultDist  = 1;

    T m_MinDist = 0.125;
    T m_MaxDist = 5;

    T m_RotationSpeed = static_cast<T>(0.005);
    T m_ZoomSpeed     = static_cast<T>(0.25);

    bool m_IsLeftHanded = false;

    Quaternion<T> m_PrimaryRotation{0, 0, 0, 1};
    Quaternion<T> m_SecondaryRotation{0, 0, 0, 1};
    Quaternion<T> m_DefaultSecondaryRotation{0, 0, 0, 1};
    Quaternion<T> m_ExtraRotation{0, 0, 0, 1};
};

} // namespace Diligent
