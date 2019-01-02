/*     Copyright 2015-2019 Egor Yusov
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

#include <array>
#include <random>
#include "SampleBase.h"
#include "BasicMath.h"

class Tutorial11_ResourceUpdates : public SampleBase
{
public:
    virtual void Initialize(Diligent::IRenderDevice *pDevice, 
                            Diligent::IDeviceContext **ppContexts, 
                            Diligent::Uint32 NumDeferredCtx, 
                            Diligent::ISwapChain *pSwapChain)override;
    virtual void Render()override;
    virtual void Update(double CurrTime, double ElapsedTime)override;
    virtual const Diligent::Char* GetSampleName()const override{return "Tutorial11: Resource Updates";}

private:
    void WriteStripPattern(Diligent::Uint8*, Diligent::Uint32 Width, Diligent::Uint32 Height, Diligent::Uint32 Stride);
    void WriteDiamondPattern(Diligent::Uint8*, Diligent::Uint32 Width, Diligent::Uint32 Height, Diligent::Uint32 Stride);
    
    void UpdateTexture(Diligent::Uint32 TexIndex);
    void MapTexture(Diligent::Uint32 TexIndex, bool MapEntireTexture);
    void UpdateBuffer(Diligent::Uint32 BufferIndex);
    void MapDynamicBuffer(Diligent::Uint32 BufferIndex);

    Diligent::RefCntAutoPtr<Diligent::IPipelineState> m_pPSO, m_pPSO_NoCull;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_CubeVertexBuffer[3];
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_CubeIndexBuffer;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_VSConstants;
    Diligent::RefCntAutoPtr<Diligent::IBuffer> m_TextureUpdateBuffer;
    void DrawCube(const float4x4& WVPMatrix, Diligent::IBuffer *pVertexBuffer, Diligent::IShaderResourceBinding *pSRB);
    static constexpr const size_t NumTextures = 4;
    static constexpr const Diligent::Uint32 MaxUpdateRegionSize = 128;
    static constexpr const Diligent::Uint32 MaxMapRegionSize = 128;
    std::array<Diligent::RefCntAutoPtr<Diligent::ITexture>,               NumTextures> m_Textures;
    std::array<Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding>, NumTextures> m_SRBs;
    double m_LastTextureUpdateTime = 0;
    double m_LastBufferUpdateTime = 0;
    double m_LastMapTime = 0;
    std::mt19937 m_gen{0}; //Use 0 as the seed to always generate the same sequence
    double m_CurrTime;
};
