/*
 *  Copyright 2019-2022 Diligent Graphics LLC
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

#include "SampleBase.hpp"

namespace Diligent
{

struct SceneDrawAttribs
{
    float4x4 ViewProj;
    float3   LightDir;
    float    AmbientLight = 0;
};

struct ScenePSOCreateAttribs
{
    IShaderSourceInputStreamFactory* pShaderSourceFactory = nullptr;

    TEXTURE_FORMAT ColorTargetFormat = TEX_FORMAT_UNKNOWN;
    TEXTURE_FORMAT DepthTargetFormat = TEX_FORMAT_UNKNOWN;
    int            TurbulenceOctaves = 2;
    int            NoiseOctaves      = 2;
};


class Terrain
{
public:
    Terrain() noexcept :
        m_FrameId{0}
    {}

    void Initialize(IRenderDevice* pDevice, IBuffer* pDrawConstants, Uint64 ImmediateContextMask);
    void CreateResources(IDeviceContext* pContext);
    void CreatePSO(const ScenePSOCreateAttribs& Attr);

    void Update(IDeviceContext* pContext);

    void BeforeDraw(IDeviceContext* pContext, const SceneDrawAttribs& Attr);
    void Draw(IDeviceContext* pContext);
    void AfterDraw(IDeviceContext* pContext);

    void Recreate(IDeviceContext* pContext);

private:
    RefCntAutoPtr<IRenderDevice> m_Device;
    Uint64                       m_ImmediateContextMask = 0;

    RefCntAutoPtr<IBuffer> m_DrawConstants;
    RefCntAutoPtr<IBuffer> m_TerrainConstants[2]; // 0 - compute pass, 1 - graphics pass

    // Terrain drawing
    RefCntAutoPtr<IPipelineState>         m_DrawPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_DrawSRB[2];
    RefCntAutoPtr<ITexture>               m_DiffuseMap;
    RefCntAutoPtr<IBuffer>                m_VB;
    RefCntAutoPtr<IBuffer>                m_IB;

    // Terrain height and normal map generator
    RefCntAutoPtr<IPipelineState>         m_GenPSO;
    RefCntAutoPtr<IShaderResourceBinding> m_GenSRB[2];
    RefCntAutoPtr<ITexture>               m_HeightMap[2];
    RefCntAutoPtr<ITexture>               m_NormalMap[2];

    // Terrain parameters
    const float m_XZScale            = 400.0f;
    const float m_UVScale            = m_XZScale * 0.1f;
    float       m_NoiseScale         = 0.f;
    Uint32      m_ComputeGroupSize   = 0; // local group size without border
    const int   m_GroupBorderSize    = 1; // added 1 pixel border for left-top sides to calculate normals using only groupshared memory
    const float m_TerrainHeightScale = 3.0f;

    Uint32 m_FrameId : 1;

public:
    int   TerrainSize = 10; // size of mesh as power of 2
    float XOffset     = 0.f;
    float Animation   = 0.f;

    bool DoubleBuffering = false;
};

} // namespace Diligent
