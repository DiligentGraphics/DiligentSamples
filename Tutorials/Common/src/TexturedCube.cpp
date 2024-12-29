/*
 *  Copyright 2019-2024 Diligent Graphics LLC
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

#include <vector>

#include "TexturedCube.hpp"
#include "BasicMath.hpp"
#include "TextureUtilities.h"
#include "GraphicsTypesX.hpp"
#include "GraphicsUtilities.h"

namespace Diligent
{

namespace TexturedCube
{

RefCntAutoPtr<IBuffer> CreateVertexBuffer(IRenderDevice*                  pDevice,
                                          GEOMETRY_PRIMITIVE_VERTEX_FLAGS Components,
                                          BIND_FLAGS                      BindFlags,
                                          BUFFER_MODE                     Mode)
{
    GeometryPrimitiveBuffersCreateInfo CubeBuffersCI;
    CubeBuffersCI.VertexBufferBindFlags = BindFlags;
    CubeBuffersCI.VertexBufferMode      = Mode;

    GeometryPrimitiveInfo  CubePrimInfo;
    RefCntAutoPtr<IBuffer> pVertices;
    CreateGeometryPrimitiveBuffers(
        pDevice,
        CubeGeometryPrimitiveAttributes{2.f, Components},
        &CubeBuffersCI,
        &pVertices,
        nullptr,
        &CubePrimInfo);
    VERIFY_EXPR(CubePrimInfo.NumVertices == 24 && CubePrimInfo.NumIndices == 36);
    return pVertices;
}

RefCntAutoPtr<IBuffer> CreateIndexBuffer(IRenderDevice* pDevice, BIND_FLAGS BindFlags, BUFFER_MODE Mode)
{
    GeometryPrimitiveBuffersCreateInfo CubeBuffersCI;
    CubeBuffersCI.IndexBufferBindFlags = BindFlags;
    CubeBuffersCI.IndexBufferMode      = Mode;

    GeometryPrimitiveInfo  CubePrimInfo;
    RefCntAutoPtr<IBuffer> pIndices;
    CreateGeometryPrimitiveBuffers(
        pDevice,
        CubeGeometryPrimitiveAttributes{},
        &CubeBuffersCI,
        nullptr,
        &pIndices,
        &CubePrimInfo);
    VERIFY_EXPR(CubePrimInfo.NumVertices == 24 && CubePrimInfo.NumIndices == 36);
    return pIndices;
}

RefCntAutoPtr<ITexture> LoadTexture(IRenderDevice* pDevice, const char* Path)
{
    TextureLoadInfo loadInfo;
    loadInfo.IsSRGB = true;
    RefCntAutoPtr<ITexture> pTex;
    CreateTextureFromFile(Path, loadInfo, pDevice, &pTex);
    return pTex;
}

RefCntAutoPtr<IPipelineState> CreatePipelineState(const CreatePSOInfo& CreateInfo, bool ConvertPSOutputToGamma)
{
    GraphicsPipelineStateCreateInfo PSOCreateInfo;
    PipelineStateDesc&              PSODesc          = PSOCreateInfo.PSODesc;
    PipelineResourceLayoutDesc&     ResourceLayout   = PSODesc.ResourceLayout;
    GraphicsPipelineDesc&           GraphicsPipeline = PSOCreateInfo.GraphicsPipeline;

    // This is a graphics pipeline
    PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

    // Pipeline state name is used by the engine to report issues.
    // It is always a good idea to give objects descriptive names.
    PSODesc.Name = "Cube PSO";

    // clang-format off
    // This tutorial will render to a single render target
    GraphicsPipeline.NumRenderTargets             = 1;
    // Set render target format which is the format of the swap chain's color buffer
    GraphicsPipeline.RTVFormats[0]                = CreateInfo.RTVFormat;
    // Set depth buffer format which is the format of the swap chain's back buffer
    GraphicsPipeline.DSVFormat                    = CreateInfo.DSVFormat;
    // Set the desired number of samples
    GraphicsPipeline.SmplDesc.Count               = CreateInfo.SampleCount;
    // Primitive topology defines what kind of primitives will be rendered by this pipeline state
    GraphicsPipeline.PrimitiveTopology            = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    // Cull back faces
    GraphicsPipeline.RasterizerDesc.CullMode      = CULL_MODE_BACK;
    // Enable depth testing
    GraphicsPipeline.DepthStencilDesc.DepthEnable = True;
    // clang-format on
    ShaderCreateInfo ShaderCI;
    // Tell the system that the shader source code is in HLSL.
    // For OpenGL, the engine will convert this into GLSL under the hood.
    ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;

    // OpenGL backend requires emulated combined HLSL texture samplers (g_Texture + g_Texture_sampler combination)
    ShaderCI.Desc.UseCombinedTextureSamplers = true;

    // Pack matrices in row-major order
    ShaderCI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

    // Presentation engine always expects input in gamma space. Normally, pixel shader output is
    // converted from linear to gamma space by the GPU. However, some platforms (e.g. Android in GLES mode,
    // or Emscripten in WebGL mode) do not support gamma-correction. In this case the application
    // has to do the conversion manually.
    ShaderMacro Macros[] = {{"CONVERT_PS_OUTPUT_TO_GAMMA", ConvertPSOutputToGamma ? "1" : "0"}};
    ShaderCI.Macros      = {Macros, _countof(Macros)};

    ShaderCI.pShaderSourceStreamFactory = CreateInfo.pShaderSourceFactory;
    // Create a vertex shader
    RefCntAutoPtr<IShader> pVS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube VS";
        ShaderCI.FilePath        = CreateInfo.VSFilePath;
        CreateInfo.pDevice->CreateShader(ShaderCI, &pVS);
    }

    // Create a pixel shader
    RefCntAutoPtr<IShader> pPS;
    {
        ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
        ShaderCI.EntryPoint      = "main";
        ShaderCI.Desc.Name       = "Cube PS";
        ShaderCI.FilePath        = CreateInfo.PSFilePath;
        CreateInfo.pDevice->CreateShader(ShaderCI, &pPS);
    }

    InputLayoutDescX InputLayout;

    Uint32 Attrib = 0;
    if (CreateInfo.Components & GEOMETRY_PRIMITIVE_VERTEX_FLAG_POSITION)
        InputLayout.Add(Attrib++, 0u, 3u, VT_FLOAT32, False);
    if (CreateInfo.Components & GEOMETRY_PRIMITIVE_VERTEX_FLAG_NORMAL)
        InputLayout.Add(Attrib++, 0u, 3u, VT_FLOAT32, False);
    if (CreateInfo.Components & GEOMETRY_PRIMITIVE_VERTEX_FLAG_TEXCOORD)
        InputLayout.Add(Attrib++, 0u, 2u, VT_FLOAT32, False);

    for (Uint32 i = 0; i < CreateInfo.NumExtraLayoutElements; ++i)
        InputLayout.Add(CreateInfo.ExtraLayoutElements[i]);

    GraphicsPipeline.InputLayout = InputLayout;

    PSOCreateInfo.pVS = pVS;
    PSOCreateInfo.pPS = pPS;

    // Define variable type that will be used by default
    ResourceLayout.DefaultVariableType = SHADER_RESOURCE_VARIABLE_TYPE_STATIC;

    // Shader variables should typically be mutable, which means they are expected
    // to change on a per-instance basis
    // clang-format off
    ShaderResourceVariableDesc Vars[] = 
    {
        {SHADER_TYPE_PIXEL, "g_Texture", SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE}
    };
    // clang-format on
    ResourceLayout.Variables    = Vars;
    ResourceLayout.NumVariables = _countof(Vars);

    // Define immutable sampler for g_Texture. Immutable samplers should be used whenever possible
    // clang-format off
    SamplerDesc SamLinearClampDesc
    {
        FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, 
        TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP
    };
    ImmutableSamplerDesc ImtblSamplers[] = 
    {
        {SHADER_TYPE_PIXEL, "g_Texture", SamLinearClampDesc}
    };
    // clang-format on
    ResourceLayout.ImmutableSamplers    = ImtblSamplers;
    ResourceLayout.NumImmutableSamplers = _countof(ImtblSamplers);

    RefCntAutoPtr<IPipelineState> pPSO;
    CreateInfo.pDevice->CreateGraphicsPipelineState(PSOCreateInfo, &pPSO);
    return pPSO;
}

} // namespace TexturedCube

} // namespace Diligent
