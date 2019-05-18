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
#include "InputController.h"

namespace Diligent
{

class FirstPersonCamera
{
public:
    void Update(InputController& Controller, float ElapsedTime);
    void SetRotation(float Yaw, float Pitch);
    void SetLookAt(const float3 &LookAt);
    void SetMoveSpeed(float MoveSpeed)         { m_fMoveSpeed = MoveSpeed; }
    void SetRotationSpeed(float RotationSpeed) { m_fRotationSpeed = RotationSpeed; }
    void SetPos(const float3 &Pos)             { m_Pos = Pos; }
    void SetProjAttribs(Float32 NearClipPlane, 
                        Float32 FarClipPlane, 
                        Float32 AspectRatio,
                        Float32 FOV,
                        bool    IsGL);
    void SetSpeedUpScales(Float32 SpeedUpScale, Float32 SuperSpeedUpScale);
    

    const float4x4& GetViewMatrix() const { return m_ViewMatrix;  }
    const float4x4& GetWorldMatrix()const { return m_WorldMatrix; }
    const float4x4& GetProjMatrix() const { return m_ProjMatrix;  }
    float3 GetWorldRight()const { return float3(m_ViewMatrix._11, m_ViewMatrix._21, m_ViewMatrix._31); }
    float3 GetWorldUp()   const { return float3(m_ViewMatrix._12, m_ViewMatrix._22, m_ViewMatrix._32); }
    float3 GetWorldAhead()const { return float3(m_ViewMatrix._13, m_ViewMatrix._23, m_ViewMatrix._33); }

    float3 GetPos() const { return m_Pos; }
    float GetCurrentSpeed() const {return m_fCurrentSpeed;}

    struct ProjectionAttribs
    {
        Float32 NearClipPlane = 1.f;
        Float32 FarClipPlane  = 1000.f;
        Float32 AspectRatio   = 1.f;
        Float32 FOV           = PI_F / 4.f;
        bool    IsGL          = false;
    };
    const ProjectionAttribs& GetProjAttribs() { return m_ProjAttribs;  }

protected:
    ProjectionAttribs m_ProjAttribs;

    MouseState m_LastMouseState;

    float3   m_Pos;
    float4x4 m_ViewMatrix;
    float4x4 m_WorldMatrix;
    float4x4 m_ProjMatrix;
    float m_fRotationSpeed = 0.01f;
    float m_fMoveSpeed     = 1.f;
    float m_fCurrentSpeed  = 0.f;

    float m_fYawAngle          = 0;    // Yaw angle of camera
    float m_fPitchAngle        = 0;    // Pitch angle of camera
    float m_fSpeedUpScale      = 1.f;
    float m_fSuperSpeedUpScale = 1.f;
};

}
