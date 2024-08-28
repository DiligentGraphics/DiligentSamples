# Tutorial05 - Texture Array

This tutorial demonstrates how to combine instancing shown in Tutotial04 with texture arrays to 
use unique texture for every instance.

![](Animation_Large.gif)

[Run in the browser](https://diligentgraphics.github.io/wasm-modules/Tutorial05_TextureArray/Tutorial05_TextureArray.html)

Texture array is a special kind of texture that consists of multiple 2D (or 1D) textures sharing the same
format, size, and the number of mip levels. Every individual texture in the array is called *slice*. Slices can
be dynamically indexed by the shaders which enables a number of useful techniques such as selecting different slices 
for different instances.

## Shaders

Vertex shader is mostly similar to the one from Tutorial04, but uses one more per-instance attribute, texture
array index that it passes to the pixel shader:

```hlsl
struct PSInput 
{ 
    float4 Pos     : SV_POSITION; 
    float2 UV      : TEX_COORD; 
    float TexIndex : TEX_ARRAY_INDEX;
};

struct VSInput
{
    // ...
    float  TexArrInd : ATTRIB6;
};

void main(in  VSInput VSIn
          out PSInput PSIn) 
{
    // ...
    PSIn.TexIndex = VSIn.TexArrInd;
}
```

Pixel shader declares `Texture2DArray` variable `g_Texture` and uses the index passed from the
vertex shader to select the slice. When sampling a texture array, 3-component vector is used as the texture
coordinate, where the third component defines the slice.

```hlsl
Texture2DArray g_Texture;
SamplerState   g_Texture_sampler;

struct PSInput 
{ 
    float4 Pos      : SV_POSITION; 
    float2 UV       : TEX_COORD; 
    float  TexIndex : TEX_ARRAY_INDEX;
};

struct PSOutput
{
    float4 Color : SV_TARGET;
};

void main(in  PSInput  PSIn,
          out PSOutput PSOut)
{
    PSOut.Color = g_Texture.Sample(g_Texture_sampler, float3(PSIn.UV, PSIn.TexIndex)); 
}
```

## Initializing the Pipeline State

Pipeline state is initialized in the same way as in Tutorial04. The only difference is that the vertex layout
defines one additional per-instance attribute, texture array index:

```cpp
LayoutElement LayoutElems[] =
{
    // ...
    // Attribute 6 - texture array index
    LayoutElement{6, 1, 1, VT_FLOAT32, False, LAYOUT_ELEMENT_AUTO_OFFSET, LAYOUT_ELEMENT_AUTO_STRIDE, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
};
```

## Loading Texture Array

Texture loading library does not provide a function that loads texture array.
Instead, we load each individual texture and then prepare texture initialization data
for all slices.

```cpp
std::vector<RefCntAutoPtr<ITextureLoader>> TexLoaders(NumTextures);
// Load textures
for (int tex = 0; tex < NumTextures; ++tex)
{
    // Create loader for the current texture
    std::stringstream FileNameSS;
    FileNameSS << "DGLogo" << tex << ".png";
    const auto      FileName = FileNameSS.str();
    TextureLoadInfo LoadInfo;
    LoadInfo.IsSRGB = true;

    CreateTextureLoaderFromFile(FileName.c_str(), IMAGE_FILE_FORMAT_UNKNOWN, LoadInfo, &TexLoaders[tex]);
}

auto TexArrDesc      = TexLoaders[0]->GetTextureDesc();
TexArrDesc.ArraySize = NumTextures;
TexArrDesc.Type      = RESOURCE_DIM_TEX_2D_ARRAY;
TexArrDesc.Usage     = USAGE_DEFAULT;
TexArrDesc.BindFlags = BIND_SHADER_RESOURCE;

// Prepare initialization data
std::vector<TextureSubResData> SubresData(TexArrDesc.ArraySize * TexArrDesc.MipLevels);
for (Uint32 slice = 0; slice < TexArrDesc.ArraySize; ++slice)
{
    for (Uint32 mip = 0; mip < TexArrDesc.MipLevels; ++mip)
    {
        SubresData[slice * TexArrDesc.MipLevels + mip] = TexLoaders[slice]->GetSubresourceData(mip, 0);
    }
}
TextureData InitData{SubresData.data(), TexArrDesc.MipLevels * TexArrDesc.ArraySize};

// Create the texture array
RefCntAutoPtr<ITexture> pTexArray;
m_pDevice->CreateTexture(TexArrDesc, &InitData, &pTexArray);

// Get shader resource view from the texture array
m_TextureSRV = pTexArray->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
```


The only last detail that is different from Tutorial04 is that `PopulateInstanceBuffer()` function computes
texture array index, for every instance, and writes it to the instance buffer along with the transform matrix.
