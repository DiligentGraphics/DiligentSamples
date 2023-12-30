/*
 *  Copyright 2019-2023 Diligent Graphics LLC
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

#include <cmath>
#include <array>

#include "GLTFViewer.hpp"
#include "MapHelper.hpp"
#include "BasicMath.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "CommonlyUsedStates.h"
#include "ShaderMacroHelper.hpp"
#include "FileSystem.hpp"
#include "imgui.h"
#include "imGuIZMO.h"
#include "ImGuiUtils.hpp"
#include "CallbackWrapper.hpp"
#include "CommandLineParser.hpp"
#include "GraphicsAccessories.hpp"
#include "EnvMapRenderer.hpp"

namespace Diligent
{

namespace HLSL
{

#include "Shaders/Common/public/BasicStructures.fxh"
#include "Shaders/PBR/public/PBR_Structures.fxh"
#include "Shaders/PBR/private/RenderPBR_Structures.fxh"

} // namespace HLSL

SampleBase* CreateSample()
{
    return new GLTFViewer();
}

// clang-format off
const std::pair<const char*, const char*> DefaultGLTFModels[] =
{
    {"Damaged Helmet",               "models/DamagedHelmet/DamagedHelmet.gltf"},
    {"Flight Helmet",                "models/FlightHelmet/glTF/FlightHelmet.gltf"},
    {"Cesium Man",                   "models/CesiumMan/glTF/CesiumMan.gltf"},
    {"Boom Box",                     "models/BoomBoxWithAxes/glTF/BoomBoxWithAxes.gltf"},
    {"Clearcoat Ring",               "models/ClearcoatRing/glTF/ClearcoatRing.gltf"},
    {"Glam Velvet Sofa",             "models/GlamVelvetSofa/glTF/GlamVelvetSofa.gltf"},
    {"Iridescence Abalone",          "models/IridescenceAbalone/glTF/IridescenceAbalone.gltf"},
    {"Iridescent Dish With Olives",  "models/IridescentDishWithOlives/glTF/IridescentDishWithOlives.gltf"},
    {"Toy Car",                      "models/ToyCar/glTF/ToyCar.gltf"},
    {"Anisotropy Barn Lamp",         "models/AnisotropyBarnLamp/glTF/AnisotropyBarnLamp.gltf"},
    {"White Furnace Test",           "models/EnvironmentTest/glTF/EnvironmentTest.gltf"},
    {"Alpha Blend Mode Test",        "models/AlphaBlendModeTest/glTF/AlphaBlendModeTest.gltf"},
    {"Metal Rough Spheres",          "models/MetalRoughSpheres/glTF/MetalRoughSpheres.gltf"},
    {"Normal Tangent Test",          "models/NormalTangentTest/glTF/NormalTangentTest.gltf"},
    {"Emissive Strength Test",       "models/EmissiveStrengthTest/glTF/EmissiveStrengthTest.gltf"},
    {"Clear Coat Test",              "models/ClearCoatTest/glTF/ClearCoatTest.gltf"},
    {"Anisotropy Disc Test",         "models/AnisotropyDiscTest/glTF/AnisotropyDiscTest.gltf"},
    {"Anisotropy Strength Test",     "models/AnisotropyStrengthTest/glTF/AnisotropyStrengthTest.gltf"},
    {"Iridescence Metallic Spheres", "models/IridescenceMetallicSpheres/glTF/IridescenceMetallicSpheres.gltf"},
    {"Interpolation Test",           "models/InterpolationTest/glTF/InterpolationTest.gltf"},
    {"Multi UV Test",                "models/MultiUVTest/glTF/MultiUVTest.gltf"},
    {"Orientation Test",             "models/OrientationTest/glTF/OrientationTest.gltf"},
    {"Sheen Test Grid",              "models/SheenTestGrid/glTF/SheenTestGrid.gltf"},
    {"Texture Coordinate Test",      "models/TextureCoordinateTest/glTF/TextureCoordinateTest.gltf"},
    {"Texture Transform Multi Test", "models/TextureTransformMultiTest/glTF/TextureTransformMultiTest.gltf"},
    {"Unlit Test",                   "models/UnlitTest/glTF/UnlitTest.gltf"},
    {"Vertex Color Test",            "models/VertexColorTest/glTF/VertexColorTest.gltf"},
};
// clang-format on

enum GBUFFER_RT : Uint32
{
    GBUFFER_RT_COLOR,
    GBUFFER_RT_NORMAL,
    GBUFFER_RT_MATERIAL_DATA,
    GBUFFER_RT_MOTION_VECTORS,
    GBUFFER_RT_DEPTH,
    GBUFFER_RT_COUNT
};

enum GBUFFER_RT_FLAG : Uint32
{
    GBUFFER_RT_FLAG_COLOR          = 1u << GBUFFER_RT_COLOR,
    GBUFFER_RT_FLAG_NORMAL         = 1u << GBUFFER_RT_NORMAL,
    GBUFFER_RT_FLAG_MATERIAL_DATA  = 1u << GBUFFER_RT_MATERIAL_DATA,
    GBUFFER_RT_FLAG_MOTION_VECTORS = 1u << GBUFFER_RT_MOTION_VECTORS,
    GBUFFER_RT_FLAG_DEPTH          = 1u << GBUFFER_RT_DEPTH,
    GBUFFER_RT_FLAG_LAST           = GBUFFER_RT_FLAG_DEPTH,
    GBUFFER_RT_FLAG_ALL            = (GBUFFER_RT_FLAG_LAST << 1u) - 1u,
};
DEFINE_FLAG_ENUM_OPERATORS(GBUFFER_RT_FLAG);

GLTFViewer::GLTFViewer() :
    m_CameraAttribs{std::make_unique<HLSL::CameraAttribs[]>(2)}
{
    m_Camera.SetDefaultSecondaryRotation(QuaternionF::RotationFromAxisAngle(float3{0.f, 1.0f, 0.0f}, -PI_F / 2.f));
    m_Camera.SetDistRange(0.1f, 5.f);
    m_Camera.SetDefaultDistance(0.9f);
    m_Camera.ResetDefaults();
    // Apply extra rotation to adjust the view to match Khronos GLTF viewer
    m_Camera.SetExtraRotation(QuaternionF::RotationFromAxisAngle(float3{0.75, 0.0, 0.75}, PI_F));
}

void GLTFViewer::LoadModel(const char* Path)
{
    if (m_Model)
    {
        m_PlayAnimation  = false;
        m_AnimationIndex = 0;
        m_AnimationTimers.clear();
        m_bResetPrevCamera = true;
    }

    GLTF::ModelCreateInfo ModelCI;
    ModelCI.FileName             = Path;
    ModelCI.pResourceManager     = m_bUseResourceCache ? m_pResourceMgr.RawPtr() : nullptr;
    ModelCI.ComputeBoundingBoxes = m_bComputeBoundingBoxes;

    m_Model = std::make_unique<GLTF::Model>(m_pDevice, m_pImmediateContext, ModelCI);

    m_ModelResourceBindings = m_GLTFRenderer->CreateResourceBindings(*m_Model, m_FrameAttribsCB);

    m_RenderParams.SceneIndex = m_Model->DefaultSceneId;
    UpdateScene();

    if (!m_Model->Animations.empty())
    {
        m_AnimationTimers.resize(m_Model->Animations.size());
        m_AnimationIndex = 0;
        m_PlayAnimation  = true;
    }

    m_CameraId = 0;
    m_CameraNodes.clear();
    for (const auto* node : m_Model->Scenes[m_RenderParams.SceneIndex].LinearNodes)
    {
        if (node->pCamera != nullptr && node->pCamera->Type == GLTF::Camera::Projection::Perspective)
            m_CameraNodes.push_back(node);
    }

    if (strstr(Path, "EnvironmentTest") != nullptr)
    {
        SetEnvironmentMap(m_WhiteFurnaceEnvMapSRV);
        m_LightIntensity = 0.0f;
    }
    else
    {
        if (SetEnvironmentMap(m_EnvironmentMapSRV))
            m_LightIntensity = 3.f;
    }

    m_ModelPath = Path;
}

void GLTFViewer::UpdateScene()
{
    m_Model->ComputeTransforms(m_RenderParams.SceneIndex, m_Transforms[0]);
    m_ModelAABB = m_Model->ComputeBoundingBox(m_RenderParams.SceneIndex, m_Transforms[0]);

    // Center and scale model
    float  MaxDim = 0;
    float3 ModelDim{m_ModelAABB.Max - m_ModelAABB.Min};
    MaxDim = std::max(MaxDim, ModelDim.x);
    MaxDim = std::max(MaxDim, ModelDim.y);
    MaxDim = std::max(MaxDim, ModelDim.z);

    float    Scale     = (1.0f / std::max(MaxDim, 0.01f)) * 0.5f;
    auto     Translate = -m_ModelAABB.Min - 0.5f * ModelDim;
    float4x4 InvYAxis  = float4x4::Identity();
    InvYAxis._22       = -1;

    m_ModelTransform = float4x4::Translation(Translate) * float4x4::Scale(Scale) * InvYAxis;
    m_Model->ComputeTransforms(m_RenderParams.SceneIndex, m_Transforms[0], m_ModelTransform);
    m_ModelAABB     = m_Model->ComputeBoundingBox(m_RenderParams.SceneIndex, m_Transforms[0]);
    m_Transforms[1] = m_Transforms[0];
}


void GLTFViewer::UpdateModelsList(const std::string& Dir)
{
    m_Models.clear();
    for (size_t i = 0; i < _countof(DefaultGLTFModels); ++i)
    {
        m_Models.push_back(ModelInfo{DefaultGLTFModels[i].first, DefaultGLTFModels[i].second});
    }

#if PLATFORM_WIN32 || PLATFORM_LINUX || PLATFORM_MACOS
    if (!Dir.empty())
    {
        auto SearchRes = FileSystem::SearchRecursive(Dir.c_str(), "*.gltf");
        for (const auto& File : SearchRes)
        {
            m_Models.push_back(ModelInfo{File.Name, Dir + FileSystem::SlashSymbol + File.Name});
        }
    }
#endif
}

GLTFViewer::CommandLineStatus GLTFViewer::ProcessCommandLine(int argc, const char* const* argv)
{
    CommandLineParser ArgsParser{argc, argv};
    ArgsParser.Parse("use_cache", m_bUseResourceCache);
    ArgsParser.Parse("model", m_ModelPath);
    ArgsParser.Parse("compute_bounds", m_bComputeBoundingBoxes);

    std::string ExtraModelsDir;
    ArgsParser.Parse("dir", 'd', ExtraModelsDir);
    UpdateModelsList(ExtraModelsDir.c_str());

    return CommandLineStatus::OK;
}

void GLTFViewer::CreateGLTFResourceCache()
{
    auto InputLayout = GLTF::VertexAttributesToInputLayout(GLTF::DefaultVertexAttributes.data(), static_cast<Uint32>(GLTF::DefaultVertexAttributes.size()));
    auto Strides     = InputLayout.ResolveAutoOffsetsAndStrides();

    std::vector<VertexPoolElementDesc> VtxPoolElems;
    VtxPoolElems.reserve(Strides.size());
    m_CacheUseInfo.VtxLayoutKey.Elements.reserve(Strides.size());
    for (const auto& Stride : Strides)
    {
        VtxPoolElems.emplace_back(Stride, BIND_VERTEX_BUFFER);
        m_CacheUseInfo.VtxLayoutKey.Elements.emplace_back(Stride, BIND_VERTEX_BUFFER);
    }

    VertexPoolCreateInfo VtxPoolCI;
    VtxPoolCI.Desc.Name        = "GLTF vertex pool";
    VtxPoolCI.Desc.VertexCount = 32768;
    VtxPoolCI.Desc.pElements   = VtxPoolElems.data();
    VtxPoolCI.Desc.NumElements = static_cast<Uint32>(VtxPoolElems.size());

    std::array<DynamicTextureAtlasCreateInfo, 1> Atlases;
    Atlases[0].Desc.Name      = "GLTF texture atlas";
    Atlases[0].Desc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
    Atlases[0].Desc.Usage     = USAGE_DEFAULT;
    Atlases[0].Desc.BindFlags = BIND_SHADER_RESOURCE;
    Atlases[0].Desc.Format    = TEX_FORMAT_RGBA8_UNORM;
    Atlases[0].Desc.Width     = 4096;
    Atlases[0].Desc.Height    = 4096;
    Atlases[0].Desc.MipLevels = 6;

    GLTF::ResourceManager::CreateInfo ResourceMgrCI;

    ResourceMgrCI.IndexAllocatorCI.Desc.Name      = "GLTF index buffer";
    ResourceMgrCI.IndexAllocatorCI.Desc.BindFlags = BIND_INDEX_BUFFER;
    ResourceMgrCI.IndexAllocatorCI.Desc.Usage     = USAGE_DEFAULT;
    ResourceMgrCI.IndexAllocatorCI.Desc.Size      = sizeof(Uint32) * 8 << 10;

    ResourceMgrCI.NumVertexPools = 1;
    ResourceMgrCI.pVertexPoolCIs = &VtxPoolCI;

    ResourceMgrCI.DefaultAtlasDesc.Desc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
    ResourceMgrCI.DefaultAtlasDesc.Desc.Usage     = USAGE_DEFAULT;
    ResourceMgrCI.DefaultAtlasDesc.Desc.BindFlags = BIND_SHADER_RESOURCE;
    ResourceMgrCI.DefaultAtlasDesc.Desc.Width     = 4096;
    ResourceMgrCI.DefaultAtlasDesc.Desc.Height    = 4096;
    ResourceMgrCI.DefaultAtlasDesc.Desc.MipLevels = 6;

    m_pResourceMgr = GLTF::ResourceManager::Create(m_pDevice, ResourceMgrCI);

    m_CacheUseInfo.pResourceMgr = m_pResourceMgr;

    m_CacheUseInfo.BaseColorFormat    = TEX_FORMAT_RGBA8_UNORM;
    m_CacheUseInfo.PhysicalDescFormat = TEX_FORMAT_RGBA8_UNORM;
    m_CacheUseInfo.NormalFormat       = TEX_FORMAT_RGBA8_UNORM;
    m_CacheUseInfo.OcclusionFormat    = TEX_FORMAT_RGBA8_UNORM;
    m_CacheUseInfo.EmissiveFormat     = TEX_FORMAT_RGBA8_UNORM;
}

static RefCntAutoPtr<ITextureView> CreateWhiteFurnaceEnvMap(IRenderDevice* pDevice)
{
    TextureDesc TexDesc;
    TexDesc.Name      = "White Furnace Env Map";
    TexDesc.Type      = RESOURCE_DIM_TEX_CUBE;
    TexDesc.Usage     = USAGE_IMMUTABLE;
    TexDesc.BindFlags = BIND_SHADER_RESOURCE;
    TexDesc.Format    = TEX_FORMAT_RGBA32_FLOAT;
    TexDesc.Width     = 16;
    TexDesc.Height    = 16;
    TexDesc.MipLevels = 1;
    TexDesc.ArraySize = 6;

    std::vector<float4>            Data(6 * TexDesc.Width * TexDesc.Height, float4{1});
    std::vector<TextureSubResData> SubResData(6);
    for (auto& Subres : SubResData)
    {
        Subres.pData  = Data.data();
        Subres.Stride = TexDesc.Width * sizeof(float4);
    }
    TextureData InitData{SubResData.data(), static_cast<Uint32>(SubResData.size())};

    RefCntAutoPtr<ITexture> pEnvMap;
    pDevice->CreateTexture(TexDesc, &InitData, &pEnvMap);
    VERIFY_EXPR(pEnvMap);

    return RefCntAutoPtr<ITextureView>{pEnvMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE)};
}

bool GLTFViewer::SetEnvironmentMap(ITextureView* pEnvMap)
{
    if (m_pCurrentEnvMapSRV == pEnvMap)
        return false;

    m_pCurrentEnvMapSRV = pEnvMap;
    m_GLTFRenderer->PrecomputeCubemaps(m_pImmediateContext, m_pCurrentEnvMapSRV);

    return true;
}

static PBR_Renderer::CreateInfo::PSMainSourceInfo GetPbrPSMainSource(PBR_Renderer::PSO_FLAGS PSOFlags)
{
    PBR_Renderer::CreateInfo::PSMainSourceInfo PSMainInfo;

    PSMainInfo.OutputStruct = R"(
struct PSOutput
{
    float4 Color        : SV_Target0;
    float4 Normal       : SV_Target1;
    float4 MaterialData : SV_Target2;
    float4 MotionVec    : SV_Target3;
};
)";

    PSMainInfo.Footer = R"(
    PSOutput PSOut;
#if UNSHADED
    PSOut.Color = g_Frame.Renderer.UnshadedColor + g_Frame.Renderer.HighlightColor;
#else
    PSOut.Color = OutColor;
#endif
    
    PSOut.Normal       = float4(Shading.BaseLayer.Normal, 1.0);
    PSOut.MaterialData = float4(Shading.BaseLayer.Metallic, Shading.BaseLayer.Srf.PerceptualRoughness, 0.0, 1.0);

#   if ENABLE_CLEAR_COAT
	{
        PSOut.Normal.xyz      = lerp(PSOut.Normal.xyz, Shading.Clearcoat.Normal, Shading.Clearcoat.Factor);
        PSOut.MaterialData.xy = lerp(PSOut.MaterialData.xy, float2(0.0, Shading.Clearcoat.Srf.PerceptualRoughness), Shading.Clearcoat.Factor);
	}
#   endif

    PSOut.MotionVec = float4(0.0, 0.0, 0.0, 1.0);
    return PSOut;
)";

    return PSMainInfo;
}

void GLTFViewer::CreateGLTFRenderer()
{
    GLTF_PBR_Renderer::CreateInfo RendererCI;

    RendererCI.EnableClearCoat       = true;
    RendererCI.EnableSheen           = true;
    RendererCI.EnableIridescence     = true;
    RendererCI.EnableTransmission    = true;
    RendererCI.EnableAnisotropy      = true;
    RendererCI.FrontCounterClockwise = true;
    if (m_bEnablePostProcessing)
        RendererCI.GetPSMainSource = GetPbrPSMainSource;

    RendererCI.SheenAlbedoScalingLUTPath    = "textures/sheen_albedo_scaling.jpg";
    RendererCI.PreintegratedCharlieBRDFPath = "textures/charlie_preintegrated.jpg";

    m_RenderParams.Flags =
        GLTF_PBR_Renderer::PSO_FLAG_DEFAULT |
        GLTF_PBR_Renderer::PSO_FLAG_ENABLE_CLEAR_COAT |
        GLTF_PBR_Renderer::PSO_FLAG_ALL_TEXTURES |
        GLTF_PBR_Renderer::PSO_FLAG_ENABLE_SHEEN |
        GLTF_PBR_Renderer::PSO_FLAG_ENABLE_ANISOTROPY |
        GLTF_PBR_Renderer::PSO_FLAG_ENABLE_IRIDESCENCE |
        GLTF_PBR_Renderer::PSO_FLAG_ENABLE_TRANSMISSION |
        GLTF_PBR_Renderer::PSO_FLAG_ENABLE_VOLUME |
        GLTF_PBR_Renderer::PSO_FLAG_ENABLE_TEXCOORD_TRANSFORM;
    if (m_bUseResourceCache)
        m_RenderParams.Flags |= GLTF_PBR_Renderer::PSO_FLAG_USE_TEXTURE_ATLAS;

    if (m_bEnablePostProcessing)
    {
        RendererCI.NumRenderTargets = GBUFFER_RT_DEPTH;
        for (Uint32 i = 0; i < RendererCI.NumRenderTargets; ++i)
            RendererCI.RTVFormats[i] = m_GBuffer->GetElementDesc(i).Format;
        RendererCI.DSVFormat = m_GBuffer->GetElementDesc(GBUFFER_RT_DEPTH).Format;
    }
    else
    {
        RendererCI.NumRenderTargets = 1;
        RendererCI.RTVFormats[0]    = m_pSwapChain->GetDesc().ColorBufferFormat;
        RendererCI.DSVFormat        = m_pSwapChain->GetDesc().DepthBufferFormat;

        if (RendererCI.RTVFormats[0] == TEX_FORMAT_RGBA8_UNORM || RendererCI.RTVFormats[0] == TEX_FORMAT_BGRA8_UNORM)
            m_RenderParams.Flags |= GLTF_PBR_Renderer::PSO_FLAG_CONVERT_OUTPUT_TO_SRGB;
    }

    m_GLTFRenderer = std::make_unique<GLTF_PBR_Renderer>(m_pDevice, nullptr, m_pImmediateContext, RendererCI);

    if (m_bUseResourceCache)
    {
        if (!m_pResourceMgr)
        {
            CreateGLTFResourceCache();
        }
    }
    else
    {
        m_pResourceMgr.Release();
        m_CacheUseInfo = {};
    }

    // Reset environment map in GLTF renderer
    if (m_pCurrentEnvMapSRV != nullptr)
    {
        auto* pEnvMap       = m_pCurrentEnvMapSRV;
        m_pCurrentEnvMapSRV = nullptr;
        SetEnvironmentMap(pEnvMap);
    }
}

void GLTFViewer::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    m_bWireframeSupported = m_pDevice->GetDeviceInfo().Features.WireframeFill;

    RefCntAutoPtr<ITexture> EnvironmentMap;
    CreateTextureFromFile("textures/papermill.ktx", TextureLoadInfo{"Environment map"}, m_pDevice, &EnvironmentMap);
    m_EnvironmentMapSRV = EnvironmentMap->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);

    m_WhiteFurnaceEnvMapSRV = CreateWhiteFurnaceEnvMap(m_pDevice);

    {
        GBuffer::ElementDesc GBufferElems[GBUFFER_RT_COUNT];
        GBufferElems[GBUFFER_RT_COLOR]          = {TEX_FORMAT_RGBA16_FLOAT};
        GBufferElems[GBUFFER_RT_NORMAL]         = {TEX_FORMAT_RGBA16_FLOAT};
        GBufferElems[GBUFFER_RT_MATERIAL_DATA]  = {TEX_FORMAT_RG8_UNORM};
        GBufferElems[GBUFFER_RT_MOTION_VECTORS] = {TEX_FORMAT_RG16_FLOAT};
        GBufferElems[GBUFFER_RT_DEPTH]          = {TEX_FORMAT_D32_FLOAT};
        static_assert(GBUFFER_RT_COUNT == 5, "Not all G-buffer elements are initialized");

        m_GBuffer = std::make_unique<GBuffer>(GBufferElems, _countof(GBufferElems));
    }

    CreateGLTFRenderer();

    CreateUniformBuffer(m_pDevice, sizeof(HLSL::PBRFrameAttribs), "PBR frame attribs buffer", &m_FrameAttribsCB);
    // clang-format off
    StateTransitionDesc Barriers [] =
    {
        {m_FrameAttribsCB, RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_CONSTANT_BUFFER, STATE_TRANSITION_FLAG_UPDATE_STATE},
        {EnvironmentMap,   RESOURCE_STATE_UNKNOWN, RESOURCE_STATE_SHADER_RESOURCE, STATE_TRANSITION_FLAG_UPDATE_STATE}
    };
    // clang-format on
    m_pImmediateContext->TransitionResourceStates(_countof(Barriers), Barriers);

    RefCntAutoPtr<IRenderStateNotationParser> pRSNParser;
    {
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pStreamFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory("render_states", &pStreamFactory);

        CreateRenderStateNotationParser({}, &pRSNParser);
        pRSNParser->ParseFile("RenderStates.json", pStreamFactory);
    }

    RefCntAutoPtr<IRenderStateNotationLoader> pRSNLoader;
    {
        RefCntAutoPtr<IShaderSourceInputStreamFactory> pStreamFactory;
        m_pEngineFactory->CreateDefaultShaderSourceStreamFactory("shaders", &pStreamFactory);
        CreateRenderStateNotationLoader({m_pDevice, pRSNParser, pStreamFactory}, &pRSNLoader);
    }

    {
        EnvMapRenderer::CreateInfo EnvMapRendererCI;
        EnvMapRendererCI.pDevice          = m_pDevice;
        EnvMapRendererCI.pCameraAttribsCB = m_FrameAttribsCB;
        EnvMapRendererCI.NumRenderTargets = 1;
        EnvMapRendererCI.RTVFormats[0]    = m_pSwapChain->GetDesc().ColorBufferFormat;
        EnvMapRendererCI.DSVFormat        = m_pSwapChain->GetDesc().DepthBufferFormat;

        m_EnvMapRenderer = std::make_unique<EnvMapRenderer>(EnvMapRendererCI);
    }

    CreateBoundBoxPSO(pRSNLoader);

    m_LightDirection = normalize(float3(0.5f, 0.6f, -0.2f));

    if (m_Models.empty())
    {
        // ProcessCommandLine is not called on all platforms, so we need to initialize the models list.
        UpdateModelsList("");
    }
    LoadModel(!m_ModelPath.empty() ? m_ModelPath.c_str() : m_Models[m_SelectedModel].Path.c_str());
}

void GLTFViewer::ApplyPosteffects::Initialize(IRenderDevice* pDevice, TEXTURE_FORMAT RTVFormat)
{
    ShaderCreateInfo ShaderCI;
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    pDevice->GetEngineFactory()->CreateDefaultShaderSourceStreamFactory("shaders", &pShaderSourceFactory);
    ShaderCI.pShaderSourceStreamFactory = pShaderSourceFactory;

    ShaderMacroHelper Macros;
    Macros.Add("CONVERT_OUTPUT_TO_SRGB", RTVFormat == TEX_FORMAT_RGBA8_UNORM || RTVFormat == TEX_FORMAT_BGRA8_UNORM);
    //Macros.Add("TONE_MAPPING_MODE", m_Params.ToneMappingMode);
    ShaderCI.Macros = Macros;

    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc       = {"Full Screen Triangle VS", SHADER_TYPE_VERTEX, true};
        ShaderCI.EntryPoint = "FullScreenTriangleVS";
        ShaderCI.FilePath   = "FullScreenTriangleVS.fx";

        pDevice->CreateShader(ShaderCI, &pVS);
        VERIFY_EXPR(pVS);
    }


    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc       = {"Apply Post Effects PS", SHADER_TYPE_PIXEL, true};
        ShaderCI.EntryPoint = "main";
        ShaderCI.FilePath   = "ApplyPostEffects.psh";

        pDevice->CreateShader(ShaderCI, &pPS);
        VERIFY_EXPR(pPS);
    }

    PipelineResourceLayoutDescX ResourceLauout;
    ResourceLauout
        .SetDefaultVariableType(SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC);
    //    .AddVariable(SHADER_TYPE_PIXEL, "cbPostProcessAttribs", SHADER_RESOURCE_VARIABLE_TYPE_STATIC);

    GraphicsPipelineStateCreateInfoX PsoCI{"Apply Post Effects"};
    PsoCI
        .AddRenderTarget(RTVFormat)
        .AddShader(pVS)
        .AddShader(pPS)
        .SetDepthStencilDesc(DSS_DisableDepth)
        .SetRasterizerDesc(RS_SolidFillNoCull)
        .SetResourceLayout(ResourceLauout)
        .SetPrimitiveTopology(PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

    pDevice->CreatePipelineState(PsoCI, &pPSO);
    VERIFY_EXPR(pPSO);
    pPSO->CreateShaderResourceBinding(&pSRB, true);
    ptex2DColorVar = pSRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_tex2DColor");
    VERIFY_EXPR(ptex2DColorVar != nullptr);
    //m_PSO->GetStaticVariableByName(SHADER_TYPE_PIXEL, "cbPostProcessAttribs")->Set(m_PostProcessAttribsCB);
}

void GLTFViewer::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        {
            m_ModelNames.resize(m_Models.size());
            for (size_t i = 0; i < m_Models.size(); ++i)
                m_ModelNames[i] = m_Models[i].Name.c_str();

            if (ImGui::Combo("Model", &m_SelectedModel, m_ModelNames.data(), static_cast<int>(m_ModelNames.size()), 20))
            {
                LoadModel(m_Models[m_SelectedModel].Path.c_str());
            }
        }
#ifdef PLATFORM_WIN32
        if (ImGui::Button("Load model"))
        {
            FileDialogAttribs OpenDialogAttribs{FILE_DIALOG_TYPE_OPEN};
            OpenDialogAttribs.Title  = "Select GLTF file";
            OpenDialogAttribs.Filter = "glTF files\0*.gltf;*.glb\0";
            auto FileName            = FileSystem::FileDialog(OpenDialogAttribs);
            if (!FileName.empty())
            {
                LoadModel(FileName.c_str());
            }
        }
#endif
        if (m_Model->Scenes.size() > 1)
        {
            std::vector<std::pair<Uint32, std::string>> SceneList;
            SceneList.reserve(m_Model->Scenes.size());
            for (Uint32 i = 0; i < m_Model->Scenes.size(); ++i)
            {
                SceneList.emplace_back(i, !m_Model->Scenes[i].Name.empty() ? m_Model->Scenes[i].Name : std::to_string(i));
            }
            if (ImGui::Combo("Scene", &m_RenderParams.SceneIndex, SceneList.data(), static_cast<int>(SceneList.size())))
                UpdateScene();
        }

        if (!m_CameraNodes.empty())
        {
            std::vector<std::pair<Uint32, std::string>> CamList;
            CamList.emplace_back(0, "default");
            for (Uint32 i = 0; i < m_CameraNodes.size(); ++i)
            {
                const auto& Cam = *m_CameraNodes[i]->pCamera;
                CamList.emplace_back(i + 1, Cam.Name.empty() ? std::to_string(i) : Cam.Name);
            }

            if (ImGui::Combo("Camera", &m_CameraId, CamList.data(), static_cast<int>(CamList.size())))
                m_bResetPrevCamera = true;
        }

        if (m_CameraId == 0)
        {
            auto ModelRotation = m_Camera.GetSecondaryRotation();
            if (ImGui::gizmo3D("Model Rotation", ModelRotation, ImGui::GetTextLineHeight() * 10))
                m_Camera.SetSecondaryRotation(ModelRotation);
            ImGui::SameLine();
            ImGui::gizmo3D("Light direction", m_LightDirection, ImGui::GetTextLineHeight() * 10);

            if (ImGui::Button("Reset view"))
            {
                m_Camera.ResetDefaults();
            }

            auto CameraDist = m_Camera.GetDist();
            if (ImGui::SliderFloat("Camera distance", &CameraDist, m_Camera.GetMinDist(), m_Camera.GetMaxDist()))
                m_Camera.SetDist(CameraDist);
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::TreeNode("Lighting"))
        {
            ImGui::ColorEdit3("Light Color", &m_LightColor.r);
            // clang-format off
            ImGui::SliderFloat("Light Intensity",    &m_LightIntensity,                  0.f, 50.f);
            ImGui::SliderFloat("Occlusion strength", &m_ShaderAttribs.OcclusionStrength, 0.f,  1.f);
            ImGui::SliderFloat("Emission scale",     &m_ShaderAttribs.EmissionScale,     0.f,  1.f);
            ImGui::SliderFloat("IBL scale",          &m_ShaderAttribs.IBLScale,          0.f,  1.f);
            // clang-format on
            ImGui::TreePop();
        }

        if (!m_Model->Animations.empty())
        {
            ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
            if (ImGui::TreeNode("Animation"))
            {
                ImGui::Checkbox("Play", &m_PlayAnimation);
                std::vector<const char*> Animations(m_Model->Animations.size());
                for (size_t i = 0; i < m_Model->Animations.size(); ++i)
                    Animations[i] = m_Model->Animations[i].Name.c_str();
                ImGui::Combo("Active Animation", reinterpret_cast<int*>(&m_AnimationIndex), Animations.data(), static_cast<int>(Animations.size()));
                ImGui::TreePop();
            }
        }

        ImGui::SetNextItemOpen(true, ImGuiCond_FirstUseEver);
        if (ImGui::TreeNode("Tone mapping"))
        {
            // clang-format off
            ImGui::SliderFloat("Average log lum", &m_ShaderAttribs.AverageLogLum, 0.01f, 10.0f);
            ImGui::SliderFloat("Middle gray",     &m_ShaderAttribs.MiddleGray,    0.01f,  1.0f);
            ImGui::SliderFloat("White point",     &m_ShaderAttribs.WhitePoint,    0.1f,  20.0f);
            // clang-format on
            ImGui::TreePop();
        }

        {
            std::array<const char*, static_cast<size_t>(BackgroundMode::NumModes)> BackgroundModes;
            BackgroundModes[static_cast<size_t>(BackgroundMode::None)]              = "None";
            BackgroundModes[static_cast<size_t>(BackgroundMode::EnvironmentMap)]    = "Environmen Map";
            BackgroundModes[static_cast<size_t>(BackgroundMode::Irradiance)]        = "Irradiance";
            BackgroundModes[static_cast<size_t>(BackgroundMode::PrefilteredEnvMap)] = "PrefilteredEnvMap";
            ImGui::Combo("Background mode", reinterpret_cast<int*>(&m_BackgroundMode), BackgroundModes.data(), static_cast<int>(BackgroundModes.size()));
        }

        ImGui::SliderFloat("Env map mip", &m_EnvMapMipLevel, 0.0f, 7.0f);

        {
            std::pair<GLTF_PBR_Renderer::DebugViewType, const char*> DebugViews[] = {
                {GLTF_PBR_Renderer::DebugViewType::None, "None"},
                {GLTF_PBR_Renderer::DebugViewType::Texcoord0, "Tex coords 0"},
                {GLTF_PBR_Renderer::DebugViewType::Texcoord1, "Tex coords 1"},
                {GLTF_PBR_Renderer::DebugViewType::BaseColor, "Base Color"},
                {GLTF_PBR_Renderer::DebugViewType::Transparency, "Transparency"},
                {GLTF_PBR_Renderer::DebugViewType::Occlusion, "Occlusion"},
                {GLTF_PBR_Renderer::DebugViewType::Emissive, "Emissive"},
                {GLTF_PBR_Renderer::DebugViewType::Metallic, "Metallic"},
                {GLTF_PBR_Renderer::DebugViewType::Roughness, "Roughness"},
                {GLTF_PBR_Renderer::DebugViewType::DiffuseColor, "Diffuse color"},
                {GLTF_PBR_Renderer::DebugViewType::SpecularColor, "Specular color (R0)"},
                {GLTF_PBR_Renderer::DebugViewType::Reflectance90, "Reflectance90"},
                {GLTF_PBR_Renderer::DebugViewType::MeshNormal, "Mesh normal"},
                {GLTF_PBR_Renderer::DebugViewType::ShadingNormal, "Shading normal"},
                {GLTF_PBR_Renderer::DebugViewType::MotionVectors, "Motion Vectors"},
                {GLTF_PBR_Renderer::DebugViewType::NdotV, "n*v"},
                {PBR_Renderer::DebugViewType::PunctualLighting, "Punctual Lighting"},
                {GLTF_PBR_Renderer::DebugViewType::DiffuseIBL, "Diffuse IBL"},
                {GLTF_PBR_Renderer::DebugViewType::SpecularIBL, "Specular IBL"},
                {GLTF_PBR_Renderer::DebugViewType::ClearCoat, "Clear Coat"},
                {GLTF_PBR_Renderer::DebugViewType::ClearCoatFactor, "Clear Coat Factor"},
                {GLTF_PBR_Renderer::DebugViewType::ClearCoatRoughness, "Clear Coat Roughness"},
                {GLTF_PBR_Renderer::DebugViewType::ClearCoatNormal, "Clear Coat Normal"},
                {GLTF_PBR_Renderer::DebugViewType::Sheen, "Sheen"},
                {GLTF_PBR_Renderer::DebugViewType::SheenColor, "Sheen Color"},
                {GLTF_PBR_Renderer::DebugViewType::SheenRoughness, "Sheen Roughness"},
                {GLTF_PBR_Renderer::DebugViewType::AnisotropyStrength, "Anisotropy Strength"},
                {GLTF_PBR_Renderer::DebugViewType::AnisotropyDirection, "Anisotropy Direction"},
                {GLTF_PBR_Renderer::DebugViewType::Iridescence, "Iridescence"},
                {GLTF_PBR_Renderer::DebugViewType::IridescenceFactor, "Iridescence Factor"},
                {GLTF_PBR_Renderer::DebugViewType::IridescenceThickness, "Iridescence Thickness"},
                {GLTF_PBR_Renderer::DebugViewType::Transmission, "Transmission"},
                {GLTF_PBR_Renderer::DebugViewType::Thickness, "Volume Thickness"},
            };
            static_assert(_countof(DebugViews) == 33, "Did you add a new debug view mode? You may want to handle it here");

            ImGui::Combo("Debug view", &m_RenderParams.DebugView, DebugViews, _countof(DebugViews), 15);
        }

        ImGui::Combo("Bound box mode", reinterpret_cast<int*>(&m_BoundBoxMode),
                     "None\0"
                     "Local\0"
                     "Global\0\0");

        if (m_bWireframeSupported)
        {
            ImGui::Checkbox("Wireframe", &m_RenderParams.Wireframe);
        }

        if (ImGui::TreeNode("Renderer Features"))
        {
            auto FeatureCheckbox = [&](const char* Name, GLTF_PBR_Renderer::PSO_FLAGS Flag) {
                bool Enabled = (m_RenderParams.Flags & Flag) != 0;
                if (ImGui::Checkbox(Name, &Enabled))
                {
                    if (Enabled)
                        m_RenderParams.Flags |= Flag;
                    else
                        m_RenderParams.Flags &= ~Flag;
                }
            };
            FeatureCheckbox("Vertex Colors", GLTF_PBR_Renderer::PSO_FLAG_USE_VERTEX_COLORS);
            FeatureCheckbox("Vertex Normals", GLTF_PBR_Renderer::PSO_FLAG_USE_VERTEX_NORMALS);
            FeatureCheckbox("Texcoords", GLTF_PBR_Renderer::PSO_FLAG_USE_TEXCOORD0 | GLTF_PBR_Renderer::PSO_FLAG_USE_TEXCOORD1);
            FeatureCheckbox("Joints", GLTF_PBR_Renderer::PSO_FLAG_USE_JOINTS);
            FeatureCheckbox("Clear coat", GLTF_PBR_Renderer::PSO_FLAG_ENABLE_CLEAR_COAT);
            FeatureCheckbox("Sheen", GLTF_PBR_Renderer::PSO_FLAG_ENABLE_SHEEN);
            FeatureCheckbox("Anisotropy", GLTF_PBR_Renderer::PSO_FLAG_ENABLE_ANISOTROPY);
            FeatureCheckbox("Iridescence", GLTF_PBR_Renderer::PSO_FLAG_ENABLE_IRIDESCENCE);
            FeatureCheckbox("Transmission", GLTF_PBR_Renderer::PSO_FLAG_ENABLE_TRANSMISSION);
            FeatureCheckbox("Volume", GLTF_PBR_Renderer::PSO_FLAG_ENABLE_VOLUME);
            FeatureCheckbox("Color map", GLTF_PBR_Renderer::PSO_FLAG_USE_COLOR_MAP);
            FeatureCheckbox("Normal map", GLTF_PBR_Renderer::PSO_FLAG_USE_NORMAL_MAP);
            FeatureCheckbox("Phys desc map", GLTF_PBR_Renderer::PSO_FLAG_USE_PHYS_DESC_MAP);
            FeatureCheckbox("Occlusion", GLTF_PBR_Renderer::PSO_FLAG_USE_AO_MAP);
            FeatureCheckbox("Emissive", GLTF_PBR_Renderer::PSO_FLAG_USE_EMISSIVE_MAP);
            {
                ImGui::ScopedDisabler Disable{(m_RenderParams.Flags & GLTF_PBR_Renderer::PSO_FLAG_ENABLE_CLEAR_COAT) == 0, 0.5f};
                FeatureCheckbox("Clear coat map", GLTF_PBR_Renderer::PSO_FLAG_USE_CLEAR_COAT_MAP);
                FeatureCheckbox("Clear coat roughness map", GLTF_PBR_Renderer::PSO_FLAG_USE_CLEAR_COAT_ROUGHNESS_MAP);
                FeatureCheckbox("Clear coat normal map", GLTF_PBR_Renderer::PSO_FLAG_USE_CLEAR_COAT_NORMAL_MAP);
            }
            {
                ImGui::ScopedDisabler Disable{(m_RenderParams.Flags & GLTF_PBR_Renderer::PSO_FLAG_ENABLE_SHEEN) == 0, 0.5f};
                FeatureCheckbox("Sheen color map", GLTF_PBR_Renderer::PSO_FLAG_USE_SHEEN_COLOR_MAP);
                FeatureCheckbox("Sheen roughness map", GLTF_PBR_Renderer::PSO_FLAG_USE_SHEEN_ROUGHNESS_MAP);
            }
            {
                ImGui::ScopedDisabler Disable{(m_RenderParams.Flags & GLTF_PBR_Renderer::PSO_FLAG_ENABLE_ANISOTROPY) == 0, 0.5f};
                FeatureCheckbox("Anisotropy map", GLTF_PBR_Renderer::PSO_FLAG_USE_ANISOTROPY_MAP);
            }
            {
                ImGui::ScopedDisabler Disable{(m_RenderParams.Flags & GLTF_PBR_Renderer::PSO_FLAG_ENABLE_IRIDESCENCE) == 0, 0.5f};
                FeatureCheckbox("Iridescence map", GLTF_PBR_Renderer::PSO_FLAG_USE_IRIDESCENCE_MAP);
                FeatureCheckbox("Iridescence thickness map", GLTF_PBR_Renderer::PSO_FLAG_USE_IRIDESCENCE_THICKNESS_MAP);
            }
            {
                ImGui::ScopedDisabler Disable{(m_RenderParams.Flags & GLTF_PBR_Renderer::PSO_FLAG_ENABLE_TRANSMISSION) == 0, 0.5f};
                FeatureCheckbox("Transmission map", GLTF_PBR_Renderer::PSO_FLAG_USE_TRANSMISSION_MAP);
            }
            {
                ImGui::ScopedDisabler Disable{(m_RenderParams.Flags & GLTF_PBR_Renderer::PSO_FLAG_ENABLE_VOLUME) == 0, 0.5f};
                FeatureCheckbox("Thickness map", GLTF_PBR_Renderer::PSO_FLAG_USE_THICKNESS_MAP);
            }
            FeatureCheckbox("IBL", GLTF_PBR_Renderer::PSO_FLAG_USE_IBL);
            FeatureCheckbox("Tone Mapping", GLTF_PBR_Renderer::PSO_FLAG_ENABLE_TONE_MAPPING);
            FeatureCheckbox("UV transform", GLTF_PBR_Renderer::PSO_FLAG_ENABLE_TEXCOORD_TRANSFORM);
            FeatureCheckbox("Motion Vectors", GLTF_PBR_Renderer::PSO_FLAG_COMPUTE_MOTION_VECTORS);

            ImGui::Separator();

            if (ImGui::Checkbox("Resource cache", &m_bUseResourceCache))
            {
                CreateGLTFRenderer();
                LoadModel(m_ModelPath.c_str());
            }
            if (ImGui::Checkbox("Post processing", &m_bEnablePostProcessing))
            {
                CreateGLTFRenderer();
            }

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Alpha Modes"))
        {
            auto AlphaModeCheckbox = [&](const char* Name, GLTF_PBR_Renderer::RenderInfo::ALPHA_MODE_FLAGS Flag) {
                bool Enabled = (m_RenderParams.AlphaModes & Flag) != 0;
                if (ImGui::Checkbox(Name, &Enabled))
                {
                    if (Enabled)
                        m_RenderParams.AlphaModes |= Flag;
                    else
                        m_RenderParams.AlphaModes &= ~Flag;
                }
            };
            AlphaModeCheckbox("Opaque", GLTF_PBR_Renderer::RenderInfo::ALPHA_MODE_FLAG_OPAQUE);
            AlphaModeCheckbox("Mask", GLTF_PBR_Renderer::RenderInfo::ALPHA_MODE_FLAG_MASK);
            AlphaModeCheckbox("Blend", GLTF_PBR_Renderer::RenderInfo::ALPHA_MODE_FLAG_BLEND);
            ImGui::TreePop();
        }
    }
    ImGui::End();
}

void GLTFViewer::CreateBoundBoxPSO(IRenderStateNotationLoader* pRSNLoader)
{
    auto ModifyCI = MakeCallback([this](PipelineStateCreateInfo& PipelineCI) {
        auto& GraphicsPipelineCI{static_cast<GraphicsPipelineStateCreateInfo&>(PipelineCI)};
        GraphicsPipelineCI.GraphicsPipeline.RTVFormats[0]    = m_pSwapChain->GetDesc().ColorBufferFormat;
        GraphicsPipelineCI.GraphicsPipeline.DSVFormat        = m_pSwapChain->GetDesc().DepthBufferFormat;
        GraphicsPipelineCI.GraphicsPipeline.NumRenderTargets = 1;
    });
    pRSNLoader->LoadPipelineState({"BoundBox PSO", PIPELINE_TYPE_GRAPHICS, true, ModifyCI, ModifyCI}, &m_BoundBoxPSO);

    m_BoundBoxPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "cbCameraAttribs")->Set(m_FrameAttribsCB);
    m_BoundBoxPSO->CreateShaderResourceBinding(&m_BoundBoxSRB, true);
}

GLTFViewer::~GLTFViewer()
{
}

// Render a frame
void GLTFViewer::Render()
{
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();

    if (m_bEnablePostProcessing)
    {
        if (!m_ApplyPostFX)
        {
            m_ApplyPostFX.Initialize(m_pDevice, m_pSwapChain->GetDesc().ColorBufferFormat);
        }

        const auto& SCDesc = m_pSwapChain->GetDesc();
        m_GBuffer->Resize(m_pDevice, SCDesc.Width, SCDesc.Height);
        m_GBuffer->Bind(m_pImmediateContext, GBUFFER_RT_FLAG_ALL, nullptr, GBUFFER_RT_FLAG_ALL);
    }
    else
    {
        // Clear the back buffer
        const float ClearColor[] = {0.032f, 0.032f, 0.032f, 1.0f};
        m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    }



    {
        MapHelper<HLSL::PBRFrameAttribs> FrameAttribs{m_pImmediateContext, m_FrameAttribsCB, MAP_WRITE, MAP_FLAG_DISCARD};
        FrameAttribs->Camera     = m_CameraAttribs[m_CurrentFrameNumber & 0x01];
        FrameAttribs->PrevCamera = m_CameraAttribs[(m_CurrentFrameNumber + 1) & 0x01];

        {
            if (m_BoundBoxMode != BoundBoxMode::None)
            {
                float4x4 BBTransform;
                if (m_BoundBoxMode == BoundBoxMode::Local)
                {
                    BBTransform =
                        float4x4::Scale(m_ModelAABB.Max - m_ModelAABB.Min) *
                        float4x4::Translation(m_ModelAABB.Min) *
                        m_RenderParams.ModelTransform;
                }
                else if (m_BoundBoxMode == BoundBoxMode::Global)
                {
                    auto TransformedBB = m_ModelAABB.Transform(m_RenderParams.ModelTransform);
                    BBTransform        = float4x4::Scale(TransformedBB.Max - TransformedBB.Min) * float4x4::Translation(TransformedBB.Min);
                }
                else
                {
                    UNEXPECTED("Unexpected bound box mode");
                }

                for (int row = 0; row < 4; ++row)
                    FrameAttribs->Camera.f4ExtraData[row] = float4::MakeVector(BBTransform[row]);
            }
        }
        {
            auto& Light     = FrameAttribs->Light;
            Light.Direction = m_LightDirection;
            Light.Intensity = m_LightColor * m_LightIntensity;
        }
        {
            auto& Renderer = FrameAttribs->Renderer;
            m_GLTFRenderer->SetInternalShaderParameters(Renderer);

            Renderer.OcclusionStrength = m_ShaderAttribs.OcclusionStrength;
            Renderer.EmissionScale     = m_ShaderAttribs.EmissionScale;
            Renderer.AverageLogLum     = m_ShaderAttribs.AverageLogLum;
            Renderer.MiddleGray        = m_ShaderAttribs.MiddleGray;
            Renderer.WhitePoint        = m_ShaderAttribs.WhitePoint;
            Renderer.IBLScale          = m_ShaderAttribs.IBLScale;
            Renderer.HighlightColor    = m_ShaderAttribs.HighlightColor;
            Renderer.UnshadedColor     = m_ShaderAttribs.WireframeColor;
            Renderer.PointSize         = 1;
        }
    }

    if (m_pResourceMgr)
    {
        m_GLTFRenderer->Begin(m_pDevice, m_pImmediateContext, m_CacheUseInfo, m_CacheBindings, m_FrameAttribsCB);
        m_GLTFRenderer->Render(m_pImmediateContext, *m_Model, m_Transforms[m_CurrentFrameNumber & 0x01], &m_Transforms[(m_CurrentFrameNumber + 1) & 0x01], m_RenderParams, nullptr, &m_CacheBindings);
    }
    else
    {
        m_GLTFRenderer->Begin(m_pImmediateContext);
        m_GLTFRenderer->Render(m_pImmediateContext, *m_Model, m_Transforms[m_CurrentFrameNumber & 0x01], &m_Transforms[(m_CurrentFrameNumber + 1) & 0x01], m_RenderParams, &m_ModelResourceBindings);
    }

    if (m_BoundBoxMode != BoundBoxMode::None)
    {
        m_pImmediateContext->SetPipelineState(m_BoundBoxPSO);
        m_pImmediateContext->CommitShaderResources(m_BoundBoxSRB, RESOURCE_STATE_TRANSITION_MODE_VERIFY);
        DrawAttribs DrawAttrs{24, DRAW_FLAG_VERIFY_ALL};
        m_pImmediateContext->Draw(DrawAttrs);
    }

    if (m_BackgroundMode != BackgroundMode::None)
    {
        ITextureView* pEnvMapSRV = nullptr;
        switch (m_BackgroundMode)
        {
            case BackgroundMode::EnvironmentMap:
                pEnvMapSRV = m_pCurrentEnvMapSRV;
                break;

            case BackgroundMode::Irradiance:
                pEnvMapSRV = m_GLTFRenderer->GetIrradianceCubeSRV();
                break;

            case BackgroundMode::PrefilteredEnvMap:
                pEnvMapSRV = m_GLTFRenderer->GetPrefilteredEnvMapSRV();
                break;

            default:
                UNEXPECTED("Unexpected background mode");
        }

        HLSL::ToneMappingAttribs TMAttribs;
        TMAttribs.iToneMappingMode     = (m_RenderParams.Flags & GLTF_PBR_Renderer::PSO_FLAG_ENABLE_TONE_MAPPING) ? TONE_MAPPING_MODE_UNCHARTED2 : TONE_MAPPING_MODE_NONE;
        TMAttribs.bAutoExposure        = 0;
        TMAttribs.fMiddleGray          = m_ShaderAttribs.MiddleGray;
        TMAttribs.bLightAdaptation     = 0;
        TMAttribs.fWhitePoint          = m_ShaderAttribs.WhitePoint;
        TMAttribs.fLuminanceSaturation = 1.0;

        EnvMapRenderer::RenderAttribs EnvMapAttribs;
        EnvMapAttribs.pContext            = m_pImmediateContext;
        EnvMapAttribs.pEnvMap             = pEnvMapSRV;
        EnvMapAttribs.AverageLogLum       = m_ShaderAttribs.AverageLogLum;
        EnvMapAttribs.MipLevel            = m_EnvMapMipLevel;
        EnvMapAttribs.ConvertOutputToSRGB = (m_RenderParams.Flags & GLTF_PBR_Renderer::PSO_FLAG_CONVERT_OUTPUT_TO_SRGB) != 0;

        m_EnvMapRenderer->Render(EnvMapAttribs, TMAttribs);
    }

    if (m_bEnablePostProcessing)
    {
        m_pImmediateContext->SetRenderTargets(1, &pRTV, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        // Clear the back buffer
        const float ClearColor[] = {0.032f, 0.032f, 0.032f, 1.0f};
        m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

        m_pImmediateContext->SetPipelineState(m_ApplyPostFX.pPSO);
        m_ApplyPostFX.ptex2DColorVar->Set(m_GBuffer->GetBuffer(GBUFFER_RT_COLOR)->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE));
        m_pImmediateContext->CommitShaderResources(m_ApplyPostFX.pSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
        m_pImmediateContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
    }
}

void GLTFViewer::Update(double CurrTime, double ElapsedTime)
{
    if (m_CameraId == 0)
    {
        m_Camera.Update(m_InputController);
    }

    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();

    float YFov  = PI_F / 4.0f;
    float ZNear = 0.1f;
    float ZFar  = 100.f;

    float4x4 CameraView;
    if (m_CameraId == 0)
    {
        CameraView = m_Camera.GetRotation().ToMatrix() * float4x4::Translation(0.f, 0.0f, m_Camera.GetDist());

        m_RenderParams.ModelTransform = m_Camera.GetSecondaryRotation().ToMatrix();
    }
    else
    {
        const auto* pCameraNode           = m_CameraNodes[m_CameraId - 1];
        const auto* pCamera               = pCameraNode->pCamera;
        const auto& CameraGlobalTransform = m_Transforms[m_CurrentFrameNumber & 0x01].NodeGlobalMatrices[pCameraNode->Index];

        // GLTF camera is defined such that the local +X axis is to the right,
        // the lens looks towards the local -Z axis, and the top of the camera
        // is aligned with the local +Y axis.
        // https://github.com/KhronosGroup/glTF/tree/master/specification/2.0#cameras
        // We need to inverse the Z axis as our camera looks towards +Z.
        float4x4 InvZAxis = float4x4::Identity();
        InvZAxis._33      = -1;

        CameraView = CameraGlobalTransform.Inverse() * InvZAxis;
        YFov       = pCamera->Perspective.YFov;
        ZNear      = pCamera->Perspective.ZNear;
        ZFar       = pCamera->Perspective.ZFar;

        m_RenderParams.ModelTransform = float4x4::Identity();
    }

    // Apply pretransform matrix that rotates the scene according the surface orientation
    CameraView *= GetSurfacePretransformMatrix(float3{0, 0, 1});

    float4x4 CameraWorld = CameraView.Inverse();

    // Get projection matrix adjusted to the current screen orientation
    const auto CameraProj     = GetAdjustedProjectionMatrix(YFov, ZNear, ZFar);
    const auto CameraViewProj = CameraView * CameraProj;

    float3 CameraWorldPos = float3::MakeVector(CameraWorld[3]);

    auto& CurrCamAttribs = m_CameraAttribs[m_CurrentFrameNumber & 0x01];

    const auto& SCDesc = m_pSwapChain->GetDesc();

    CurrCamAttribs.f4ViewportSize = float4{static_cast<float>(SCDesc.Width), static_cast<float>(SCDesc.Height), 1.f / SCDesc.Width, 1.f / SCDesc.Height};
    CurrCamAttribs.mViewT         = CameraView.Transpose();
    CurrCamAttribs.mProjT         = CameraProj.Transpose();
    CurrCamAttribs.mViewProjT     = CameraViewProj.Transpose();
    CurrCamAttribs.mViewProjInvT  = CameraViewProj.Inverse().Transpose();
    CurrCamAttribs.f4Position     = float4(CameraWorldPos, 1);

    if (m_bResetPrevCamera)
    {
        m_CameraAttribs[(m_CurrentFrameNumber + 1) & 0x01] = CurrCamAttribs;
    }

    if (!m_Model->Animations.empty() && m_PlayAnimation)
    {
        float& AnimationTimer = m_AnimationTimers[m_AnimationIndex];
        AnimationTimer += static_cast<float>(ElapsedTime);
        AnimationTimer = std::fmod(AnimationTimer, m_Model->Animations[m_AnimationIndex].End);

        m_Model->ComputeTransforms(m_RenderParams.SceneIndex, m_Transforms[m_CurrentFrameNumber & 0x01], m_ModelTransform, m_AnimationIndex, AnimationTimer);
        if (m_bResetPrevCamera)
        {
            m_Transforms[(m_CurrentFrameNumber + 1) & 0x01] = m_Transforms[m_CurrentFrameNumber & 0x01];
        }
    }

    m_bResetPrevCamera = false;
}

} // namespace Diligent
