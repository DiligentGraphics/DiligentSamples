/*
 *  Copyright 2019-2022 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
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

#include "GLFWDemo.hpp"

namespace Diligent
{

class Game final : public GLFWDemo
{
public:
    virtual bool Initialize() override;
    virtual void Update(float dt) override;
    virtual void Draw() override;
    virtual void KeyEvent(Key key, KeyState state) override;
    virtual void MouseEvent(float2 pos) override;

private:
    void GenerateMap();
    void CreateSDFMap();
    void CreatePipelineState();
    void InitPlayer();
    void BindResources();
    void LoadNewMap();

    void GetScreenTransform(float2& XRange, float2& YRange);

private:
    struct
    {
        float2 Pos; // pixels
        float2 FlashLightDir   = {1.0f, 0.0f};
        float  FlashLightPower = 0.0f; // 0 - off, 1 - max brightness

        // per-frame states
        float2 PendingPos;
        float2 MousePos;
        bool   LMBPressed = false;

        RefCntAutoPtr<IBuffer> pConstants;
    } m_Player;

    struct
    {
        float2                                TeleportPos; // pixels, player must reach this point to finish game
        float                                 TeleportWaveAnim = 0.0f;
        std::vector<bool>                     MapData; // 0 - empty, 1 - wall
        RefCntAutoPtr<ITexture>               pMapTex;
        RefCntAutoPtr<IPipelineState>         pPSO;
        RefCntAutoPtr<IShaderResourceBinding> pSRB;
        RefCntAutoPtr<IBuffer>                pConstants;
    } m_Map;

    struct
    {
        const float PlayerRadius          = 0.25f; // pixels, must be less than 0.5, because we use 1 bit SDF
        const float AmbientLightRadius    = 4.0f;  // pixels
        const float FlshLightMaxDist      = 25.0f; // pixels
        const float PlayerVelocity        = 4.0f;  // pixels / second
        const float FlashLightAttenuation = 4.0f;  // power / second
        const float MaxDT                 = 1.0f / 30.0f;
        const uint  MaxCollisionSteps     = 8;

        const float TeleportRadius = 1.0f; // pixels

        const uint2  MapTexDim       = {64, 64};
        const Uint32 SDFTexScale     = 2;
        const uint2  SDFTexDim       = MapTexDim * SDFTexScale;
        const int    TexFilterRadius = 8; // max distance in pixels that can be added to position during ray marching
    } Constants;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> m_pShaderSourceFactory;
    RefCntAutoPtr<IRenderStateNotationLoader>      m_pRSNLoader;
};

} // namespace Diligent
