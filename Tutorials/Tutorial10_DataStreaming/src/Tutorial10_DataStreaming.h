/*     Copyright 2015-2018 Egor Yusov
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
#include <memory>
#include "SampleBase.h"
#include "BasicMath.h"
#include "LockHelper.h"

class Tutorial10_DataStreaming : public SampleBase
{
public:
    ~Tutorial10_DataStreaming()override;
    virtual void GetEngineInitializationAttribs(Diligent::DeviceType DevType, 
                                                Diligent::EngineCreationAttribs &Attribs, 
                                                Diligent::Uint32 &NumDeferredContexts)override;
    virtual void Initialize(Diligent::IRenderDevice *pDevice, 
                            Diligent::IDeviceContext **ppContexts, 
                            Diligent::Uint32 NumDeferredCtx, 
                            Diligent::ISwapChain *pSwapChain)override;
    virtual void Render()override;
    virtual void Update(double CurrTime, double ElapsedTime)override;
    virtual const Diligent::Char* GetSampleName()const override{return "Tutorial10: Streaming";}

private:
    static void SetNumPolygons(const void *value, void * clientData);
    static void GetNumPolygons(void *value, void * clientData);
    static void SetBatchSize(const void *value, void * clientData);
    static void GetBatchSize(void *value, void * clientData);
    static void SetWorkerThreadCount(const void *value, void * clientData);
    static void GetWorkerThreadCount(void *value, void * clientData);
    void InitializePolygons();
    void InitializePolygonGeometry();
    void CreateInstanceBuffer();
    void UpdatePolygons(float elapsedTime);
    void StartWorkerThreads();
    void StopWorkerThreads();
    template<bool UseBatch>
    void RenderSubset(Diligent::IDeviceContext *pCtx, Diligent::Uint32 Subset);

    static void WorkerThreadFunc(Tutorial10_DataStreaming *pThis, Diligent::Uint32 ThreadNum);

    ThreadingTools::Signal m_RenderSubsetSignal;
    ThreadingTools::Signal m_ExecuteCommandListsSignal;
    ThreadingTools::Signal m_GotoNextFrameSignal;
    std::mutex m_NumThreadsCompletedMtx;
    std::atomic_int m_NumThreadsCompleted;
    std::atomic_int m_NumThreadsReady;
    std::vector<std::thread> m_WorkerThreads;
    std::vector< Diligent::RefCntAutoPtr<Diligent::ICommandList> > m_CmdLists;

    static constexpr const int NumStates = 5;
    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pPSO[2][NumStates];
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_PolygonAttribsCB;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_BatchDataBuffer;
    
    static constexpr const int MaxVertsInStreamingBuffer = 1024;
    std::unique_ptr<class StreamingBuffer> m_StreamingVB;
    std::unique_ptr<class StreamingBuffer> m_StreamingIB;

    static constexpr int NumTextures = 4;
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_SRB[NumTextures];
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> m_BatchSRB;
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_TextureSRV[NumTextures];
    Diligent::RefCntAutoPtr<Diligent::ITextureView> m_TexArraySRV;
    int m_NumPolygons = 1000;
    int m_BatchSize = 5;

    int m_MaxThreads = 8;
    int m_NumWorkerThreads = 4;

    struct PolygonData
    {
        float2 Pos;
        float2 MoveDir;
        float Size;
        float Angle;
        float RotSpeed;
        int TextureInd;
        int StateInd;
        int NumVerts;
    };
    std::vector<PolygonData> m_Polygons;

    struct InstanceData
    {
        float4 PolygonRotationAndScale;
        float2 PolygonCenter;
        float TexArrInd;
    };

    static constexpr const Diligent::Uint32 MinPolygonVerts = 3;
    static constexpr const Diligent::Uint32 MaxPolygonVerts = 10;
    struct PolygonGeometry
    {
        std::vector<float2> Verts;
        std::vector<Diligent::Uint32> Inds;
    };
    std::vector<PolygonGeometry> m_PolygonGeo;

    std::pair<Diligent::Uint32, Diligent::Uint32> WritePolygon(const PolygonGeometry& PolygonGeo, Diligent::IDeviceContext* pCtx, size_t CtxNum);

};
