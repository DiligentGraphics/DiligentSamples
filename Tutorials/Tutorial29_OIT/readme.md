# Tutorial29 - Order-Independent Transparency

This tutorial demonstrates how to implement order-independent transparency.


# Introduction

Transparent objects have always posed a challenge in computer graphics because the order in which they are rendered affects the final image. This is because the blending operation is not commutative, meaning that the order in which transparent surfaces are composited over each other matters. Sorting objects based on their depth only provides a partial solution as in many cases it is not possible to determine a single correct order that would work for all pixels in the image (e.g. when transparent objects intersect, for self-overlaping transparent objects with complex geometry, for nested objects, etc.). This is where order-independent transparency (OIT) comes in. OIT methods allow for rendering transparent objects in any order without visual artifacts. This tutorial demonstrates how to implement OIT in Diligent Engine.


# Algorithms


## Unsorted alpha-blending

This is the simplest approach that is present to demonstrate the visual artifacts that occur when rendering transparent objects without sorting them.


## Weighted-blended OIT

[Weighted-blended OIT](https://casual-effects.blogspot.com/2015/03/implemented-weighted-blended-order.html) assigns weights to each transparent surface based on its depth and transparency, trying to assign higher weights to surfaces that are closer to the camera and more opaque. The method is simple to implement and very efficient, but due to its approximation nature it has some limitations. First, the weighting functions need to be tweaked for each scene to achieve the best results. Second, the method does not work well for surfaces with high opacity.


## Layered OIT

This approach is based on the method of [Fang Liu et al.](https://dl.acm.org/doi/10.1145/1730804.1730817). However, instead of storing the fragments itself, the method first builds the transmittance function that allows rendering the transparent objects in any order with additive blending.

Consider three surfaces A, B, and C that are composited on top of each other in that order. The final color is given by the following equation:

```
RGB = (RGB_A * T_B  + RGB_B) * T_C + RGB_C = RGB_A * T_B * T_C + RGB_B * T_C + RGB_C
```

where `RGB_A`, `RGB_B`, and `RGB_C` are the (alpha-premultiplied) colors of the surfaces, and `T_B` and `T_C` are the transmittance functions of the surfaces. The transmittance function is defined as the fraction of light that passes through the surface. 

Note that we can rewrite this equation in a general form as:

```
RGB = RGB_A * Tc(A)  + RGB_B * Tc(B) + RGB_C * Tc(C)
```

where `Tc(X)` is the cumulative transmittance function from the camera to the surface X. The cumulative transmittance function is defined as the product of the transmittance functions of all surfaces between the camera and the surface X. Thus if we know the transmittance function, we can render the transparent objects in any order with additive blending.


### The transmittance function

The transmittance function in our implementation is represented as a set of a K closest layers storing the layer transmittance and depth. All other layers are merged into the tail that contains the total number of merged layers and total transmittance. Each layer is represented as a 32-bit integer where the 24 most significant bits store the depth and the 8 least significant bits store the transmittance. This representation allows for sorting the layers based on their depth using atomic operations. The following function is used to pack the layer data into a 32-bit integer:

```hlsl
uint PackOITLayer(float Depth, float Transmittance)
{
    uint D = uint(clamp(Depth, 0.0, 1.0) * 16777215.0);
    uint T = uint(clamp(Transmittance, 0.0, 1.0) * 255.0);
    return (D << 8u) | T;
}
```

The layer data is stored in a structured buffer and the tail is stored in RGBA8 texture.


### Implementation

The algorithm runs after all opaque objects have been rendered. It consists of the following steps discussed in details below:

* Clear oit layers buffer
* Render transparent objects to build the transmittance function
* Attenuate the background
* Render transparent objects and composite them using the transmittance function

#### Clearing the OIT layers buffer

The layers buffer is cleared by a compute shader that sets the value of each element to `0xFFFFFFFF` which corresponds to the empty layer.


#### Building the transmittance function

The main challenge in building the transmitance function is to design an algorithm that can merge the layers in parallel. Recall that the GPU processes multiple surfaces that cover the same pixel simultaneously and in arbitrary order.
We build upon the original algorithm by Fang Liu et al. and use atomic operations. Recall that we pack the layer depth and transmittance into a 32-bit integer that allows using the atomic min operation to insert a new layer into the buffer. The algorithm keeps layers sorted based on their depth. To insert a new layer, it performs atomic min operation with all layers in order. If the operation succeeds, the new layer is inserted into the buffer, and atomic operation returns the previous value, which in turn needs to be inserted into the buffer. The remaining layers are merged into the tail.

The algorithm is implemented in the following pixel shader:

```hlsl
RWStructuredBuffer<uint> g_rwOITLayers;

// By default, early depth stencil will be disabled for this shader because it writes to a UAV.
// Force it to be enabled.
[earlydepthstencil]
float4 main(in PSInput PSIn) : SV_Target
{
    float D = PSIn.Pos.z;
    float A = lerp(g_Constants.MinOpacity, g_Constants.MaxOpacity, PSIn.Color.a);
    uint Layer = 0xFFFFFFFFu;
    if (A > OPACITY_THRESHOLD)
    {
        float T = 1.0 - A;
        Layer = PackOITLayer(D, T);
        uint Offset = GetOITLayerDataOffset(uint2(PSIn.Pos.xy), g_Constants.ScreenSize);
        for (uint i = 0; i < uint(NUM_OIT_LAYERS); ++i)
        {
            uint OrigLayer;
            InterlockedMin(g_rwOITLayers[Offset + i], Layer, OrigLayer);
            if (OrigLayer == 0xFFFFFFFFu || // Empty space
                OrigLayer == Layer)         // Layer matches another one exactly
            {
                // Do not update tail transmittance
                Layer = 0xFFFFFFFFu;
                break;
            }
            // Layer > OrigLayer: we did not insert the layer
            // Layer < OrigLayer: we inserted the layer, so now we need to 
            //                    insert the original layer
            Layer = max(Layer, OrigLayer);
        }
    }
    
    // RGB Blend: Src * 1 + Dst * 1
    // A   Blend: Src * 0 + Dst * SrcA
    if (Layer == 0xFFFFFFFFu)
    {
        return float4(0.0, 0.0, 0.0, 1.0);
    }
    else
    {
        float TailT = GetOITLayerTransmittance(Layer);
        return float4(1.0 / 255.0, 0.0, 0.0, TailT);
    }
}
```

The algorithm starts by obtaining the pixel depth value and the alpha value of the transparent object. Notice that alpha represents the object opacity, while we need the transmittance value which is `1 - alpha`. The opacity value is then compared with the opacity threshold that is set to `1.0/255.0`. Since we use 8 bits to store transmittance, any opacity below that corresponds to the fully transparent object and is discarded.

```hlsl
float D = PSIn.Pos.z;
float A = lerp(g_Constants.MinOpacity, g_Constants.MaxOpacity, PSIn.Color.a);
uint Layer = 0xFFFFFFFFu;
if (A > OPACITY_THRESHOLD)
{
    float T = 1.0 - A;
```

The algorithm then packs the layer data and starts inserting it into the buffer:

```hlsl
    Layer = PackOITLayer(D, T);
    uint Offset = GetOITLayerDataOffset(uint2(PSIn.Pos.xy), g_Constants.ScreenSize);
    for (uint i = 0; i < uint(NUM_OIT_LAYERS); ++i)
    {
```

For each layer, it performs atomic min operation in an attempt to insert the new layer. The operation returns the previous value stored in the buffer. Few outcomes are possible:

* Original value is `0xFFFFFFFF` - the layer is inserted into the empty space in the buffer. In this case, nothing else needs to be done.
* Original value is equal to the new layer - this is a rare case where two layers at the same depth have the same transmittance. In this case, the new layer is discarded.
* The two remaining cases is either the layer was successfully inserted and `OrigLayer` contains another value larger than the new layer, or the layer was not inserted and `OrigLayer` contains a value smaller that the new layer. In both cases, the algorithm needs to insert the maximum of the two values into the next position in the buffer.

```hlsl
    for (uint i = 0; i < uint(NUM_OIT_LAYERS); ++i)
    {
        uint OrigLayer;
        InterlockedMin(g_rwOITLayers[Offset + i], Layer, OrigLayer);
        if (OrigLayer == 0xFFFFFFFFu || OrigLayer == Layer)
        {
            Layer = 0xFFFFFFFFu;
            break;
        }
        Layer = max(Layer, OrigLayer);
    }
```

Let's take a look at the following example. Suppose we have the following values in the buffer: `10`, `20`, `30`, and we want to insert the value `15`. The algorithm will perform the following steps:

1. Attempt to insert `15` into the first position. Since `10 < 15`, the buffer is not updated. `OrigLayer = 10`, `Layer = max(15, 10) = 15`
2. Attempt to insert `15` into the second position. Since `15 < 20`, the buffer is updated. `OrigLayer = 20`, `Layer = max(15, 20) = 20`. Buffer now contains `10`, `15`, `30`
3. Attempt to insert `20` into the third position. Since `20 < 30`, the buffer is updated. `OrigLayer = 30`, `Layer = max(20, 30) = 30`. Buffer now contains `10`, `15`, `20`, and we left with the value `30` that needs to be inserted into the tail.

Note that multiple shader invocations can attempt to insert different layers into the same place in the buffer. In general, the buffer contents may change between each loop iteration. However, the following considerations ensure that the insertion algorithm works correctly:

1. Values in the buffer may only decrease. If an algorithm is attempting to insert a value at position `i`, it may only be inserted at position `i` or later, no matter what other invocations are doing.
2. Each invocation guarantees that value at position `i` is larger than the value at position `i-1`. This ensures that the buffer is always sorted.

After the loop finishes, there are two possible outcomes: either the layer was inserted into the buffer, in which case `Layer == 0xFFFFFFFFu`, or we are left with the value that needs to be inserted into the tail. The tail is updated using the alpha blending. We accumulate the total number of tail layers in the color channel, and the total transmittance in the alpha channel:

```hlsl
// RGB Blend: Src * 1 + Dst * 1
// A   Blend: Src * 0 + Dst * SrcA
if (Layer == 0xFFFFFFFFu)
{
    return float4(0.0, 0.0, 0.0, 1.0);
}
else
{
    float TailT = GetOITLayerTransmittance(Layer);
    return float4(1.0 / 255.0, 0.0, 0.0, TailT);
}
```

An important property of this algorithm is that it is stable. The order in which transparent objects are rendered does not affect the final result. The K closest layers are stored in the buffer, and all other layers are merged into the tail. The tail blending operations are commutative, so the order in which they are composited does not matter.


A very important detail not to miss is that the shader uses the `earlydepthstencil` attribute to force early depth stencil test to be enabled. This is necessary because the shader writes to a UAV, which by default disables early depth stencil test. Sinc we want opaque objects to occlude any transparent objects behind them, we need to force early depth stencil test to be enabled.


#### Attenuating the background

We are now ready to render the transparent objects, but before we do that, we need to attenuate the background based on the transmittance function. This is done by rendering a full-screen quad that uses alpha-blending to multiply the background color with the transmittance value. The source code for the pixel shader is shown below:

```hlsl
StructuredBuffer<uint> g_OITLayers;
Texture2D<float4>      g_OITTail;

void main(in float4 Pos : SV_Position,
          out PSOutput PSOut)
{
    uint Offset = GetOITLayerDataOffset(uint2(Pos.xy), g_Constants.ScreenSize);
    float T = 1.0;
    uint layer = 0u;
    while (layer < uint(NUM_OIT_LAYERS))
    {
        uint LayerDT = g_OITLayers[Offset + layer];
        if (LayerDT == 0xFFFFFFFFu)
            break;
        T *= GetOITLayerTransmittance(LayerDT);
        ++layer;
    }
    if (layer == uint(NUM_OIT_LAYERS))
    {
        float4 Tail = g_OITTail.Load(int3(Pos.xy, 0));
        T *= Tail.a;
    }
    if (T == 1.0)
        discard;

    // RGB blend: Src * 0 + Dst * SrcA
    PSOut.Color = float4(0.0, 0.0, 0.0, T);
}

```

The shader iterates over all layers in the buffer and accumulates the transmittance value. If the maximum number of layers is reached, the tail transmittance is loaded from the alpha channel of the tail texture.


#### Compositing the transparent objects

The final step is to render the transparent objects and composite them using the transmittance function. To compute the transmittance, the pixel shader iterates over all layers in the buffer and accumulates the transmittance of all layers closer to the camera than the current pixel:

```hlsl
float Depth = PSIn.Pos.z;
uint D = uint(Depth * 16777215.0);
float T = 1.0;
    
uint Offset = GetOITLayerDataOffset(uint2(PSIn.Pos.xy), g_Constants.ScreenSize);

uint layer = 0u;
while (layer < uint(NUM_OIT_LAYERS))
{
    uint LayerDT = g_OITLayers[Offset + layer];
    uint LayerD = GetOITLayerDepth(LayerDT);
    // +1u helps to avoid precision issues.
    if (D <= LayerD + 1u)
    {
        break;
    }
    float LayerT = GetOITLayerTransmittance(LayerDT);
    T *= LayerT;
    ++layer;
}
if (layer == uint(NUM_OIT_LAYERS))
{
    float4 Tail = g_OITTail.Load(int3(PSIn.Pos.xy, 0));
    // Average contribution of all tail layers.
    T /= max(255.0 * Tail.x, 1.0);
}

Color.rgb *= T;
```

Note that the contribution of the tail layers is averaged. The total number of tail layers is stored in the red channel of the tail texture.


### API-specific notes

Since **WebGPU** does not support the `earlydepthstencil` attribute, we have to perform the depth test manually in the pixel shader.

Since **OpenGL** does not allow using the depth buffer of a default framebuffer with any other render target, we have to perform a few extra copies.


# Discussion

The layered OIT algorithm is efficient and stable. It provides 100% correct results if the number of overlapping transparent objects is not greater than the number of layers in the buffer. It properly handles objects with high opacity, including 100% opaque. For `K` layers, the algorithm uses `K * 4 + 4` bytes. For 4 layers, this is 20 bytes. Tweaking the number of layers allows for a trade-off between memory consumption/performance and quality.

# References

- [Weighted Blended Order-Independent Transparency by Morgan McGuire and Louis Bavoil](https://jcgt.org/published/0002/02/09/)
- [Implementing Weighted, Blended Order-Independent Transparency by Morgan McGuire](https://casual-effects.blogspot.com/2015/03/implemented-weighted-blended-order.html)
- [Order-Independent Transparency Vulkan Sample by NVidia](https://github.com/nvpro-samples/vk_order_independent_transparency)
- [FreePipe: a programmable parallel rendering architecture for efficient multi-fragment effects by Fang Liu et al.](https://dl.acm.org/doi/10.1145/1730804.1730817)
