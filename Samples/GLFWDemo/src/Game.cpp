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

#include <random>
#include <vector>

#include "Game.hpp"
#include "ShaderMacroHelper.hpp"
#include "CallbackWrapper.hpp"

namespace Diligent
{

namespace
{

#include "../assets/Structures.fxh"

static_assert(sizeof(MapConstants) % 16 == 0, "must be aligned to 16 bytes");
static_assert(sizeof(PlayerConstants) % 16 == 0, "must be aligned to 16 bytes");

} // namespace

inline float fract(float x)
{
    return x - floor(x);
}

GLFWDemo* CreateGLFWApp()
{
    return new Game{};
}

bool Game::Initialize()
{
    try
    {
        GetEngineFactory()->CreateDefaultShaderSourceStreamFactory(nullptr, &m_pShaderSourceFactory);
        CHECK_THROW(m_pShaderSourceFactory);

        RefCntAutoPtr<IRenderStateNotationParser> pRSNParser;
        {
            CreateRenderStateNotationParser({}, &pRSNParser);
            CHECK_THROW(pRSNParser);
            pRSNParser->ParseFile("RenderStates.json", m_pShaderSourceFactory);
        }
        {
            RenderStateNotationLoaderCreateInfo RSNLoaderCI{};
            RSNLoaderCI.pDevice        = GetDevice();
            RSNLoaderCI.pStreamFactory = m_pShaderSourceFactory;
            RSNLoaderCI.pParser        = pRSNParser;
            CreateRenderStateNotationLoader(RSNLoaderCI, &m_pRSNLoader);
            CHECK_THROW(m_pRSNLoader);
        }

        GenerateMap();
        CreateSDFMap();
        CreatePipelineState();
        InitPlayer();
        BindResources();

        return true;
    }
    catch (...)
    {
        return false;
    }
}

void Game::Update(float dt)
{
    dt = std::min(dt, Constants.MaxDT);

    // update player position
    const auto PosDeltaLen = length(m_Player.PendingPos);
    if (PosDeltaLen > 0.1f)
    {
        const float2 StartPos = m_Player.Pos;
        const float2 Dir      = (m_Player.PendingPos / PosDeltaLen);
        const float2 EndPos   = m_Player.Pos + Dir * dt * Constants.PlayerVelocity;
        const uint2  TexDim   = Constants.MapTexDim;

        const auto ReadMap = [&](int x, int y) //
        {
            if (x >= 0 && x < static_cast<int>(TexDim.x) &&
                y >= 0 && y < static_cast<int>(TexDim.y))
            {
                return m_Map.MapData[x + y * TexDim.x] ? 1.0f : 0.0f;
            }
            else
                return 1.0f;
        };

        // check collisions with walls
        for (Uint32 i = 0; i < Constants.MaxCollisionSteps; ++i)
        {
            const float2 Pos      = lerp(StartPos, EndPos, static_cast<float>(i) / (Constants.MaxCollisionSteps - 1));
            const float2 FetchPos = Pos - float2(0.5f, 0.5f);

            // MapData is a 1 bit distance field, use bilinear filter to calculate distance from nearest wall to player position
            int   x    = static_cast<int>(FetchPos.x);
            int   y    = static_cast<int>(FetchPos.y);
            float c00  = ReadMap(x, y);
            float c10  = ReadMap(x + 1, y);
            float c01  = ReadMap(x, y + 1);
            float c11  = ReadMap(x + 1, y + 1);
            float dist = lerp(lerp(c00, c10, fract(FetchPos.x)), lerp(c01, c11, fract(FetchPos.x)), fract(FetchPos.y));

            if (dist > Constants.PlayerRadius)
                break; // intersection found

            m_Player.Pos = Pos;
        }

        // test intersection with teleport
        float DistToTeleport = length(m_Map.TeleportPos - m_Player.Pos);
        if (DistToTeleport < Constants.TeleportRadius)
        {
            // m_Player.Pos will be set to map center
            LoadNewMap();
        }
    }
    m_Player.PendingPos = {};

    // update flash light direction
    {
        float2 XRange, YRange;
        GetScreenTransform(XRange, YRange);

        // convert player position to signed normalized screen coordinates
        float2 UNormPlayerPos = m_Player.Pos / Constants.MapTexDim.Recast<float>();
        float2 SNormPlayerPos = float2{lerp(XRange.x, XRange.y, UNormPlayerPos.x),
                                       lerp(YRange.x, YRange.y, UNormPlayerPos.y)};

        // convert mouse position to signed normalized screen coordinates
        const auto& SCDesc        = GetSwapChain()->GetDesc();
        float2      SNormMousePos = (m_Player.MousePos / uint2{SCDesc.Width, SCDesc.Height}.Recast<float>()) * 2.0f - float2(1.f, 1.f);
        SNormMousePos.y           = -SNormMousePos.y;

        // calculate direction from player position to mouse position
        m_Player.FlashLightDir = normalize(SNormMousePos - SNormPlayerPos);
    }

    // increase brightness if left mouse button pressed, decrease if not pressed
    m_Player.FlashLightPower = clamp(m_Player.FlashLightPower + Constants.FlashLightAttenuation * (m_Player.LMBPressed ? dt : -dt), 0.0f, 1.0f);

    m_Map.TeleportWaveAnim = fract(m_Map.TeleportWaveAnim + dt * 0.5f);
}

void Game::Draw()
{
    auto* pContext   = GetContext();
    auto* pSwapchain = GetSwapChain();

    ITextureView* pRTV = pSwapchain->GetCurrentBackBufferRTV();
    pContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    const float ClearColor[4] = {};
    pContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_VERIFY);

    // update map constants
    {
        const auto&  SCDesc = pSwapchain->GetDesc();
        MapConstants Const;

        GetScreenTransform(Const.ScreenRectLR, Const.ScreenRectTB);

        Const.UVToMap            = Constants.MapTexDim.Recast<float>();
        Const.MapToUV            = float2(1.0f, 1.0f) / Const.UVToMap;
        Const.TeleportRadius     = Constants.TeleportRadius;
        Const.TeleportWaveRadius = Constants.TeleportRadius * m_Map.TeleportWaveAnim;
        Const.TeleportPos        = m_Map.TeleportPos;

        pContext->UpdateBuffer(m_Map.pConstants, 0, sizeof(Const), &Const, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    // update player constants
    {
        PlayerConstants Const;
        Const.PlayerPos          = m_Player.Pos;
        Const.PlayerRadius       = Constants.PlayerRadius;
        Const.FlashLightDir      = m_Player.FlashLightDir;
        Const.FlashLightPower    = m_Player.FlashLightPower;
        Const.AmbientLightRadius = Constants.AmbientLightRadius;
        Const.FlshLightMaxDist   = Constants.FlshLightMaxDist;

        pContext->UpdateBuffer(m_Player.pConstants, 0, sizeof(Const), &Const, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    // draw map, player and flash light using single shader with ray marching
    {
        pContext->SetPipelineState(m_Map.pPSO);
        pContext->CommitShaderResources(m_Map.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DrawAttribs drawAttr{4, DRAW_FLAG_VERIFY_ALL};
        pContext->Draw(drawAttr);
    }

    pContext->Flush();
    pSwapchain->Present();
}

void Game::GetScreenTransform(float2& XRange, float2& YRange)
{
    const auto& SCDesc       = GetSwapChain()->GetDesc();
    const float ScreenAspect = static_cast<float>(SCDesc.Width) / SCDesc.Height;
    const float TexAspect    = static_cast<float>(Constants.MapTexDim.x) / Constants.MapTexDim.y;

    if (ScreenAspect > TexAspect)
    {
        XRange.y = (TexAspect / ScreenAspect);
        YRange.y = 1.f;
    }
    else
    {
        XRange.y = 1.f;
        YRange.y = (ScreenAspect / TexAspect);
    }
    XRange.x = -XRange.y;
    YRange.x = -YRange.y;
}

void Game::KeyEvent(Key key, KeyState state)
{
    if (state == KeyState::Press || state == KeyState::Repeat)
    {
        switch (key)
        {
            case Key::W:
            case Key::Up:
            case Key::NP_Up:
                m_Player.PendingPos.y += 1.0f;
                break;

            case Key::S:
            case Key::Down:
            case Key::NP_Down:
                m_Player.PendingPos.y -= 1.0f;
                break;

            case Key::D:
            case Key::Right:
            case Key::NP_Right:
                m_Player.PendingPos.x += 1.0f;
                break;

            case Key::A:
            case Key::Left:
            case Key::NP_Left:
                m_Player.PendingPos.x -= 1.0f;
                break;

            case Key::Space:
                m_Player.PendingPos = m_Player.FlashLightDir;
                break;

            case Key::Esc:
                Quit();
                break;

            default:
                break;
        }
    }

    if (key == Key::MB_Left)
        m_Player.LMBPressed = (state != KeyState::Release);

    // generate new map
    if (state == KeyState::Release && key == Key::Tab)
        LoadNewMap();
}

void Game::MouseEvent(float2 pos)
{
    m_Player.MousePos = pos;
}

void Game::GenerateMap()
{
    const uint2 TexDim  = Constants.MapTexDim;
    auto&       MapData = m_Map.MapData;

    MapData.clear();
    MapData.resize(size_t{TexDim.x} * size_t{TexDim.y}, 0);

    // Set top and bottom borders
    for (Uint32 x = 0, x2 = TexDim.x * (TexDim.y - 1); x < TexDim.x; ++x, ++x2)
    {
        MapData[x]  = true;
        MapData[x2] = true;
    }

    // Set left and right borders
    for (size_t y = 0; y < TexDim.y; ++y)
    {
        MapData[y * TexDim.x]           = true;
        MapData[(y + 1) * TexDim.x - 1] = true;
    }

    // Generate random walls and write them to a 1-bit texture
    {
        std::mt19937                       Gen{std::random_device{}()};
        std::uniform_int_distribution<int> NumSegDistrib{0, 4};
        std::uniform_int_distribution<int> SegmentDistrib{-3, 4};

        const auto SetPixel = [&](int2 pos) {
            if (pos.x >= 0 && pos.x < static_cast<int>(TexDim.x) &&
                pos.y >= 0 && pos.y < static_cast<int>(TexDim.y))
                MapData[pos.x + pos.y * TexDim.x] = true;
        };

        for (Uint32 y = 2; y < TexDim.y - 2; y += 4)
        {
            for (Uint32 x = 2; x < TexDim.x - 2; x += 4)
            {
                const int2 Center      = uint2{x, y}.Recast<int>();
                const int  NumSegments = NumSegDistrib(Gen);

                int2 Pos = Center;
                for (int s = 0; s < NumSegments; ++s)
                {
                    const Uint32 Axis   = static_cast<Uint32>(s) & 1;
                    const int    Count  = SegmentDistrib(Gen);
                    const int    MinPos = std::min(0, Count);
                    const int    MaxPos = std::max(0, Count);

                    for (int i = MinPos; i < MaxPos; ++i)
                    {
                        int2 Offset;
                        Offset[Axis] = i;
                        SetPixel(Pos + Offset);
                    }

                    Pos[Axis] += Count;
                }
            }
        }
    }

    // Clear center to put player
    for (Uint32 y = TexDim.y / 2 - 2; y < TexDim.y / 2 + 2; ++y)
    {
        for (Uint32 x = TexDim.x / 2 - 2; x < TexDim.x / 2 + 2; ++x)
        {
            MapData[x + y * TexDim.x] = false;
        }
    }

    // Find position for the teleport
    {
        std::mt19937                          Gen{std::random_device{}()};
        std::uniform_real_distribution<float> Seed{0.0f, 0.2f};

        const auto TestTeleportPos = [&](int2 pos, int2& EmptyPixelPos, float& Suitability) //
        {
            const int FetchOffset = 2;
            float     MinDist     = 1.0e+10f;
            EmptyPixelPos         = pos;
            Suitability           = Seed(Gen);
            for (int y = pos.y - FetchOffset; y < pos.y + FetchOffset; ++y)
            {
                for (int x = pos.x - FetchOffset; x < pos.x + FetchOffset; ++x)
                {
                    if (x >= 0 && y >= 0 && x < static_cast<int>(TexDim.x) && y < static_cast<int>(TexDim.y))
                    {
                        float Dist    = length(int2(x, y).Recast<float>() - pos.Recast<float>());
                        bool  IsEmpty = !MapData[x + y * TexDim.x];
                        Suitability += (IsEmpty ? 1.f : 0.f) / std::max(1.0f, Dist * Dist);
                        if (IsEmpty && Dist < MinDist)
                        {
                            MinDist       = Dist;
                            EmptyPixelPos = int2(x, y);
                        }
                    }
                }
            }
        };

        const Uint32 Offset = 3;
        const uint2  CandidatePos[] =
            {
                // clang-format off
                uint2{           Offset,            Offset},
                uint2{TexDim.x - Offset,            Offset},
                uint2{           Offset, TexDim.y - Offset},
                uint2{TexDim.x - Offset, TexDim.y - Offset},
                uint2{TexDim.x / 2,                 Offset},
                uint2{TexDim.x / 2,      TexDim.y - Offset},
                uint2{           Offset, TexDim.y / 2     },
                uint2{TexDim.x - Offset, TexDim.y / 2     }
                // clang-format on
            };

        float MaxSuitability = 0.0f;
        for (Uint32 i = 0; i < _countof(CandidatePos); ++i)
        {
            float Suitability;
            int2  Pos;
            TestTeleportPos(CandidatePos[i].Recast<int>(), Pos, Suitability);

            if (Suitability > MaxSuitability)
            {
                MaxSuitability    = Suitability;
                m_Map.TeleportPos = Pos.Recast<float>();
            }
        }
        m_Map.TeleportWaveAnim = 0.0f;
    }
}

void Game::CreateSDFMap()
{
    const uint2 SrcTexDim = Constants.MapTexDim;
    const uint2 DstTexDim = Constants.SDFTexDim;

    // Create Src and Dst textures.
    RefCntAutoPtr<ITexture> pSrcTex;
    {
        TextureDesc TexDesc;
        TexDesc.Name      = "SDF Map texture";
        TexDesc.Type      = RESOURCE_DIM_TEX_2D;
        TexDesc.Width     = DstTexDim.x;
        TexDesc.Height    = DstTexDim.y;
        TexDesc.Format    = TEX_FORMAT_R16_FLOAT;
        TexDesc.BindFlags = BIND_SHADER_RESOURCE | BIND_UNORDERED_ACCESS;

        m_Map.pMapTex = nullptr;
        GetDevice()->CreateTexture(TexDesc, nullptr, &m_Map.pMapTex);
        CHECK_THROW(m_Map.pMapTex != nullptr);

        TexDesc.Name      = "Src texture";
        TexDesc.Width     = SrcTexDim.x;
        TexDesc.Height    = SrcTexDim.y;
        TexDesc.Format    = TEX_FORMAT_R8_UNORM;
        TexDesc.BindFlags = BIND_SHADER_RESOURCE;

        GetDevice()->CreateTexture(TexDesc, nullptr, &pSrcTex);
        CHECK_THROW(pSrcTex != nullptr);
    }


    RefCntAutoPtr<IPipelineState>         pGenSdfPSO;
    RefCntAutoPtr<IShaderResourceBinding> pGenSdfSRB;
    {
        {
            ShaderMacroHelper Macros;
            Macros.AddShaderMacro("RADIUS", Constants.TexFilterRadius);
            Macros.AddShaderMacro("DIST_SCALE", 1.0f / float(Constants.SDFTexScale));

            auto Callback = MakeCallback([&](ShaderCreateInfo& ShaderCI, SHADER_TYPE ShaderType, bool& IsAddToCache) {
                ShaderCI.Macros = Macros;
            });

            m_pRSNLoader->LoadPipelineState({"Generate SDF map PSO", PIPELINE_TYPE_COMPUTE, true, nullptr, nullptr, Callback, Callback}, &pGenSdfPSO);
            CHECK_THROW(pGenSdfPSO != nullptr);
        }

        pGenSdfPSO->CreateShaderResourceBinding(&pGenSdfSRB, true);
        CHECK_THROW(pGenSdfSRB != nullptr);

        pGenSdfSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_SrcTex")->Set(pSrcTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        pGenSdfSRB->GetVariableByName(SHADER_TYPE_COMPUTE, "g_DstTex")->Set(m_Map.pMapTex->GetDefaultView(TEXTURE_VIEW_UNORDERED_ACCESS));
    }

    // Generate signed distance field (SDF)
    auto* pContext = GetContext();

    // upload map to pSrcTex
    {
        std::vector<Uint8> MapData(m_Map.MapData.size());
        VERIFY_EXPR(MapData.size() == (size_t{SrcTexDim.x} * size_t{SrcTexDim.y}));

        // convert 1-bit to 8-bit texture
        for (size_t i = 0; i < MapData.size(); ++i)
            MapData[i] = m_Map.MapData[i] ? 0xFF : 0;

        TextureSubResData SubresData;
        SubresData.pData       = MapData.data();
        SubresData.Stride      = sizeof(MapData[0]) * SrcTexDim.x;
        SubresData.DepthStride = static_cast<Uint32>(MapData.size());

        pContext->UpdateTexture(pSrcTex, 0, 0, Box{0, SrcTexDim.x, 0, SrcTexDim.y}, SubresData, RESOURCE_STATE_TRANSITION_MODE_NONE, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }

    // Compute SDF - for each pixel find the minimal distance from empty space to a wall or from the wall to empty space.
    {
        const uint2 LocalGroupSize = {8, 8};

        pContext->SetPipelineState(pGenSdfPSO);
        pContext->CommitShaderResources(pGenSdfSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        DispatchComputeAttribs dispatchAttrs;
        dispatchAttrs.ThreadGroupCountX = (DstTexDim.x + LocalGroupSize.x - 1) / LocalGroupSize.x;
        dispatchAttrs.ThreadGroupCountY = (DstTexDim.y + LocalGroupSize.y - 1) / LocalGroupSize.y;

        pContext->DispatchCompute(dispatchAttrs);
    }

    pContext->Flush();
}

void Game::CreatePipelineState()
{
    auto Callback = MakeCallback([&](PipelineStateCreateInfo& PipelineCI) {
        auto& GraphicsPipelineCI{static_cast<GraphicsPipelineStateCreateInfo&>(PipelineCI)};
        GraphicsPipelineCI.GraphicsPipeline.RTVFormats[0]    = GetSwapChain()->GetDesc().ColorBufferFormat;
        GraphicsPipelineCI.GraphicsPipeline.NumRenderTargets = 1;
    });

    m_pRSNLoader->LoadPipelineState({"Draw map PSO", PIPELINE_TYPE_GRAPHICS, true, Callback, Callback}, &m_Map.pPSO);
    CHECK_THROW(m_Map.pPSO != nullptr);

    BufferDesc CBDesc;
    CBDesc.Name      = "Map constants buffer";
    CBDesc.Size      = sizeof(MapConstants);
    CBDesc.Usage     = USAGE_DEFAULT;
    CBDesc.BindFlags = BIND_UNIFORM_BUFFER;

    GetDevice()->CreateBuffer(CBDesc, nullptr, &m_Map.pConstants);
    CHECK_THROW(m_Map.pConstants != nullptr);
}

void Game::InitPlayer()
{
    if (!m_Player.pConstants)
    {
        BufferDesc CBDesc;
        CBDesc.Name      = "Player constants buffer";
        CBDesc.Size      = sizeof(PlayerConstants);
        CBDesc.Usage     = USAGE_DEFAULT;
        CBDesc.BindFlags = BIND_UNIFORM_BUFFER;

        GetDevice()->CreateBuffer(CBDesc, nullptr, &m_Player.pConstants);
        CHECK_THROW(m_Player.pConstants != nullptr);
    }

    m_Player.Pos = Constants.MapTexDim.Recast<float>() * 0.5f;
}

void Game::BindResources()
{
    // Recreate SRB because variable declared as mutable and can not be changed.
    m_Map.pSRB = nullptr;
    m_Map.pPSO->CreateShaderResourceBinding(&m_Map.pSRB, true);
    CHECK_THROW(m_Map.pSRB != nullptr);

    m_Map.pSRB->GetVariableByName(SHADER_TYPE_VERTEX, "cbMapConstants")->Set(m_Map.pConstants);
    m_Map.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbMapConstants")->Set(m_Map.pConstants);
    m_Map.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_SDFMap")->Set(m_Map.pMapTex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
    m_Map.pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "cbPlayerConstants")->Set(m_Player.pConstants);
}

void Game::LoadNewMap()
{
    try
    {
        GetDevice()->IdleGPU();
        GenerateMap();
        CreateSDFMap();
        InitPlayer();
        BindResources();
    }
    catch (...)
    {}
}

} // namespace Diligent
