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

#include <thread>
#include <atomic>

#include "Terrain.hpp"

// Single staging texture allocates less memory, but spends more time
// than when UpdateTexture() is used with implicit staging buffer.
#define USE_STAGING_TEXTURE 0

namespace Diligent
{

class Buildings
{
public:
    Buildings();
    ~Buildings();

    void Initialize(IRenderDevice* pDevice, IBuffer* pDrawConstants, Uint64 ImmediateContextMask);
    void CreateResources(IDeviceContext* pContext);
    void CreatePSO(const ScenePSOCreateAttribs& Attr);

    void BeforeDraw(IDeviceContext* pContext, const SceneDrawAttribs& Attr);
    void Draw(IDeviceContext* pContext);
    void AfterDraw(IDeviceContext* pContext);

    void UpdateAtlas(IDeviceContext* pContext, Uint32 RequiredTransferRateMb, Uint32& ActualTransferRateMb);

    Uint32 GetOpaqueTexAtlasDataSize() const
    {
        const auto& TexDesc = m_OpaqueTexAtlas->GetDesc();
        return TexDesc.Width * TexDesc.Height * TexDesc.ArraySize * 4;
    }

private:
    void GenerateOpaqueTexture();
    void ThreadProc();

    RefCntAutoPtr<IRenderDevice> m_Device;
    Uint64                       m_ImmediateContextMask = 0;
    RefCntAutoPtr<IBuffer>       m_DrawConstants;

    // Buildings drawing (opaque)
    RefCntAutoPtr<IPipelineState>         m_DrawOpaquePSO;
    RefCntAutoPtr<IShaderResourceBinding> m_DrawOpaqueSRB;
    RefCntAutoPtr<ITexture>               m_OpaqueTexAtlas;
    RESOURCE_STATE                        m_OpaqueTexAtlasDefaultState = RESOURCE_STATE_UNKNOWN;
    RefCntAutoPtr<IBuffer>                m_OpaqueVB;
    RefCntAutoPtr<IBuffer>                m_OpaqueIB;

    // Buildings parameters
    const float m_DistributionScale      = 8.f;
    const int   m_DistributionGridSize   = 20;
    Uint32      m_m_OpaqueTexAtlasOffset = 0;


    std::vector<Uint32> m_OpaqueTexAtlasPixels;
    Uint32              m_OpaqueTexAtlasSliceSize = 0; // in bytes

    enum class TaskStatus : Uint32
    {
        NewTask  = 0,
        GenTex   = 1,
        TexReady = 2,
        CopyTex  = 3,
        Initial  = ~0u,
    };
    struct GenTexTask
    {
        std::atomic<TaskStatus> Status{TaskStatus::Initial}; // protects access to other fields
        std::vector<Uint32>     Pixels;
        Uint32                  ArraySlice = 0;
        Uint32                  Time       = 0;
    };
    GenTexTask        m_GenTexTask;
    std::thread       m_GenTexThread;
    std::atomic<bool> m_GenTexThreadLooping{false};

#if USE_STAGING_TEXTURE
    RefCntAutoPtr<ITexture> m_OpaqueTexAtlasStaging;
    RefCntAutoPtr<IFence>   m_UploadCompleteFence;
    Uint64                  m_UploadCompleteFenceValue = 0;
#endif

public:
    Uint32 CurrentTime = 0;
};

} // namespace Diligent
