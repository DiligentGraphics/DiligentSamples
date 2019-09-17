/*     Copyright 2019 Diligent Graphics LLC
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

#pragma once 

#include <atomic>
#include <vector>
#include <thread>
#include <mutex>
#include "SampleBase.h"
#include "BasicMath.h"
#include "ThreadSignal.h"

namespace Diligent
{

class Tutorial06_Multithreading final : public SampleBase
{
public:
    ~Tutorial06_Multithreading()override;
    virtual void GetEngineInitializationAttribs(DeviceType         DevType, 
                                                EngineCreateInfo&  Attribs)override final;
    virtual void Initialize(IEngineFactory*  pEngineFactory,
                            IRenderDevice*   pDevice, 
                            IDeviceContext** ppContexts, 
                            Uint32           NumDeferredCtx, 
                            ISwapChain*      pSwapChain)override final;
    virtual void Render()override final;
    virtual void Update(double CurrTime, double ElapsedTime)override final;
    virtual const Char* GetSampleName()const override final{return "Tutorial06: Multithreaded rendering";}

private:
    void CreatePipelineState (std::vector<StateTransitionDesc>& Barriers);
    void CreateVertexBuffer  (std::vector<StateTransitionDesc>& Barriers);
    void CreateIndexBuffer   (std::vector<StateTransitionDesc>& Barriers);
    void LoadTextures        (std::vector<StateTransitionDesc>& Barriers);
    void InitUI();
    void PopulateInstanceData();

    void StartWorkerThreads();
    void StopWorkerThreads();

    void RenderSubset(IDeviceContext* pCtx, Uint32 Subset);

    static void WorkerThreadFunc(Tutorial06_Multithreading* pThis, Uint32 ThreadNum);

    ThreadingTools::Signal m_RenderSubsetSignal;
    ThreadingTools::Signal m_ExecuteCommandListsSignal;
    ThreadingTools::Signal m_GotoNextFrameSignal;
    std::mutex m_NumThreadsCompletedMtx;
    std::atomic_int m_NumThreadsCompleted;
    std::atomic_int m_NumThreadsReady;
    std::vector<std::thread> m_WorkerThreads;
    std::vector< RefCntAutoPtr<ICommandList> > m_CmdLists;

    RefCntAutoPtr<IPipelineState> m_pPSO;
    RefCntAutoPtr<IBuffer>        m_CubeVertexBuffer;
    RefCntAutoPtr<IBuffer>        m_CubeIndexBuffer;
    RefCntAutoPtr<IBuffer>        m_InstanceConstants;
    RefCntAutoPtr<IBuffer>        m_VSConstants;
        
    static constexpr int NumTextures = 4;
    RefCntAutoPtr<IShaderResourceBinding> m_SRB[NumTextures];
    RefCntAutoPtr<ITextureView>           m_TextureSRV[NumTextures];
    float4x4 m_ViewProjMatrix;
    float4x4 m_RotationMatrix;
    int m_GridSize = 5;

    int m_MaxThreads = 8;
    int m_NumWorkerThreads = 4;

    struct InstanceData
    {
        float4x4 Matrix;
        int TextureInd;
    };
    std::vector<InstanceData> m_InstanceData;
};

}
