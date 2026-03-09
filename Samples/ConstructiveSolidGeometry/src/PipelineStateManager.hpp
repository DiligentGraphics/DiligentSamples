/*
 *  Copyright 2026 Diligent Graphics LLC
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

#include <string>
#include <string_view>
#include <vector>

#include "PipelineState.h"
#include "RefCntAutoPtr.hpp"
#include "RenderStateNotationLoader.h"
#include "DebugUtilities.hpp"

namespace Diligent
{

using PSOModifyCallback = void (*)(PipelineStateCreateInfo&, void*);

struct PSODescriptor
{
    PIPELINE_TYPE     PipelineType   = PIPELINE_TYPE_GRAPHICS;
    PSOModifyCallback ModifyCallback = nullptr;
    void*             pCallbackData  = nullptr;
    bool              LookupInCache  = true;
};

struct PSORequestResult
{
    IPipelineState*         pPSO = nullptr;
    IShaderResourceBinding* pSRB = nullptr;

    explicit operator bool() const { return pPSO != nullptr && pSRB != nullptr; }
};

class PipelineStateManager
{
public:
    explicit PipelineStateManager(IRenderStateNotationLoader* pLoader) :
        m_pLoader{pLoader}
    {
        VERIFY_EXPR(m_pLoader != nullptr);
    }

    void RegisterPSO(Uint32 Id, std::string_view Name, const PSODescriptor& Desc)
    {
        if (Id >= m_Entries.size())
            m_Entries.resize(Id + 1);

        auto& Entry      = m_Entries[Id];
        Entry.Name       = Name;
        Entry.Descriptor = Desc;
    }

    PSORequestResult RequestPSO(Uint32 Id)
    {
        VERIFY_EXPR(Id < m_Entries.size());
        auto& Entry = m_Entries[Id];

        if (!Entry.PSO)
        {
            LoadPipelineStateInfo LoadInfo;
            LoadInfo.PipelineType        = Entry.Descriptor.PipelineType;
            LoadInfo.Name                = Entry.Name.c_str();
            LoadInfo.ModifyPipeline      = Entry.Descriptor.ModifyCallback;
            LoadInfo.pModifyPipelineData = Entry.Descriptor.pCallbackData;
            LoadInfo.LookupInCache       = Entry.Descriptor.LookupInCache;
            m_pLoader->LoadPipelineState(LoadInfo, &Entry.PSO);
        }

        if (Entry.PSO && !Entry.SRB)
            Entry.PSO->CreateShaderResourceBinding(&Entry.SRB, false);

        return {Entry.PSO, Entry.SRB};
    }

    void InvalidateSRBs()
    {
        for (auto& Entry : m_Entries)
            Entry.SRB.Release();
    }

    void InvalidateAll()
    {
        for (auto& Entry : m_Entries)
        {
            Entry.PSO.Release();
            Entry.SRB.Release();
        }
    }

private:
    struct PSOEntry
    {
        std::string                           Name;
        PSODescriptor                         Descriptor;
        RefCntAutoPtr<IPipelineState>         PSO;
        RefCntAutoPtr<IShaderResourceBinding> SRB;
    };

    IRenderStateNotationLoader* m_pLoader = nullptr;
    std::vector<PSOEntry>       m_Entries;
};

} // namespace Diligent
