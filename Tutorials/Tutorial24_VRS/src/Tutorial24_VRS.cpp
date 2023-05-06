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

#include "Tutorial24_VRS.hpp"

#include <utility>

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
static_assert(sizeof(Constants) % 16 == 0, "must be aligned to 16 bytes");
} // namespace HLSL

SampleBase* CreateSample()
{
    return new Tutorial24_VRS();
}

void Tutorial24_VRS::CreateVRSPipelineState(IShaderSourceInputStreamFactory* pShaderSourceFactory)
{
    const bool IsMetal = m_pDevice->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_METAL;

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
    ShaderCI.ShaderCompiler             = IsMetal ? SHADER_COMPILER_DEFAULT : SHADER_COMPILER_DXC;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc       = {"VRS - VS", SHADER_TYPE_VERTEX, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "CubeVRS.vsh";

        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc       = {"VRS - PS", SHADER_TYPE_PIXEL, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "CubeVRS.psh";

        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    constexpr LayoutElement LayoutElems[] = {
        {0, 0, 3, VT_FLOAT32, False},
        {1, 0, 2, VT_FLOAT32, False} //
    };
    GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

    constexpr SamplerDesc SamLinearClampDesc{
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP //
    };
    constexpr ImmutableSamplerDesc ImtblSamplers[] = {{SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}};

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
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_VRS.PSO[VRS_MODE_TEXTURE_BASED]);

    m_VRS.PSO[VRS_MODE_PER_DRAW]->CreateShaderResourceBinding(&m_VRS.SRB);
}

void Tutorial24_VRS::CreateDensityMapPipelineState(IShaderSourceInputStreamFactory* pShaderSourceFactory)
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
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc       = {"FDM - VS", SHADER_TYPE_VERTEX, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "CubeFDM_vs.glsl";

        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc       = {"FDM - PS", SHADER_TYPE_PIXEL, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "CubeFDM_fs.glsl";

        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    constexpr LayoutElement LayoutElems[] = {
        {0, 0, 3, VT_FLOAT32, False},
        {1, 0, 2, VT_FLOAT32, False} //
    };
    GraphicsPipeline.InputLayout.LayoutElements = LayoutElems;
    GraphicsPipeline.InputLayout.NumElements    = _countof(LayoutElems);

    constexpr SamplerDesc SamLinearClampDesc{
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR,
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP //
    };
    constexpr ImmutableSamplerDesc ImtblSamplers[] = {{SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}};

    PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
    PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    PSODesc.Name                      = "Texture based shading rate";
    GraphicsPipeline.ShadingRateFlags = PIPELINE_SHADING_RATE_FLAG_TEXTURE_BASED;
    m_pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &m_VRS.PSO[VRS_MODE_TEXTURE_BASED]);

    m_VRS.PSO[VRS_MODE_TEXTURE_BASED]->CreateShaderResourceBinding(&m_VRS.SRB);
}

void Tutorial24_VRS::CreateBlitPipelineState(IShaderSourceInputStreamFactory* pShaderSourceFactory)
{
    const bool IsMetal = m_pDevice->GetDeviceInfo().Type == RENDER_DEVICE_TYPE_METAL;

    GraphicsPipelineStateCreateInfo PSOCreateInfo;

    auto& PSODesc          = PSOCreateInfo.PSODesc;
    auto& GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

    PSODesc.Name = "Blit to swapchain image";

    GraphicsPipeline.NumRenderTargets                     = 1;
    GraphicsPipeline.RTVFormats[0]                        = m_pSwapChain->GetDesc().ColorBufferFormat;
    GraphicsPipeline.PrimitiveTopology                    = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    GraphicsPipeline.RasterizerDesc.CullMode              = CULL_MODE_BACK;
    GraphicsPipeline.RasterizerDesc.FillMode              = FILL_MODE_SOLID;
    GraphicsPipeline.RasterizerDesc.FrontCounterClockwise = False;
    GraphicsPipeline.DepthStencilDesc.DepthEnable         = False;

    PSODesc.ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;

    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage             = IsMetal ? SHADER_SOURCE_LANGUAGE_MSL : SHADER_SOURCE_LANGUAGE_HLSL;
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc       = {"Blit - VS", SHADER_TYPE_VERTEX, true};
        ShaderCI.EntryPoint = "VSmain";
        ShaderCI.FilePath   = IsMetal ? "ImageBlit.msl" : "ImageBlit.vsh";

        m_pDevice->CreateShader(ShaderCI, &pVS);
    }

    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc       = {"Blit - PS", SHADER_TYPE_PIXEL, true};
        ShaderCI.EntryPoint = "PSmain";
        ShaderCI.FilePath   = IsMetal ? "ImageBlit.msl" : "ImageBlit.psh";

        m_pDevice->CreateShader(ShaderCI, &pPS);
    }

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    SamplerDesc SamLinearClampDesc{
        FILTER_TYPE_POINT, FILTER_TYPE_POINT, FILTER_TYPE_POINT,
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP //
    };

    // Only immutable sampler can be used to sample subsampled texture.
    const auto& SRProps = m_pDevice->GetAdapterInfo().ShadingRate;
    if ((SRProps.CapFlags & SHADING_RATE_CAP_FLAG_SUBSAMPLED_RENDER_TARGET) != 0)
    {
        SamLinearClampDesc.Flags  = SAMPLER_FLAG_SUBSAMPLED;
        SamLinearClampDesc.MinLOD = SamLinearClampDesc.MaxLOD = 0.0f;
    }
    const ImmutableSamplerDesc ImtblSamplers[] = {{SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}};

    PSODesc.ResourceLayout.ImmutableSamplers    = ImtblSamplers;
    PSODesc.ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

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

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);

    const auto& SRProps = m_pDevice->GetAdapterInfo().ShadingRate;
    if (SRProps.Format == SHADING_RATE_FORMAT_UNORM8)
        CreateDensityMapPipelineState(pShaderSourceFactory);
    else
        CreateVRSPipelineState(pShaderSourceFactory);

    CreateBlitPipelineState(pShaderSourceFactory);

    {
        BufferDesc BuffDesc;
        BuffDesc.Name           = "Constants";
        BuffDesc.Size           = sizeof(HLSL::Constants);
        BuffDesc.BindFlags      = BIND_UNIFORM_BUFFER;
        BuffDesc.Usage          = USAGE_DYNAMIC;
        BuffDesc.CPUAccessFlags = CPU_ACCESS_WRITE;

        m_pDevice->CreateBuffer(BuffDesc, nullptr, &m_Constants);

        m_VRS.SRB->GetVariableByName(SHADER_TYPE_VERTEX, "g_Constants")->Set(m_Constants);
        if (auto* pVar = m_VRS.SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Constants"))
            pVar->Set(m_Constants);
    }

    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_pDevice, TexturedCube::VERTEX_COMPONENT_FLAG_POS_UV);
    m_CubeIndexBuffer  = TexturedCube::CreateIndexBuffer(m_pDevice);
    LoadTexture();

    if (SRProps.CapFlags & SHADING_RATE_CAP_FLAG_PER_DRAW)
    {
        m_VRSModes.emplace_back(VRS_MODE_PER_DRAW, "Per draw");
    }

    if (SRProps.CapFlags & SHADING_RATE_CAP_FLAG_PER_PRIMITIVE)
    {
        m_VRSModes.emplace_back(VRS_MODE_PER_PRIMITIVE, "Per primitive");
    }

    if (SRProps.CapFlags & SHADING_RATE_CAP_FLAG_TEXTURE_BASED)
    {
        m_VRSModes.emplace_back(VRS_MODE_TEXTURE_BASED, "Texture based");
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
        auto SR = SRProps.ShadingRates[i].Rate;
        m_ShadingRates.emplace_back(SR, ShadingRateNames[SR]);
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
        MapHelper<HLSL::Constants> CBConstants{m_pImmediateContext, m_Constants, MAP_WRITE, MAP_FLAG_DISCARD};
        CBConstants->WorldViewProj        = m_WorldViewProjMatrix.Transpose();
        CBConstants->PrimitiveShadingRate = m_ShadingRate;
        CBConstants->DrawMode             = m_ShowShadingRate ? 1 : 0;
        CBConstants->SurfaceScale         = GetSurfaceScale();
    }

    // Draw to the scaled surface
    {
        ITextureView*           pRTVs[] = {m_pRTV};
        SetRenderTargetsAttribs RTAttrs;
        RTAttrs.NumRenderTargets    = 1;
        RTAttrs.ppRenderTargets     = pRTVs;
        RTAttrs.pDepthStencil       = m_pDSV;
        RTAttrs.StateTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;

        switch (m_VRSMode)
        {
            case VRS_MODE_PER_DRAW:
                m_pImmediateContext->SetShadingRate(m_ShadingRate, SHADING_RATE_COMBINER_PASSTHROUGH, SHADING_RATE_COMBINER_PASSTHROUGH);
                break;
            case VRS_MODE_PER_PRIMITIVE:
                m_pImmediateContext->SetShadingRate(SHADING_RATE_1X1, SHADING_RATE_COMBINER_OVERRIDE, SHADING_RATE_COMBINER_PASSTHROUGH);
                break;
            case VRS_MODE_TEXTURE_BASED:
                m_pImmediateContext->SetShadingRate(SHADING_RATE_1X1, SHADING_RATE_COMBINER_PASSTHROUGH, SHADING_RATE_COMBINER_OVERRIDE);
                RTAttrs.pShadingRateMap = m_pShadingRateMap;
                break;
            default:
                UNEXPECTED("Unexpected VRS mode");
        }

        m_pImmediateContext->SetRenderTargetsExt(RTAttrs);

        constexpr float ClearColor[] = {0.4f, 0.4f, 0.4f, 1.f};
        m_pImmediateContext->ClearRenderTarget(pRTVs[0], ClearColor, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
        m_pImmediateContext->ClearDepthStencil(m_pDSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

        m_pImmediateContext->SetPipelineState(m_VRS.PSO[m_VRSMode]);
        m_pImmediateContext->CommitShaderResources(m_VRS.SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        IBuffer* pBuffs[] = {m_CubeVertexBuffer};
        m_pImmediateContext->SetVertexBuffers(0, _countof(pBuffs), pBuffs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
        m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawIndexedAttribs DrawAttrs;
        DrawAttrs.IndexType  = VT_UINT32;
        DrawAttrs.NumIndices = 36;
        DrawAttrs.Flags      = DRAW_FLAG_VERIFY_ALL;
        m_pImmediateContext->DrawIndexed(DrawAttrs);
    }

    // Blit or resolve to swapchain
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
    if (m_VRSMode == VRS_MODE_TEXTURE_BASED && (MState.ButtonFlags & MouseState::BUTTON_FLAG_LEFT) != 0)
    {
        const auto& SCDesc = m_pSwapChain->GetDesc();
        const auto  Width  = SCDesc.Width;
        const auto  Height = SCDesc.Height;

        float2 NewMPos{MState.PosX, MState.PosY};
        switch (SCDesc.PreTransform)
        {
            case SURFACE_TRANSFORM_IDENTITY:
                break;

            case SURFACE_TRANSFORM_ROTATE_90:
                std::swap(NewMPos.x, NewMPos.y);
                NewMPos.x = Width - NewMPos.x;
                break;

            case SURFACE_TRANSFORM_ROTATE_180:
                NewMPos.x = Width - NewMPos.x;
                NewMPos.y = Height - NewMPos.y;
                break;

            case SURFACE_TRANSFORM_ROTATE_270:
                std::swap(NewMPos.x, NewMPos.y);
                NewMPos.y = Height - NewMPos.y;
                break;

            default:
                UNSUPPORTED("Unsupported surface transform");
        }

        NewMPos = (NewMPos + float2{0.5f}) / uint2{Width, Height}.Recast<float>();

        if (m_PrevNormMPos != NewMPos)
            UpdateVRSPattern(NewMPos);
    }

    if (m_Animation)
        m_fCurrentTime += static_cast<float>(ElapsedTime);

    // Apply rotation
    float4x4 CubeModelTransform = float4x4::RotationY(m_fCurrentTime * 1.0f) * float4x4::RotationX(-PI_F * 0.1f);

    // Camera is at (0, 0, -4) looking along the Z axis
    float4x4 View = float4x4::Translation(0.f, 0.0f, 4.f);

    // Get pretransform matrix that rotates the scene according the surface orientation
    auto SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    // Get projection matrix adjusted to the current screen orientation
    auto Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

    // Compute world-view-projection matrix
    m_WorldViewProjMatrix = CubeModelTransform * View * SrfPreTransform * Proj;
}

void Tutorial24_VRS::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (!m_VRSModes.empty())
            ImGui::Combo("VRS mode", &m_VRSMode, m_VRSModes.data(), static_cast<int>(m_VRSModes.size()));

        if (m_VRSMode == VRS_MODE_TEXTURE_BASED)
            ImGui::Text("Click at any point on the screen to change shading rate");
        else if (!m_ShadingRates.empty())
            ImGui::Combo("Default shading rate", &m_ShadingRate, m_ShadingRates.data(), static_cast<int>(m_ShadingRates.size()));

        ImGui::Checkbox("Show shading rate", &m_ShowShadingRate);
        ImGui::Checkbox("Animation", &m_Animation);

        const char* SurfaceScaleStr[] = {"1/4", "1/2", "1", "2", "4"};
        const int   OldSurfaceScale   = m_SurfaceScaleExp2;
        ImGui::TextDisabled("Surface scale");
        ImGui::SliderInt("##SurfaceScale", &m_SurfaceScaleExp2, -2, 2, SurfaceScaleStr[m_SurfaceScaleExp2 + 2]);

        // Recreate render targets
        if (OldSurfaceScale != m_SurfaceScaleExp2)
        {
            const auto& SCDesc = m_pSwapChain->GetDesc();
            WindowResize(SCDesc.Width, SCDesc.Height);
        }
    }
    ImGui::End();
}

#if !(PLATFORM_MACOS || PLATFORM_IOS)
void Tutorial24_VRS::WindowResize(Uint32 Width, Uint32 Height)
{
    if (Width == 0 || Height == 0)
        return;

    // Scale surface
    Width  = ScaleSurface(Width);
    Height = ScaleSurface(Height);

    // Check if the image needs to be recreated.
    if (m_pRTV != nullptr &&
        m_pRTV->GetTexture()->GetDesc().Width == Width &&
        m_pRTV->GetTexture()->GetDesc().Height == Height)
        return;

    const auto& SRProps = m_pDevice->GetAdapterInfo().ShadingRate;

    // Use subsampled render targets, if they are supported, as this may be more optimal.
    const bool CreateSubsampled = (SRProps.CapFlags & SHADING_RATE_CAP_FLAG_SUBSAMPLED_RENDER_TARGET) != 0;

    m_pShadingRateMap = nullptr;
    m_pRTV            = nullptr;
    m_pDSV            = nullptr;

    TextureDesc TexDesc;
    TexDesc.Name      = "Temporary render target";
    TexDesc.Type      = RESOURCE_DIM_TEX_2D;
    TexDesc.Width     = Width;
    TexDesc.Height    = Height;
    TexDesc.Format    = ColorFormat;
    TexDesc.BindFlags = BIND_RENDER_TARGET | BIND_SHADER_RESOURCE;
    TexDesc.MiscFlags = CreateSubsampled ? MISC_TEXTURE_FLAG_SUBSAMPLED : MISC_TEXTURE_FLAG_NONE;

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
    TexDesc.Width     = (Width + SRProps.MinTileSize[0] - 1) / SRProps.MinTileSize[0];
    TexDesc.Height    = (Height + SRProps.MinTileSize[1] - 1) / SRProps.MinTileSize[1];
    TexDesc.BindFlags = BIND_SHADING_RATE;
    TexDesc.MiscFlags = MISC_TEXTURE_FLAG_NONE;

    switch (SRProps.Format)
    {
        case SHADING_RATE_FORMAT_PALETTE: TexDesc.Format = TEX_FORMAT_R8_UINT; break;
        case SHADING_RATE_FORMAT_UNORM8: TexDesc.Format = TEX_FORMAT_RG8_UNORM; break;
        default: UNEXPECTED("Unexpected shading rate texture format");
    }

    RefCntAutoPtr<ITexture> pSRTex;
    m_pDevice->CreateTexture(TexDesc, nullptr, &pSRTex);
    m_pShadingRateMap = pSRTex->GetDefaultView(TEXTURE_VIEW_SHADING_RATE);

    UpdateVRSPattern(m_PrevNormMPos);

    m_BlitSRB = nullptr;
    m_BlitPSO->CreateShaderResourceBinding(&m_BlitSRB);
    m_BlitSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(pRT->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
}

void Tutorial24_VRS::UpdateVRSPattern(const float2 MPos)
{
    if (m_pShadingRateMap == nullptr)
        return;

    m_PrevNormMPos = MPos;

    auto*              pVRSTex = m_pShadingRateMap->GetTexture();
    const auto&        Desc    = pVRSTex->GetDesc();
    const auto&        SRProps = m_pDevice->GetAdapterInfo().ShadingRate;
    std::vector<Uint8> SRData;

    auto GetAxisShadingRate = [&](Uint32 TileIdx, Uint32 NumTiles, float Origin) {
        float TilePos = (static_cast<float>(TileIdx) + 0.5f) / static_cast<float>(NumTiles);
        float Dist    = std::abs(TilePos - Origin);
        auto  Rate    = clamp(static_cast<Uint32>(Dist * (AXIS_SHADING_RATE_MAX + 1) + 0.5f), 0u, Uint32{AXIS_SHADING_RATE_MAX});
        return static_cast<AXIS_SHADING_RATE>(Rate);
    };

    switch (SRProps.Format)
    {
        case SHADING_RATE_FORMAT_PALETTE:
        {
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

            const size_t RowStride = AlignUp(Desc.Width, 32u);
            SRData.resize(RowStride * size_t{Desc.Height});

            for (Uint32 y = 0; y < Desc.Height; ++y)
            {
                for (Uint32 x = 0; x < Desc.Width; ++x)
                {
                    auto XRate = GetAxisShadingRate(x, Desc.Width, MPos.x);
                    auto YRate = GetAxisShadingRate(y, Desc.Height, MPos.y);

                    SRData[size_t{x} + size_t{y} * RowStride] = RemapShadingRate[(XRate << SHADING_RATE_X_SHIFT) | YRate];
                }
            }
            break;
        }
        case SHADING_RATE_FORMAT_UNORM8:
        {
            const size_t RowStride = AlignUp(Desc.Width * 2, 32u);
            SRData.resize(RowStride * size_t{Desc.Height});

            for (Uint32 y = 0; y < Desc.Height; ++y)
            {
                for (Uint32 x = 0; x < Desc.Width; ++x)
                {
                    auto XRate = GetAxisShadingRate(x, Desc.Width, MPos.x);
                    auto YRate = GetAxisShadingRate(y, Desc.Height, MPos.y);

                    SRData[size_t{x} * 2u + size_t{y} * RowStride + 0u] = 255 >> XRate;
                    SRData[size_t{x} * 2u + size_t{y} * RowStride + 1u] = 255 >> YRate;
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

    // If shading rate access type is not ON_GPU, access to the texture happens on the CPU
    // side during SetRenderTargetsExt() or Flush() call, so we have to wait until the texture
    // is updated.
    const bool GPUtoCPUSyncRequired = (SRProps.ShadingRateTextureAccess != SHADING_RATE_TEXTURE_ACCESS_ON_GPU);

    if (GPUtoCPUSyncRequired)
    {
        m_pImmediateContext->Flush();
        m_pImmediateContext->WaitForIdle();
    }

    m_pImmediateContext->UpdateTexture(pVRSTex, 0, 0, TexBox, SubResData, RESOURCE_STATE_TRANSITION_MODE_NONE, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    if (GPUtoCPUSyncRequired)
    {
        m_pImmediateContext->Flush();
        m_pImmediateContext->WaitForIdle();
    }
}
#endif

} // namespace Diligent
