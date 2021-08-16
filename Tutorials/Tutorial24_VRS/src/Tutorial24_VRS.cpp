/*
 *  Copyright 2019-2021 Diligent Graphics LLC
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

#include "Tutorial24_VRS.hpp"
#include "Align.hpp"
#include "MapHelper.hpp"
#include "TextureUtilities.h"
#include "../../Common/src/TexturedCube.hpp"
#include "imgui.h"
#include "ImGuiUtils.hpp"

namespace Diligent
{
namespace HLSL
{
#include "../assets/Structures.fxh"
}

SampleBase* CreateSample()
{
    return new Tutorial24_VRS();
}

void Tutorial24_VRS::CreateVRSPipelineState()
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    auto& PSODesc          = PSOCreateInfo.PSODesc;
    auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

    GraphicsPipeline.NumRenderTargets                     = 1;
    GraphicsPipeline.RTVFormats[0]                        = ColorFormat;
    GraphicsPipeline.DSVFormat                            = DepthFormat;
    GraphicsPipeline.PrimitiveTopology                    = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_BACK;
    GraphicsPipeline.RasterizerDesc.FillMode              = FILL_MODE_SOLID;
    GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = False;
    GraphicsPipeline.DepthStencilDesc.DepthEnable         = True;

    PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.ShaderCompiler             = SHADER_COMPILER_DXC;
    ShaderCI.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "VRS - VS";
        ShaderCI.FilePath        = "CubeVRS.vsh";

        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "VRS - PS";
        ShaderCI.FilePath        = "CubeVRS.psh";

        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    const LayoutElement LayoutElems[] = {
        {0, 0, 3, VT_FLOAT32, False},
        {1, 0, 2, VT_FLOAT32, False} //
    };
    GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

    const SamplerDesc SamLinearClampDesc{
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP //
    };
    const ImmutableSamplerDesc ImtblSamplers[] = {{SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}};

    PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
    PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    PSODesc.Name                      = "Per primitive shading rate";
    GraphicsPipeline.ShadingRateFlags = PIPELINE_SHADING_RATE_FLAG_PER_PRIMITIVE;
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_VRS.PSO[VRS_MODE_PER_DRAW]);

    m_VRS.PSO[VRS_MODE_PER_PRIMITIVE] = m_VRS.PSO[VRS_MODE_PER_DRAW];

    PSODesc.Name                      = "Texture based shading rate";
    GraphicsPipeline.ShadingRateFlags = PIPELINE_SHADING_RATE_FLAG_TEXTURE_BASED;
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_VRS.PSO[2]);

    m_VRS.PSO[0]->CreateShaderResourceBinding(&m_VRS.SRB);
}

void Tutorial24_VRS::CreateDensityMapPipelineState()
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    auto& PSODesc          = PSOCreateInfo.PSODesc;
    auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

    GraphicsPipeline.NumRenderTargets                     = 1;
    GraphicsPipeline.RTVFormats[0]                        = ColorFormat;
    GraphicsPipeline.DSVFormat                            = DepthFormat;
    GraphicsPipeline.PrimitiveTopology                    = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_BACK;
    GraphicsPipeline.RasterizerDesc.FillMode              = FILL_MODE_SOLID;
    GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = False;
    GraphicsPipeline.DepthStencilDesc.DepthEnable         = True;

    PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM;
    ShaderCI.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "VRS - VS";
        ShaderCI.FilePath        = "CubeFDM_vs.glsl";

        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "VRS - PS";
        ShaderCI.FilePath        = "CubeFDM_fs.glsl";

        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    const LayoutElement LayoutElems[] = {
        {0, 0, 3, VT_FLOAT32, False},
        {1, 0, 2, VT_FLOAT32, False} //
    };
    GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

    const SamplerDesc SamLinearClampDesc{
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP //
    };
    const ImmutableSamplerDesc ImtblSamplers[] = {{SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}};

    PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
    PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    PSODesc.Name                      = "Texture based shading rate";
    GraphicsPipeline.ShadingRateFlags = PIPELINE_SHADING_RATE_FLAG_TEXTURE_BASED;
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_VRS.PSO[2]);

    m_VRS.PSO[2]->CreateShaderResourceBinding(&m_VRS.SRB);
}

void Tutorial24_VRS::CreateBlitPipelineState()
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    auto& PSODesc          = PSOCreateInfo.PSODesc;
    auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

    PSODesc.Name = "Per primitive shading rate";

    GraphicsPipeline.NumRenderTargets                     = 1;
    GraphicsPipeline.RTVFormats[0]                        = m_pSwapChain->GetDesc().ColorBufferFormat;
    GraphicsPipeline.PrimitiveTopology                    = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_BACK;
    GraphicsPipeline.RasterizerDesc.FillMode              = FILL_MODE_SOLID;
    GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = False;
    GraphicsPipeline.DepthStencilDesc.DepthEnable         = False;

    PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage             = SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.UseCombinedTextureSamplers = true;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Blit - VS";
        ShaderCI.FilePath        = "ImageBlit.vsh";

        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Blit - PS";
        ShaderCI.FilePath        = "ImageBlit.psh";

        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_BlitPSO);
}

void Tutorial24_VRS::LoadTexture()
{
    TextureLoadInfo loadInfo;
    loadInfo.IsSRGB = true;
    RefCntAutoPtr<ITexture> Tex;
    CreateTextureFromFile("DGLogo.png", loadInfo, m_pDevice, &Tex);
    // Get shader resource view from the texture
    m_TextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    // Set texture SRV in the SRB
    m_VRS.SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);
}

void Tutorial24_VRS::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

#if PLATFORM_ANDROID
    CreateDensityMapPipelineState();
#else
    CreateVRSPipelineState();
#endif
    CreateBlitPipelineState();

    {
        BufferDesc BuffDesc;
        BuffDesc.Name           = "Constants";
        BuffDesc.uiSizeInBytes  = sizeof(HLSL::Constants);
        BuffDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        BuffDesc.Usage          = USAGE_DYNAMIC;
        BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

        m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_Constants);

        m_VRS.SRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_Constants")->Set(m_Constants);
        m_VRS.SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Constants")->Set(m_Constants);
    }

    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_pDevice, TexturedCube::VERTEX_COMPONENT_FLAG_POS_UV);
    m_CubeIndexBuffer  = TexturedCube::CreateIndexBuffer(m_pDevice);
    LoadTexture();

    const auto& SRProps = m_pDevice->GetAdapterInfo().ShadingRate;

    if (SRProps.CapFlags & SHADING_RATE_CAP_FLAG_PER_DRAW)
    {
        m_VRSModeNames[m_NumVRSModes] = "Per draw";
        m_VRSModeList[m_NumVRSModes]  = VRS_MODE_PER_DRAW;
        ++m_NumVRSModes;
    }

    if (SRProps.CapFlags & SHADING_RATE_CAP_FLAG_PER_PRIMITIVE)
    {
        m_VRSModeNames[m_NumVRSModes] = "Per primitive";
        m_VRSModeList[m_NumVRSModes]  = VRS_MODE_PER_PRIMITIVE;
        ++m_NumVRSModes;
    }

    if (SRProps.CapFlags & SHADING_RATE_CAP_FLAG_TEXTURE_BASED)
    {
        m_VRSModeNames[m_NumVRSModes] = "Texture based";
        m_VRSModeList[m_NumVRSModes]  = VRS_MODE_TEXTURE_BASED;
        ++m_NumVRSModes;
    }

    static const char* ShadingRateNames[] =
        {
            "1x1", // SHADING_RATE_1X1
            "1x2", // SHADING_RATE_1X2
            "1x4", // SHADING_RATE_1X4
            "",
            "2x1", // SHADING_RATE_2X1
            "2x2", // SHADING_RATE_2X2
            "2x4", // SHADING_RATE_2X4
            "",
            "4x1", // SHADING_RATE_4X1
            "4x2", // SHADING_RATE_4X2
            "4x4"  // SHADING_RATE_4X4
        };
    static_assert(_countof(ShadingRateNames) == SHADING_RATE_MAX + 1, "array size mismatch");

    for (Uint32 i = 0; i < SRProps.NumShadingRates; ++i)
    {
        auto SR                               = SRProps.ShadingRates[i].Rate;
        m_ShadingRateNames[m_NumShadingRates] = ShadingRateNames[SR];
        m_ShadingRates[m_NumShadingRates]     = SR;
        ++m_NumShadingRates;
    }
}

void Tutorial24_VRS::ModifyEngineInitInfo(const ModifyEngineInitInfoAttribs& Attribs)
{
    SampleBase::ModifyEngineInitInfo(Attribs);

    Attribs.EngineCI.Features.VariableRateShading = DEVICE_FEATURE_STATE_ENABLED;
}

void Tutorial24_VRS::Render()
{
    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<HLSL::Constants> CBConstants(m_pImmediateContext, m_Constants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants->WorldViewProj        = m_WorldViewProjMatrix.Transpose();
        CBConstants->PrimitiveShadingRate = m_ShadingRates[m_ShadingRateIndex];
        CBConstants->DrawMode             = m_ShowShadingRate ? 1 : 0;
    }

    // Draw to scaled surface
    {
        ITextureView* pRTVs[] = {m_pRTV};
        m_pImmediateContext->SetRenderTargets(1, pRTVs, m_pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        const float ClearColor[] = {0.4f, 0.4f, 0.4f, 1.f};
        m_pImmediateContext->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
        m_pImmediateContext->ClearDepthStencil(m_pDSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

        const auto Mode = m_VRSModeList[m_VRSMode];
        switch (Mode)
        {
            case VRS_MODE_PER_DRAW:
                m_pImmediateContext->SetShadingRate(m_ShadingRates[m_ShadingRateIndex], SHADING_RATE_COMBINER_PASSTHROUGH, SHADING_RATE_COMBINER_PASSTHROUGH);
                break;
            case VRS_MODE_PER_PRIMITIVE:
                m_pImmediateContext->SetShadingRate(SHADING_RATE_1X1, SHADING_RATE_COMBINER_OVERRIDE, SHADING_RATE_COMBINER_PASSTHROUGH);
                break;
            case VRS_MODE_TEXTURE_BASED:
                m_pImmediateContext->SetShadingRate(SHADING_RATE_1X1, SHADING_RATE_COMBINER_PASSTHROUGH, SHADING_RATE_COMBINER_OVERRIDE);
                m_pImmediateContext->SetShadingRateTexture(m_VRS.ShadingRateView, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
                break;
        }

        m_pImmediateContext->SetPipelineState(m_VRS.PSO[Mode]);
        m_pImmediateContext->CommitShaderResources(m_VRS.SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        Uint32   offsets[] = {0};
        IBuffer* pBuffs[]  = {m_CubeVertexBuffer};
        m_pImmediateContext->SetVertexBuffers(0, _countof(pBuffs), pBuffs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawIndexedAttribs DrawAttrs;
        DrawAttrs.IndexType  = VT_UINT32;
        DrawAttrs.NumIndices = 36;
        DrawAttrs.Flags      = DRAW_FLAG_VERIFY_ALL;
        m_pImmediateContext->DrawIndexed(DrawAttrs);
    }

    // Blit to swapchain
    {
        ITextureView* pRTVs[] = {m_pSwapChain->GetCurrentBackBufferRTV()};
        m_pImmediateContext->SetRenderTargets(1, pRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_pImmediateContext->SetPipelineState(m_BlitPSO);
        m_pImmediateContext->CommitShaderResources(m_BlitSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawAttribs drawAttrs{3, DRAW_FLAG_VERIFY_ALL};
        m_pImmediateContext->Draw(drawAttrs);
    }
}

void Tutorial24_VRS::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    const auto& MState = m_InputController.GetMouseState();
    if (MState.ButtonFlags & MouseState::BUTTON_FLAG_LEFT)
    {
        UpdateVRSTexture(MState.PosX, MState.PosY);
    }

    if (m_Animation)
    {
        // Apply rotation
        float4x4 CubeModelTransform = float4x4::RotationY(static_cast<float>(CurrTime) * 1.0f) * float4x4::RotationX(-PI_F * 0.1f);

        // Camera is at (0, 0, -5) looking along the Z axis
        float4x4 View = float4x4::Translation(0.f, 0.0f, 4.f);

        // Get pretransform matrix that rotates the scene according the surface orientation
        auto SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

        // Get projection matrix adjusted to the current screen orientation
        auto Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

        // Compute world-view-projection matrix
        m_WorldViewProjMatrix = CubeModelTransform * View * SrfPreTransform * Proj;
    }
}

void Tutorial24_VRS::WindowResize(Uint32 Width, Uint32 Height)
{
    if (Width == 0 || Height == 0)
        return;

    const auto  OriginW = Width;
    const auto  OriginH = Height;
    const auto& SRProps = m_pDevice->GetAdapterInfo().ShadingRate;

    // Scale surface
    Width  = AlignUp(ScaleSurface(Width), SRProps.MaxTileSize[0]);
    Height = AlignUp(ScaleSurface(Height), SRProps.MaxTileSize[1]);

    // Check if the image needs to be recreated.
    if (m_pRTV != nullptr &&
        m_pRTV->GetTexture()->GetDesc().Width == Width &&
        m_pRTV->GetTexture()->GetDesc().Height == Height)
        return;

    m_VRS.ShadingRateView = nullptr;
    m_pRTV                = nullptr;
    m_pDSV                = nullptr;

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


    TexDesc.Name      = "Shading rate texture";
    TexDesc.Type      = RESOURCE_DIM_TEX_2D;
    TexDesc.Width     = Width / SRProps.MaxTileSize[0];
    TexDesc.Height    = Height / SRProps.MaxTileSize[1];
    TexDesc.BindFlags = BIND_SHADING_RATE;

    switch (SRProps.Format)
    {
        case SHADING_RATE_FORMAT_PALETTE: TexDesc.Format = TEX_FORMAT_R8_UINT; break;
        case SHADING_RATE_FORMAT_UNORM8: TexDesc.Format = TEX_FORMAT_RG8_UNORM; break;
        default: UNEXPECTED("Unexpected shading rate texture format");
    }

    RefCntAutoPtr<ITexture> pSRTex;
    m_pDevice->CreateTexture(TexDesc, nullptr, &pSRTex);
    m_VRS.ShadingRateView = pSRTex->GetDefaultView(TEXTURE_VIEW_SHADING_RATE);

    UpdateVRSTexture(OriginW * 0.5f, OriginH * 0.5f);

    m_BlitSRB = nullptr;
    m_BlitPSO->CreateShaderResourceBinding(&m_BlitSRB);
    m_BlitSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(pRT->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
}

void Tutorial24_VRS::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (m_NumVRSModes > 0)
            ImGui::Combo("VRS mode", &m_VRSMode, m_VRSModeNames, m_NumVRSModes);

        if (m_NumShadingRates > 0 && m_VRSModeList[m_VRSMode] != VRS_MODE_TEXTURE_BASED)
            ImGui::Combo("Default shading rate", &m_ShadingRateIndex, m_ShadingRateNames, m_NumShadingRates);
        else
            ImGui::Text("Click to any point at screen to change shading rate");

        ImGui::Checkbox("Show shading rate", &m_ShowShadingRate);
        ImGui::Checkbox("Animation", &m_Animation);

        const char* SurfaceScaleStr[] = {"1/4", "1/2", "1", "2", "4"};
        const int   OldSurfaceScale   = m_SurfaceScalePOT;
        ImGui::TextDisabled("Surface scale");
        ImGui::SliderInt("##SurfaceScale", &m_SurfaceScalePOT, -2, 2, SurfaceScaleStr[m_SurfaceScalePOT + 2]);

        // Recreate render targets
        if (OldSurfaceScale != m_SurfaceScalePOT)
        {
            const auto& SCDesc = m_pSwapChain->GetDesc();
            WindowResize(SCDesc.Width, SCDesc.Height);
        }
    }
    ImGui::End();
}

void Tutorial24_VRS::UpdateVRSTexture(float MPosX, float MPosY)
{
    if (m_VRS.ShadingRateView == nullptr)
        return;

    const auto& SRProps = m_pDevice->GetAdapterInfo().ShadingRate;
    auto*       pVRSTex = m_VRS.ShadingRateView->GetTexture();
    const auto& Desc    = pVRSTex->GetDesc();
    const auto& SCDesc  = m_pSwapChain->GetDesc();
    const auto  MPos    = float2{MPosX / SCDesc.Width, MPosY / SCDesc.Height};

    SHADING_RATE RemapShadingRate[SHADING_RATE_MAX + 1] = {};
    for (Uint32 i = 0; i < _countof(RemapShadingRate); ++i)
    {
        // ShadingRates is sorted from higher to lower rate.
        for (Uint32 j = 0; j < SRProps.NumShadingRates; ++j)
        {
            if (static_cast<SHADING_RATE>(i) >= SRProps.ShadingRates[j].Rate)
            {
                RemapShadingRate[i] = SRProps.ShadingRates[j].Rate;
                break;
            }
        }
    }

    std::vector<Uint8> SRData;
    switch (SRProps.Format)
    {
        case SHADING_RATE_FORMAT_PALETTE:
        {
            SRData.resize(Desc.Width * Desc.Height);
            for (Uint32 y = 0; y < Desc.Height; ++y)
            {
                for (Uint32 x = 0; x < Desc.Width; ++x)
                {
                    auto XDist = std::abs(static_cast<float>(x) / Desc.Width - MPos.x) * 1.5f;
                    auto YDist = std::abs(static_cast<float>(y) / Desc.Height - MPos.y) * 1.5f;

                    auto XRate = clamp(static_cast<Uint32>(XDist * AXIS_SHADING_RATE_MAX + 0.5f), 0u, Uint32{AXIS_SHADING_RATE_MAX});
                    auto YRate = clamp(static_cast<Uint32>(YDist * AXIS_SHADING_RATE_MAX + 0.5f), 0u, Uint32{AXIS_SHADING_RATE_MAX});
                    VERIFY_EXPR(XRate <= AXIS_SHADING_RATE_MAX);
                    VERIFY_EXPR(YRate <= AXIS_SHADING_RATE_MAX);

                    SRData[x + y * Desc.Width] = RemapShadingRate[(XRate << SHADING_RATE_X_SHIFT) | YRate];
                }
            }
            break;
        }
        case SHADING_RATE_FORMAT_UNORM8:
        {
            SRData.resize(Desc.Width * Desc.Height * 2);
            for (Uint32 y = 0; y < Desc.Height; ++y)
            {
                for (Uint32 x = 0; x < Desc.Width; ++x)
                {
                    auto XDist = clamp(std::abs(static_cast<float>(x) / Desc.Width - MPos.x), 0.f, 1.0f);
                    auto YDist = clamp(std::abs(static_cast<float>(y) / Desc.Height - MPos.y), 0.f, 1.0f);
                    auto Idx   = (x + y * Desc.Width) * 2;

                    SRData[Idx + 0] = static_cast<Uint8>(XDist * 255.f);
                    SRData[Idx + 1] = static_cast<Uint8>(YDist * 255.f);
                }
            }
            break;
        }
        default:
            UNEXPECTED("Unexpected shading rate texture format");
    }


    const Box         TexBox{0, Desc.Width, 0, Desc.Height};
    TextureSubResData SubResData;
    SubResData.pData  = SRData.data();
    SubResData.Stride = static_cast<Uint32>(SRData.size() / Desc.Height);

    m_pImmediateContext->UpdateTexture(pVRSTex, 0, 0, TexBox, SubResData, RESOURCE_STATE_TRANSITION_MODE_NONE, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}

} // namespace Diligent
