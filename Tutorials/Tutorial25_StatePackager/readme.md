# Tutorial25 - Render State Packager

This tutorial shows how to create and archive pipeline states with the render state packager off-line tool
on the example of a simple path tracer.

![](Screenshot.jpg)

## Render State Packager

Diligent Engine allows compiling shaders from source code at run-time and using them to create pipeline states.
While simple and convenient, this approach has downsides:

- Compiling shaders at run-time is expensive. An application that uses a lot of shaders may see significant load times.
- Compiling shaders at run-time requires bundling the compiler with the engine. For Vulkan, this includes glslang, SPIRV-Tools
  and other components, which significantly increase the binary size.
- After shaders are compiled, Diligent further needs to patch them to make them compatible with the pipeline resource layout or
  resource signatures.

Pipeline state processing can be completely performed off-line using the 
[Render state packager](https://github.com/DiligentGraphics/DiligentTools/tree/master/RenderStatePackager) tool.
The tool uses the `Diligent Render State Notation`, a JSON-based render state description language. One archive
produced by the packager can contain multiple pipeline states as well as shaders and pipeline resource signatures that can
be loaded and immediately used at run-time without any overhead. Each object can also contain data for different
backends (e.g. DX12 and Vulkan), thus one archive can be used on different platforms.

Archivig pipelines off-line provides a number of benefits:

- Run-time loading overhead is reduced to minimum as no shader compilation or byte code patching is performed.
- Shader compilation tool chain can be removed from the binary to reduce its size.
- Render states are separated from the executable code that improves the code structure.
  Shaders and pipeline states can be updated without the need to rebuild the application.
- Shader compilation errors and pipeline state issues are detected at build time.

In this tutorial, we will use the render state packager to create an archive that contains all pipeline
states required to perform basic path tracing.

## Path Tracing

This tutorial implements a basic path tracing algorithm with next event estimation (aka light source sampling).
In this section we provide some details about the rendering process. Additional information on path tracing can be easily
found on the internet. [Ray Tracing in One Weekend](https://github.com/RayTracing/raytracing.github.io/)
could be a good starting point.

The rendering process consists of the following three stages:

1. G-buffer generation
2. Path tracing and radiance accumulation
3. Resolve

At the first stage, the scene is rendered into a G-buffer consisting of the following render targets:

- Albedo (`RGBA8_UNORM`)
- Normal (`RGBA8_UNORM`)
- Emissive (`R11G11B10_FLOAT`)
- Depth (`R32_FLOAT`)

At the second state, a compute shader is executed that for each pixel of the G-buffer reconstructs its
world-space position, traces a light path through the scene and adds the contribution to the radiance accumulation
buffer. Each frame, a set number of new paths are traced and their contributions are accumulated. If camera moves
or light attributes change, the accumulation buffer is cleard and the process starts over.

Finally, at the third stage, the radiance in the light accumulation buffer is resolved by averaging all
light path contributions.

### Scene Representtion

For the sake of illustration, the scene in this tutorial is defined by a number of analytic shapes (boxes) and
rays are traced through the scene by computing intersections with each box and finding the closest one for each ray.
Real applications will likely use DXR/Vulkan ray tracing (see
[Tutorial 21 - Ray Tracing](https://github.com/DiligentGraphics/DiligentSamples/tree/master/Tutorials/Tutorial21_RayTracing) and 
[Tutorial 22 - Hybrid Rendering](https://github.com/DiligentGraphics/DiligentSamples/tree/master/Tutorials/Tutorial22_HybridRendering)).

A box is defined by its center, sizes, color and type:

```hlsl
struct BoxInfo
{
    float3  Center;
    float3  Size;
    float3  Color;
    int     Type;
};
```

For the type, two values are allowed: lambertian diffuse surface and light source.

A ray is defined by its origin and normalized direction:

```hlsl
struct RayInfo
{
    float3 Origin;
    float3 Dir;
};
```

A hit point contains information about the color at the intersection, the surface normal,
the distance from the ray origin to the hit point and also the hit type (lambertian surface,
diffuse light source or none):

```hlsl
struct HitInfo
{
    float3 Color; // Albedo or radiance for light
    float3 Normal;
    float  Distance;
    int    Type;
};
```

An intersection of the ray with the box is computed by the `IntersectAABB` function:

```hlsl
bool IntersectAABB(in    RayInfo Ray,
                   in    BoxInfo Box,
                   inout HitInfo Hit)
```

The function takes the ray information, box attributes and also the current hit point information.
If the new hit point is closer than the current one defined by the `Hit`, the struct is updated with
the new color, normal, distance and hit type.

Casting a ray though the scene then consists of intersecting the ray with each box:

```hlsl
float RoomSize  = 10.0;
float WallThick = 0.05;

BoxInfo Box;
Box.Type = HIT_TYPE_LAMBERTIAN;

float3 Green = float3(0.1, 0.6, 0.1);
float3 Red   = float3(0.6, 0.1, 0.1);
float3 Grey  = float3(0.5, 0.5, 0.5);

// Right wall
Box.Center = float3(RoomSize * 0.5 + WallThick * 0.5, 0.0, 0.0);
Box.Size   = float3(WallThick, RoomSize * 0.5, RoomSize * 0.5);
Box.Color  = Green;
IntersectAABB(Ray, Box, Hit);

// Left wall
Box.Center = float3(-RoomSize * 0.5 - WallThick * 0.5, 0.0, 0.0);
Box.Size   = float3(WallThick, RoomSize * 0.5, RoomSize * 0.5);
Box.Color  = Red;
IntersectAABB(Ray, Box, Hit);

// Ceiling
// ...
```


### G-buffer Rendering

The G-buffer is rendered by a full-screen render pass, where the pixel shader
performs ray casting through the scene. It starts with computing the ray starting
and end points using the inverse view-projection matrix:

```hlsl
float3 f3RayStart = ScreenToWorld(PSIn.Pos.xy, 0.0, f2ScreenSize, g_Constants.ViewProjInvMat);
float3 f3RayEnd   = ScreenToWorld(PSIn.Pos.xy, 1.0, f2ScreenSize, g_Constants.ViewProjInvMat);

RayInfo Ray;
Ray.Origin = f3RayStart; 
Ray.Dir    = normalize(f3RayEnd - f3RayStart);
```

Next, the shader casts a ray though the scene and writes the hit point properties to the G-buffer
targets:

```hlsl
PSOutput PSOut;

HitInfo Hit = IntersectScene(Ray, g_Constants.f2LightPosXZ, g_Constants.f2LightSizeXZ);
if (Hit.Type == HIT_TYPE_LAMBERTIAN)
{
    PSOut.Albedo.rgb = Hit.Color;
}
else if (Hit.Type == HIT_TYPE_DIFFUSE_LIGHT)
{
    PSOut.Emissive.rgb = g_Constants.f4LightIntensity.rgb;
}

PSOut.Normal = float4(saturate(Hit.Normal * 0.5 + 0.5), 0.0);
```

In this example we only have one light source, so we use the light properties from the
constant buffer. Real applications will retrieve the light properties from the hit point.

Finally, we compute the depth by transforming the hit point with the view-projection matrix:

```hlsl
float3 HitWorldPos = Ray.Origin + Ray.Dir * Hit.Distance;
float4 HitClipPos  = mul(float4(HitWorldPos, 1.0), g_Constants.ViewProjMat);
PSOut.Depth        = min(HitClipPos.z / HitClipPos.w, 1.0);
```


### Path Tracing

Path tracing is the core part of the rendering process and is implemented by a compute shader that
runs one thread for each screen pixel. It starts from positions defined by the G-buffer
and traces a given number of light paths through the scene, each path performing a given number of bounces.
For each bounce, the shader traces a ray towards the light source and compute its contribution. It then selects
a random direction at the shading point using the cosine-weighted hemispherical distribution, casts
a ray in this direction and repeats the process at the new location.

The shader starts with reading the sample attributes from the G-buffer and reconstructing the world-space position
of the current sample using the inverse view-projection matrix:

```hlsl
float  fDepth       = g_Depth.Load(int3(ThreadId.xy, 0)).x;
float3 f3SamplePos0 = ScreenToWorld(float2(ThreadId.xy) + float2(0.5, 0.5), fDepth, f2ScreenSize, g_Constants.ViewProjInvMat);

float3 f3Albedo0  = g_Albedo.Load(int3(ThreadId.xy, 0)).xyz;
float3 f3Normal0  = normalize(g_Normal.Load(int3(ThreadId.xy, 0)).xyz * 2.0 - 1.0);
float3 f3Emissive = g_Emissive.Load(int3(ThreadId.xy, 0)).xyz;
```

Where `ThreadId` is the compute shader thread index in a two-dimensional grid and matches the screen coordinates.

The shader then prepares a two-dimensional hash seed:

```hlsl
uint2 Seed = ThreadId.xy * uint2(11417, 7801) + uint2(g_Constants.uFrameSeed1, g_Constants.uFrameSeed2);
```

This is quite important moment: the seed must be unique for each frame, each sample and every bounce to
produce a good random sequence. Bad sequences will result in slower convergence or biased result.
`g_Constants.uFrameSeed1` and `g_Constants.uFrameSeed2` are random seeds computed by the CPU for each frame.

The shader then traces a given number of light paths and accumulates their contributions. Each path starts from the 
same G-buffer sample:

```hlsl
float3 f3SamplePos = f3SamplePos0;
float3 f3Albedo    = f3Albedo0;
float3 f3Normal    = f3Normal0;

// Total contribution of this path
float3 f3PathContrib = float3(0.0, 0.0, 0.0);
// Bounce attenutation
float3 f3Attenuation = float3(1.0, 1.0, 1.0);

for (int j = 0; j < g_Constants.iNumBounces; ++j)
{
    // ...
}
```

In the loop, the shader first samples a random point on the light surface:

```hlsl
float2 rnd2 = hash22(Seed);
float3 f3LightSample = SampleLight(g_Constants.f2LightPosXZ, g_Constants.f2LightSizeXZ, rnd2);
float3 f3DirToLight  = f3LightSample - f3SamplePos;
float fDistToLightSqr = dot(f3DirToLight, f3DirToLight);
f3DirToLight /= sqrt(fDistToLightSqr);
```

The `hash22` function takes a seed and produces two pseudo-random values in the [0, 1] range. The
`SampleLight` function then uses these values to interpolate between the light box corners:

```hlsl
float3 SampleLight(float2 Pos, float2 Size, float2 uv)
{
    BoxInfo Box = GetLight(Pos, Size);
    float3 Corner0 = Box.Center - Box.Size;
    float3 Corner1 = Box.Center + Box.Size;
    float3 Sample;
    Sample.xz = lerp(Corner0.xz, Corner1.xz, uv);
    Sample.y  = Corner0.y;
    return Sample;
}
```

In this example, the light source is a rectangular area light in XZ plane directed down along the Y axis.
Its properties are constant:

```hlsl
float  fLightArea       = g_Constants.f2LightSizeXZ.x * g_Constants.f2LightSizeXZ.y * 2.0;
float3 f3LightIntensity = g_Constants.f4LightIntensity.rgb * g_Constants.f4LightIntensity.a;
float3 f3LightNormal    = float3(0.0, -1.0, 0.0);
```

After the random point on the light is retrieved, a shadow ray is cast towards that point to test
visibility:

```hlsl
RayInfo ShadowRay;
ShadowRay.Origin = f3SamplePos;
ShadowRay.Dir    = f3DirToLight;
float fLightVisibility = TestShadow(ShadowRay);
```

`TestShadow` function casts a ray through the scene and returns 1 if it hits no objects, and 0 otherwise.

The path radiance is then updated as follows:

```hlsl
f3PathContrib +=
    f3Attenuation
    * f3BRDF
    * max(dot(f3DirToLight, f3Normal), 0.0)
    * fLightVisibility
    * f3LightIntensity
    * fLightProjectedArea;
```

Where:

- `f3Attenuation` is the path attenuation. Initially it is `float3(1, 1, 1)`, and is multiplied by 
  surface albedo at each bounce
- `f3BRDF` is the Lambertian BRDF (perfectly diffuse surface), and equals to `f3Albedo / PI`
- `max(dot(f3DirToLight, f3Normal), 0.0)` is the 'N dot L' term
- `fLightVisibility` is the light visibility computed by casting the shadow ray
- `f3LightIntensity` is the light intensity constant
- `fLightProjectedArea` is the light surface area projected onto the hemisphere

In Monte-Carlo integration, we pretend that each sample speaks for the full light
source surface, so we project the entire light surface area onto the hemisphere
around the shaded point and see how much solid angle it covers. This value is given
by `fLightProjectedArea`.

After adding the light contribution, we go to the next sample by selecting a random
direction using the cosine-weighted hemispherical distribution:

```hlsl
RayInfo Ray;
Ray.Origin = f3SamplePos;
Ray.Dir    = SampleDirectionCosineHemisphere(f3Normal, rnd2);
// Trace the scene in the selected direction
HitInfo Hit = IntersectScene(Ray, g_Constants.f2LightPosXZ, g_Constants.f2LightSizeXZ);
```

Cosine-weighted distribution assigns more random samples near the normal direction and fewer
at the horizon, since they produce lesser contribution due to the 'N dot L' term.

At this point, if we hit the light, we stop the loop - the light contribution is accounted for
separately:

```hlsl
if (Hit.Type != HIT_TYPE_LAMBERTIAN)
    break;
```

Finally, we multiply the attenuation with the current surface albedo and
update the sample properties:

```hlsl
f3Attenuation *= f3Albedo;

// Update current sample properties
f3SamplePos = Ray.Origin + Ray.Dir * Hit.Distance;
f3Albedo    = Hit.Color;
f3Normal    = Hit.Normal;
```

The process then repeats for the next surface sample location.

After all bounces in the path are traced, the total path contribution is combined
with the emissive term from the G-buffer and is added to the total radiance:

```hlsl
f3Radiance += f3PathContrib + f3Emissive;
```

After all samples are traced, the shader adds the total radiance to the accumulation
buffer:

```hlsl
if (g_Constants.fLastSampleCount > 0)
    f3Radiance += g_Radiance[ThreadId.xy].rgb;
g_Radiance[ThreadId.xy] = float4(f3Radiance, 0.0);
```

The `fLastSampleCount` indicates how many samples have been accumulated so far.
Zero value indicates that the shader is executed for the first time and in this
case it overwrites the previous value.


### Resolve

Radiance accumulation buffer resolve is done in another full-screen render pass and
is pretty straightforward: it simply averages all the accumulated paths:

```hlsl
void main(in  PSInput  PSIn,
          out PSOutput PSOut)
{
    PSOut.Color = g_Radiance.Load(int3(PSIn.Pos.xy, 0)) / g_Constants.fCurrSampleCount;
}
```


## Pipeline State Packaging

The three stages of the rendering process are implemented by three pipeline states. The pipelines
are defined using the Diligent Render State Notation. They are created off-line using the render
state packager and packed into a single archive. At run-time, the archive is loaded and pipelines
are unpacked using the `IDearchiver` object.

### Diligent Render State Notation

Diligent Render State Notation (DRSN) is a JSON-based render state description language.
The DRSN file conists of the following sections:

- `Imports` sections defines other DRSN files whose objects should be imported into this one,
  pretty much the same way the `#include` directive works.
- `Defaults` section defines the default values for objects defined in the file.
- `Shaders` section contains shader descriptions.
- `RenderPasses` section defines render passes used by the pipelines.
- `ResourceSignatures` section defines pipeline resource signatures.
- `Pipelines` section contains pipeline states.

All objects in DRSN files are referenced by names that must be unique for each object
category. The names are used at run-time to unpack the states.

DRSN reflects the core structures of the engine in JSON format. For example, the G-buffer PSO is
defined in DRSN as follows:

```json
"Pipelines": [
    {
        "PSODesc": {
            "Name": "G-Buffer PSO",
            "ResourceLayout": {
                "Variables": [
                    {
                        "Name": "cbConstants",
                        "ShaderStages": "PIXEL",
                        "Type": "STATIC"
                    }
                ]
            }
        },
        "GraphicsPipeline": {
            "PrimitiveTopology": "TRIANGLE_LIST",
            "RasterizerDesc": {
                "CullMode": "NONE"
            },
            "DepthStencilDesc": {
                "DepthEnable": false
            }
        },
        "pVS": {
            "Desc": {
                "Name": "Screen Triangle VS"
            },
            "FilePath": "screen_tri.vsh",
            "EntryPoint": "main"
        },
        "pPS": {
            "Desc": {
                "Name": "G-Buffer PS"
            },
            "FilePath": "g_buffer.psh",
            "EntryPoint": "main"
        }
    }
]
```

Refer to this page for more information about the
[Diligent Render State Notation](https://github.com/DiligentGraphics/DiligentTools/tree/master/RenderStatePackager#render-state-notation).


### Packager Command Line

This tutorial uses the following command line to create the archive:

```
Diligent-RenderStatePackager.exe -i RenderStates.json -r assets -s assets -o assets/StateArchive.bin --dx11 --dx12 --opengl --vulkan --metal_macos --metal_ios --print_contents
```

The command line uses the following options:

- `-i` - The input DRSN file
- `-r` - Render state notation files search directory
- `-s` - Shader search directory
- `-o` - Output archive file
- `--dx11`, `--dx12`, `--opengl`, `--vulkan`, `--metal_macos`, `--metal_ios` - device flags for which to generate
  the pipeline data. Note that devices not supported on a platform (e.g. `--metal_macos` on Windows, or `--dx11` on Linux)
  are ignored.
- `--print_contents` - Print the archive contents to the log

For the [full list of command line options](https://github.com/DiligentGraphics/DiligentTools/tree/master/RenderStatePackager#command-line-arguments),
run the packager with `-h` or `--help` option.

If you use CMake, you can define a custom command to create the archive as a build step:

```CMake
set(PSO_ARCHIVE ${CMAKE_CURRENT_SOURCE_DIR}/assets/StateArchive.bin)
set_source_files_properties(${PSO_ARCHIVE} PROPERTIES GENERATED TRUE)

set(DEVICE_FLAGS --dx11 --dx12 --opengl --vulkan --metal_macos --metal_ios)
add_custom_command(OUTPUT ${PSO_ARCHIVE} # We must use full path here!
                   COMMAND $<TARGET_FILE:Diligent-RenderStatePackager> -i RenderStates.json -r assets -s assets -o "${PSO_ARCHIVE}" ${DEVICE_FLAGS} --print_contents
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                   MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/assets/RenderStates.json"
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/screen_tri.vsh"
                           "${CMAKE_CURRENT_SOURCE_DIR}/assets/g_buffer.psh"
                           "${CMAKE_CURRENT_SOURCE_DIR}/assets/resolve.psh"
                           "${CMAKE_CURRENT_SOURCE_DIR}/assets/path_trace.csh"
                           "${CMAKE_CURRENT_SOURCE_DIR}/assets/structures.fxh"
                           "${CMAKE_CURRENT_SOURCE_DIR}/assets/scene.fxh"
                           "${CMAKE_CURRENT_SOURCE_DIR}/assets/hash.fxh"
                           "$<TARGET_FILE:Diligent-RenderStatePackager>"
                   COMMENT "Creating render state archive..."
                   VERBATIM
)
```


### Unpacking the Pipeline States

To unpack pipeline states from the archive, first create a dearciver object using the engine factory:

```cpp
RefCntAutoPtr<IDearchiver> pDearchiver;
DearchiverCreateInfo       DearchiverCI{};
m_pEngineFactory->CreateDearchiver(DearchiverCI, &pDearchiver);
```

Then read the archive data from the file and load it into the dearchiver:

```cpp
FileWrapper pArchive{"StateArchive.bin"};
auto pArchiveData = DataBlobImpl::Create();
pArchive->Read(pArchiveData);
pDearchiver->LoadArchive(pArchiveData);
```

To unpack the pipeline state from the archive, populate an instance of
`PipelineStateUnpackInfo` struct with the pipeline type and name. Also, provide
a pointer to the render device:

```cpp
PipelineStateUnpackInfo UnpackInfo;
UnpackInfo.pDevice      = m_pDevice;
UnpackInfo.PipelineType = PIPELINE_TYPE_GRAPHICS;
UnpackInfo.Name         = "Resolve PSO";
```

While most of the pipeline state parameters can be defined at build time,
some can only be specified at run-time. For instance, the swap chain format
may not be known at the archive packing time. The dearchiver gives an application
a chance to modify some of the pipeline state create properties through a special callback.
Properties that can be modified include render target and depth buffer formats, depth-stencil
state, blend state, rasterizer state. Note that properties that define the resource
layout can't be modified.
Note also that modifying properties does not affect the loading speed as no shader recompilation
or patching is necessary.

We define the callback that sets the render target formats using the `MakeCallback` helper function:

```cpp
auto ModifyResolvePSODesc = MakeCallback(
    [this](PipelineStateCreateInfo& PSODesc) {
        auto& GraphicsPSOCI    = static_cast<GraphicsPipelineStateCreateInfo&>(PSODesc);
        auto& GraphicsPipeline = GraphicsPSOCI.GraphicsPipeline;

        GraphicsPipeline.NumRenderTargets = 1;
        GraphicsPipeline.RTVFormats[0]    = m_pSwapChain->GetDesc().ColorBufferFormat;
        GraphicsPipeline.DSVFormat        = m_pSwapChain->GetDesc().DepthBufferFormat;
    });

UnpackInfo.ModifyPipelineStateCreateInfo = ModifyResolvePSODesc;
UnpackInfo.pUserData                     = ModifyResolvePSODesc;
```

Finally, we call the `UnpackPipelineState` method to create the PSO from the archive:

```cpp
pDearchiver->UnpackPipelineState(UnpackInfo, &m_pResolvePSO);
```

### Rendering

After the pipeline states are unpacked from the archive, they can be used in
a usual way. We need to create the shader resource binding objects and
initialize the variables.

To render the scene, we run each of the three stages described above. First, populate the G-buffer:

```cpp
ITextureView* ppRTVs[] = {
    m_GBuffer.pAlbedo->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),
    m_GBuffer.pNormal->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),
    m_GBuffer.pEmissive->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET),
    m_GBuffer.pDepth->GetDefaultView(TEXTURE_VIEW_RENDER_TARGET)
};
m_pImmediateContext->SetRenderTargets(_countof(ppRTVs), ppRTVs, nullptr, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

m_pImmediateContext->CommitShaderResources(m_pGBufferSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
m_pImmediateContext->SetPipelineState(m_pGBufferPSO);
m_pImmediateContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
```

Next, perform the path tracing using the compute pass:

```cpp
static constexpr Uint32 ThreadGroupSize = 8;

m_pImmediateContext->SetPipelineState(m_pPathTracePSO);
m_pImmediateContext->CommitShaderResources(m_pPathTraceSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

DispatchComputeAttribs DispatchArgs{
    (SCDesc.Width  + ThreadGroupSize - 1) / ThreadGroupSize,
    (SCDesc.Height + ThreadGroupSize - 1) / ThreadGroupSize
};
m_pImmediateContext->DispatchCompute(DispatchArgs);
```

Finally, resolve radiance:

```cpp
ITextureView* ppRTVs[] = {m_pSwapChain->GetCurrentBackBufferRTV()};
m_pImmediateContext->SetRenderTargets(_countof(ppRTVs), ppRTVs, m_pSwapChain->GetDepthBufferDSV(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

m_pImmediateContext->SetPipelineState(m_pResolvePSO);
m_pImmediateContext->CommitShaderResources(m_pResolveSRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

m_pImmediateContext->Draw({3, DRAW_FLAG_VERIFY_ALL});
```


### Controlling the Application

- *Move camera*: left mouse button + WSADQE
- *Move light*:  right mouse button

You can also change the following parameters:
- *Num bounces* - the number of bounces in each path
- *Show only last bounce* - render only the last bounce in the path
- *Samples per frame* - the number of light paths to take each frame for each pixel
- *Light intensity*, *width*, *height* and *color* - different light parameters


## Resources

1. [Ray Tracing in One Weekend](https://github.com/RayTracing/raytracing.github.io/) by P.Shirley
2. [Physically Based Rendering: From Theory to Implementation](https://www.pbrt.org/) by M.Pharr, W.Jakob, and G.Humphreys
3. [Rendering Introduction Course from TU Wien University](https://www.cg.tuwien.ac.at/courses/Rendering/VU/2021S) by B.Kerbl, and A.Celarek
