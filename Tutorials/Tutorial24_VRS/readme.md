# Tutorial24 - Variable rate shading

This tutorial demonstrates how to use variable rate shading to reduce pixel shading load.

![](Animation_Large.gif)


## VRS on desktop GPUs

Variable shading rate implementations on mobile and desktop GPUs and in different APIs supported by Diligent Engine vary significantly.
On desktop GPUs, variable rate shading is used to change the number of pixel shader invocations per small tile,
which allows reducing the number of pixel shader invocations for areas which don't require high quality (e.g., areas that will be blurred
by motion blur or depth-of-field, covered by UI, etc.). Direct3D12 and Vulkan provide three VRS types that may be combined together:
per-draw, per-primitive, and texture-based rates. These options are described below. 

### Per-draw Shading Rate

If capability `SHADING_RATE_CAP_FLAG_PER_DRAW` is supported, we can use the `IDeviceContext::SetShadingRate()` method to
set the shading rate that will be uniformly applied throught the entire render target.
The `BaseRate` parameter must be one of the shading rates supported by Device. All supported rates are listed in
`ShadingRateProperties::ShadingRates` member of the `GraphicsAdapterInfo` struct.
`PrimitiveCombiner` and `TextureCombiner` parameters must be `SHADING_RATE_COMBINER_PASSTHROUGH`.

### Per-primitive Shading Rate

If capability `SHADING_RATE_CAP_FLAG_PER_PRIMITIVE` is supported, shading rate may be specified for each primitive
by using a special vertex or geometry shader output:
`nointerpolation uint Rate : SV_ShadingRate;`

Per-primitive rate is combined with the base per-draw rate as specified by the `PrimitiveCombiner` paremeter
of the `IDeviceContext::SetShadingRate()` method. The combiner value must be one of the supported combiners in the bit
field `ShadingRateProperties::Combiners`. The two most commonly used combiners are
* `SHADING_RATE_COMBINER_PASSTHROUGH`, that defines that per-primitive rate is ignored and the base rate is used instead, and
* `SHADING_RATE_COMBINER_OVERRIDE` that defines that per-primitive rate overrides the base rate.

Other values specify how to combine per-draw base rate with the per-primitive rate.

### Texture-based Shading Rate

If capability `SHADING_RATE_CAP_FLAG_TEXTURE_BASED`, shading rate may be defined using a special texture that for every
pixel of the screen defines the rate. The texture-based rate is combined with the primitive rate, and applied last.
The combiner for the texture rate is defined by `TextureCombiner` parameter of the `IDeviceContext::SetShadingRate()`
method. The combiner value must be one of the supported combiners in the bit field `ShadingRateProperties::Combiners`.
If `PrimitiveCombiner` is `SHADING_RATE_COMBINER_PASSTHROUGH` and `TextureCombiner` is `SHADING_RATE_COMBINER_OVERRIDE`,
then only the texture shading rate will be used.

It is possible to use other parameters that define how all three rates are combined.

On desktop GPUs, VRS texture must be created with type `RESOURCE_DIM_TEX_2D`, and use bind flag `BIND_SHADING_RATE`.
Its format must be `TEX_FORMAT_R8_UINT` and the size must be less than the render target size divided by `ShadingRateProperties::MinTileSize`.
The texel values correspond to the values in `SHADING_RATE` enum. This is valid for `SHADING_RATE_FORMAT_PALETTE` format in `ShadingRateProperties::Format`.
The texture content can be updated from CPU or in a compute shader. Note that Direct3D12 forbids creating VRS texture with the render target flag, but in
Vulkan this may be allowed depending on the implementation.

### Combiners

Shading rate combination algorithm is as follows:

```cpp
SHADING_RATE ApplyCombiner(SHADING_RATE_COMBINER Combiner, SHADING_RATE OriginalRate, SHADING_RATE NewRate)
{
    switch (Combiner)
    {
        case SHADING_RATE_COMBINER_PASSTHROUGH: return OriginalRate;
        case SHADING_RATE_COMBINER_OVERRIDE:    return NewRate;
        case SHADING_RATE_COMBINER_MIN:         return Min(OriginalRate, NewRate);
        case SHADING_RATE_COMBINER_MAX:         return Max(OriginalRate, NewRate);
        case SHADING_RATE_COMBINER_SUM:         return OriginalRate + NewRate;
        case SHADING_RATE_COMBINER_MUL:         return OriginalRate * NewRate;
    }
}
```

`IDeviceContext::SetShadingRate()` sets the `BaseRate` value.<br/>
`SV_ShadingRate` output of a vertex or a geometry shader defines the `PerPrimitiveRate` value and it is combined with the per-draw rate:
`PrimitiveRate = ApplyCombiner(PrimitiveCombiner, BaseRate, PerPrimitiveRate)`<br/>

Next, the texel from the VRS texture is read to `TextureRate` and is combined with the primitive rate from the previous step:
`FinalRate = ApplyCombiner(TextureCombiner, PrimitiveRate, TextureRate)`<br/>

After that GPU executes pixel shaders on a 4x4 tile using the `FinalRate` value.<br/>

`SHADING_RATE_1X1` means that pixel shader will be executed for all pixels in the tile will be processed.
`SHADING_RATE_4X4` means that just one pixel shader per tile will be executed. Other values define intermediate rates.


## VRS on mobile GPUs

On mobile GPUs supported only texture based VRS.
Format `SHADING_RATE_FORMAT_UNORM8` means that VRS texture must be created with `RG8_UNORM` format, where R channel is a pixel shader invocation rate on X-axis, G channel - on Y-axis.
Value 1.0 means that all pixels will be processed by pixel shaders, value 0.5 - just a half of pixels will be processed on each axis.

If capability `SHADING_RATE_CAP_FLAG_TEXTURE_DEVICE_ACCESS` is supported then VRS texture content will be accessed on the GPU side, so if you update VRS texture in compute shader then engine can implicitly synchronize access to the VRS texture if automatic state transitions are used.
But if capability is not supported then VRS texture will be accessed on the CPU side when `IDeviceContext::SetRenderTargetsExt()` or `IDeviceContext::BeginRenderPass()` are used and if VRS texture updated on the GPU side you need to explicitly synchronize access to the texture.


## VRS in Metal API

Implementation of variable rate shading in Metal is very different compared to Direct3D12 and Vulkan. It requires
special handling from the application.

1. Unlike Direct3D12 and Vulkan where only pixel shader is executed at a lower rate, but rasterization is performed
   at the full resolution (in particular, depth testing is performed at full resolution), in Metal everything is downscaled.
   A 2x2 shading rate means that the entire tile is rasterized at 1/2x1/2 resolution, not just the pixel shader.
2. Only texture-based shading rate is supported.
3. Rasterization rate is not specified by a 2D texture. Instead, a special rasterization map is used that defines 
   rasterization rates for columns and rows. A rasterization rate for the specific tile is given by the row and column
   where it is located. Rows and columns use a single float value in the range 0..1.
   Diligent defines a special shading rate format `SHADING_RATE_FORMAT_COL_ROW_FP32` to indicate this kind of shading rate map.
   A new interface `IRasterizationRateMapMtl` is used to work with the rasterization rate map in Metal.
![](mtl_vrs.png)
4. A rasterization rate map is an immutable object and can't be updated. A new object has to be created to use different rates.
5. Rendering with VRS enabled is performed to a reduced-resolution texture.
6. A special resolve pass is required to upscale this texture to the full resolution.

The code snippet below shows how a resolve pass may be implemented in Metal:

```cpp
fragment
float4 PSmain(         VSOut                        in          [[stage_in]],   // vertex shader generates fullscreen triangle
              constant rasterization_rate_map_data& g_RRMData   [[buffer(0)]],  // data that copied from rasterization rate map
                       texture2d<float>             g_Texture   [[texture(0)]]) // intermediate render target
{
    constexpr sampler readSampler(coord::pixel, address::clamp_to_zero, filter::nearest);

    rasterization_rate_map_decoder Decoder(g_RRMData);

    // Convert normalized UV to intermediate render target coordinates in pixels
    float2 uv = in.UV * float2(g_Texture.get_width(), g_Texture.get_height());

    // Convert from linear to non-linear coordinates
    float2 ScreenPos = Decoder.map_screen_to_physical_coordinates(uv);

    // Resolve intermediate texture into full size texture
    return float4(g_Texture.sample(readSampler, ScreenPos));
```


## VRS API in Diligent Engine

Variable rate shading feature must be enabled during the engine initialization:

```cpp
Attribs.EngineCI.Features.VariableRateShading = DEVICE_FEATURE_STATE_ENABLED;
```

A graphics PSO that will be used with VRS must set the `PIPELINE_SHADING_RATE_FLAGS ShadingRateFlags` field in its description.<br/>
PSO that uses per-draw or per-primitive rates must be created with the `PIPELINE_SHADING_RATE_FLAG_PER_PRIMITIVE` flag.<br/>
When using a texture-based VRS, all PSOs that are used for rendering must be created with the `PIPELINE_SHADING_RATE_FLAG_TEXTURE_BASED` flag.
Combination of `PIPELINE_SHADING_RATE_FLAG_PER_PRIMITIVE` and `PIPELINE_SHADING_RATE_FLAG_TEXTURE_BASED` is valid but has no additional effect.

To begin texture-based shading rate, `IDeviceContext::SetRenderTargetsExt()` should be called, that uses attributes structure with `pShadingRateMap` field:

```cpp
ITextureView*           pRTVs[] = {m_pRTV};
SetRenderTargetsAttribs RTAttrs;
RTAttrs.NumRenderTargets    = 1;
RTAttrs.ppRenderTargets     = pRTVs;
RTAttrs.pDepthStencil       = m_pDSV;
RTAttrs.pShadingRateMap     = m_pShadingRateMap;
m_pImmediateContext->SetRenderTargetsExt(RTAttrs);

m_pImmediateContext->SetShadingRate(SHADING_RATE_1X1, SHADING_RATE_COMBINER_PASSTHROUGH, SHADING_RATE_COMBINER_OVERRIDE);
```

`IDeviceContext::SetShadingRate()` is required on desktop GPUs to enable VRS. The default values is
`SetShadingRate(SHADING_RATE_1X1, SHADING_RATE_COMBINER_PASSTHROUGH, SHADING_RATE_COMBINER_PASSTHROUGH)`.
In other implementations, VRS is always enabled when VRS texture is bound, but
`SetShadingRate(SHADING_RATE_1X1, SHADING_RATE_COMBINER_PASSTHROUGH, SHADING_RATE_COMBINER_OVERRIDE)` can be used for compatibility.
