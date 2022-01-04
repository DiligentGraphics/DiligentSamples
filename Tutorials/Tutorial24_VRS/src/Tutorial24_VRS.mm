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

#include <Metal/Metal.h>
#include "RasterizationRateMapMtl.h"
#include "RenderDeviceMtl.h"
#include "Tutorial24_VRS.hpp"

namespace Diligent
{

void Tutorial24_VRS::WindowResize(Uint32 Width, Uint32 Height)
{
    if (Width == 0 || Height == 0)
        return;

    UpdateVRSPattern(m_PrevNormMPos);
}

void Tutorial24_VRS::UpdateVRSPattern(const float2 MPos)
{
    m_PrevNormMPos = MPos;

    const auto& SCDesc = m_pSwapChain->GetDesc();

    // Scale surface
    auto Width  = ScaleSurface(SCDesc.Width);
    auto Height = ScaleSurface(SCDesc.Height);

    RasterizationRateMapCreateInfo RasterRateMapCI;
    RasterRateMapCI.Desc.ScreenWidth  = Width;
    RasterRateMapCI.Desc.ScreenHeight = Height;
    RasterRateMapCI.Desc.LayerCount   = 1;
    
    const Uint32       TileSize = 16;
    std::vector<float> Horizontal(Width / TileSize);
    std::vector<float> Vertical(Height / TileSize);

    for (size_t i = 0; i < Horizontal.size(); ++i)
    {
        Horizontal[i] = clamp(1.f - std::abs((static_cast<float>(i) + 0.5f) / Horizontal.size() - MPos.x) * 2.f, 0.125f, 1.f);
    }
    for (size_t i = 0; i < Vertical.size(); ++i)
    {
        Vertical[i] = clamp(1.f - std::abs((static_cast<float>(i) + 0.5f) / Vertical.size() - MPos.y) * 2.f, 0.125f, 1.f);
    }

    RasterizationRateLayerDesc Layer;
    Layer.HorizontalCount   = static_cast<Uint32>(Horizontal.size());
    Layer.VerticalCount     = static_cast<Uint32>(Vertical.size());
    Layer.pHorizontal       = Horizontal.data();
    Layer.pVertical         = Vertical.data();
    RasterRateMapCI.pLayers = &Layer;

    RefCntAutoPtr<IRenderDeviceMtl>         pDeviceMtl{m_pDevice, IID_RenderDeviceMtl};
    RefCntAutoPtr<IRasterizationRateMapMtl> pRasterRateMap;
    pDeviceMtl->CreateRasterizationRateMap(RasterRateMapCI, &pRasterRateMap);
    if (!pRasterRateMap)
        return;

    m_pShadingRateMap = pRasterRateMap->GetView();

    Uint64 BufferSize  = 0;
    Uint32 BufferAlign = 0;
    pRasterRateMap->GetParameterBufferSizeAndAlign(BufferSize, BufferAlign);

    if (m_pShadingRateParamBuffer == nullptr ||
        BufferSize > m_pShadingRateParamBuffer->GetDesc().Size)
    {
        m_pShadingRateParamBuffer = nullptr;

        BufferDesc BuffDesc;
        BuffDesc.Name           = "RRM parameters buffer";
        BuffDesc.Size           = BufferSize;
        BuffDesc.Usage          = USAGE_UNIFIED; // buffer is used for host access and will be accessed in shader
        BuffDesc.BindFlags      = BIND_UNIFORM_BUFFER; // only uniform buffer is compatible with unified memory
        BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

        m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_pShadingRateParamBuffer);
    }
    pRasterRateMap->CopyParameterDataToBuffer(m_pShadingRateParamBuffer, 0);

    pRasterRateMap->GetPhysicalSizeForLayer(0, Width, Height);

    // Check if the image needs to be recreated.
    if (m_pRTV != nullptr &&
        m_pRTV->GetTexture()->GetDesc().Width == Width &&
        m_pRTV->GetTexture()->GetDesc().Height == Height)
        return;

    m_pRTV = nullptr;
    m_pDSV = nullptr;

    TextureDesc TexDesc;
    TexDesc.Name      = "Render target";
    TexDesc.Type      = RESOURCE_DIM_TEX_2D;
    TexDesc.Width     = Width;
    TexDesc.Height    = Height;
    TexDesc.Format    = ColorFormat;
    TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;

    RefCntAutoPtr<ITexture> pRT;
    m_pDevice->CreateTexture(TexDesc, nullptr, &pRT);
    m_pRTV = pRT->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET);


    TexDesc.Name      = "Depth target";
    TexDesc.Format    = DepthFormat;
    TexDesc.BindFlags = BIND_DEPTH_STENCIL;

    RefCntAutoPtr<ITexture> pDS;
    m_pDevice->CreateTexture(TexDesc, nullptr, &pDS);
    m_pDSV = pDS->GetDefaultView(TEXTURE_VIEW_DEPTH_STENCIL);

    m_BlitSRB = nullptr;
    m_BlitPSO->CreateShaderResourceBinding(&m_BlitSRB);
    m_BlitSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(pRT->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    m_BlitSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_RRMData")->Set(m_pShadingRateParamBuffer);
    m_BlitSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Constants")->Set(m_Constants);
}

} // namespace Diligent
