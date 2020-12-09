# Tutorial21 - Ray tracing

This tutorial demonstrates how to use ray tracing shaders and acceleration structures.

Unlike the rasterization, ray tracing provides a physically correct way to simulate lighting.
In ray tracing we trace light path through the scene, the light may be blocked by opaque objects that create shadows, may pass through transparent objects or may reflect from one object to another.
The final color is a combination of all of these interactions.


## Create bottom-level acceleration structures

Acceleration structures used to efficiently compute intersections of rays with geometry. Bottom-level acceleration structures (BLAS) represent the scene geometry.
Single BLAS may contain only triangle geometry or only axis-aligned bounding boxes (AABBs).</br>
For textured cube we use the same data as used in previous tutorials, but data divided on vertex positions which used for BLAS creation
and on UV, normals, indices, primitives which passed to the closest hit shader in uniform buffer.

Here we specify single triangle geometry with 24 vertices and 12 primitives. BLAS will allocate space that is enough for this geometry description.
In this tutorial `GeometryName` is not used anywhere else except BLAS build, but in other cases geometry name may be used to change geometry data using BLAS update operation,
also geometry name may be used in shader binding table as described below (see [Shader binding table](#create-shader-binding-table)).
```cpp
BLASTriangleDesc Triangles;
Triangles.GeometryName         = "Cube";
Triangles.MaxVertexCount       = _countof(CubePos);
Triangles.VertexValueType      = VT_FLOAT32;
Triangles.VertexComponentCount = 3;
Triangles.MaxPrimitiveCount    = _countof(Indices) / 3;
Triangles.IndexType            = VT_UINT32;

BottomLevelASDesc ASDesc;
ASDesc.Name          = "Cube BLAS";
ASDesc.Flags         = RAYTRACING_BUILD_AS_PREFER_FAST_TRACE;
ASDesc.pTriangles    = &Triangles;
ASDesc.TriangleCount = 1;

m_pDevice->CreateBLAS(ASDesc, &m_pCubeBLAS);
```

Vertex and index buffer creation is the same as in previous tutorials except we use `BIND_RAY_TRACING` to allow access to buffers during BLAS build.
All buffers which are used in BLAS or TLAS build commands must be created with `BIND_RAY_TRACING` flag.
We must create a scratch buffer which will store temporary data during the BLAS build, call `m_pCubeBLAS->GetScratchBufferSizes()` to get a minimal buffer size.
```cpp
BLASBuildTriangleData TriangleData;
TriangleData.GeometryName         = Triangles.GeometryName;
TriangleData.pVertexBuffer        = CubeVertexBuffer;
TriangleData.VertexStride         = sizeof(CubePos[0]);
TriangleData.VertexCount          = Triangles.MaxVertexCount;
TriangleData.VertexValueType      = Triangles.VertexValueType;
TriangleData.VertexComponentCount = Triangles.VertexComponentCount;
TriangleData.pIndexBuffer         = CubeIndexBuffer;
TriangleData.PrimitiveCount       = Triangles.MaxPrimitiveCount;
TriangleData.IndexType            = Triangles.IndexType;
TriangleData.Flags                = RAYTRACING_GEOMETRY_FLAG_OPAQUE;

BuildBLASAttribs Attribs;
Attribs.pBLAS             = m_pCubeBLAS;
Attribs.pTriangleData     = &TriangleData;
Attribs.TriangleDataCount = 1;
Attribs.pScratchBuffer    = pScratchBuffer;

m_pImmediateContext->BuildBLAS(Attribs);
```

BLAS creation and building for procedural geometry has similar code and will not be considered here.


## Create top-level acceleration structure

Top-level acceleration structure (TLAS) represents the scene. TLAS contains multiple instances that refer to a single BLAS. Instance is something like a single scene object.
To create TLAS we must specify only a number of instances. Additionally we specify `RAYTRACING_BUILD_AS_ALLOW_UPDATE` flag to update TLAS with different instance transformation,
`RAYTRACING_BUILD_AS_PREFER_FAST_TRACE` flag to allow GPU to make some optimization of the expense of building performance.

```cpp
TopLevelASDesc TLASDesc;
TLASDesc.Name             = "TLAS";
TLASDesc.MaxInstanceCount = NumInstances;
TLASDesc.Flags            = RAYTRACING_BUILD_AS_ALLOW_UPDATE | RAYTRACING_BUILD_AS_PREFER_FAST_TRACE;
m_pDevice->CreateTLAS(TLASDesc, &m_pTLAS);
```

`InstanceName` used in TLAS update operation to match instance data to the previous instance state and used in the shader binding table to bind hit groups to instances (see [Shader binding table](#create-shader-binding-table)).</br>
Instance index in the array passed to hit shaders as a `InstanceIndex()` and has very limited usage, `CustomId` specified by the user and passed to a hit shader as a `InstanceID()`,
`CustomId` may be used to apply different materials to each instance with the same geometry.</br>
`Mask` can be used to group instances and trace rays only on selected groups.
```cpp
Instances[0].InstanceName = "Cube Instance 1";
Instances[0].CustomId     = 0; // texture index
Instances[0].pBLAS        = m_pCubeBLAS;
Instances[0].Mask         = OPAQUE_GEOM_MASK;

Instances[5].InstanceName = "Sphere Instance";
Instances[5].CustomId     = 0; // box index
Instances[5].pBLAS        = m_pProceduralBLAS;
Instances[5].Mask         = OPAQUE_GEOM_MASK;

Instances[6].InstanceName = "Glass Instance";
Instances[6].pBLAS        = m_pCubeBLAS;
Instances[6].Mask         = TRANSPARENT_GEOM_MASK;
```

For each instance we specify a transformation matrix with rotation and translation.
Updating instance transformation during TLAS update operation is much faster than updating BLAS with vertex transformation or with transform buffer.
```cpp
Instances[6].Transform.SetTranslation(4.0f, 4.5f, -7.0f);
```

Here we build or update TLAS.</br>
`HitGroupStride` is a number of different ray types, in this tutorial we use two ray types: primary and shadow,
In other cases you may add a secondary ray type to use optimized hit shaders for reflected rays.</br>
`BindingMode` is a hit group location calculation mode. `HIT_GROUP_BINDING_MODE_PER_INSTANCE` allow you to bind single hit group
for instance for different ray types, so it means each instance has offset as `HIT_GROUP_STRIDE * instance_index`.
```cpp
BuildTLASAttribs Attribs;
Attribs.HitGroupStride  = HIT_GROUP_STRIDE;
Attribs.BindingMode     = HIT_GROUP_BINDING_MODE_PER_INSTANCE;
```

Instance buffer will store instance data during TLAS build operation.
As well as for BLAS build operation, the scratch buffer must be at least `m_pTLAS->GetScratchBufferSizes()` bytes for building and updating.
```cpp
Attribs.pInstances      = Instances;
Attribs.InstanceCount   = _countof(Instances);
Attribs.pInstanceBuffer = m_InstanceBuffer;
Attribs.pScratchBuffer  = m_ScratchBuffer;
Attribs.pTLAS           = m_pTLAS;
m_pImmediateContext->BuildTLAS(Attribs);
```


## Initializing the Pipeline State

The initialization of ray tracing pipeline is more complex than for other pipelines.
```cpp
RayTracingPipelineStateCreateInfo PSOCreateInfo;
PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_RAY_TRACING;
```

We use HLSL shaders with shader model 6.3, this is a minimal version with ray tracing support.
Only the DXC compiler can be used for HLSL compilation.
```cpp
ShaderCI.ShaderCompiler = SHADER_COMPILER_DXC;
ShaderCI.HLSLVersion    = {6, 3};
ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
```

Here we create shader groups that will be used by shader binding table.</br>
General shaders contain only one shader that must be ray generation, ray miss or callable.</br>
Triangle hit group contains closest hit shader and optional any-hit shader.</br>
Procedural hit group contains intersection shader and optional closest hit and any-hit shaders.
```cpp
const RayTracingGeneralShaderGroup GeneralShaders[] =
{
    {"Main",        pRG},
    {"PrimaryMiss", pPrimaryMiss},
    {"ShadowMiss",  pShadowMiss}
};
const RayTracingTriangleHitShaderGroup TriangleHitShaders[] =
{
    {"CubePrimaryHit",  pCubePrimaryHit},
    {"GroundHit",       pGroundHit},
    {"GlassPrimaryHit", pGlassPrimaryHit}
};
const RayTracingProceduralHitShaderGroup ProceduralHitShaders[] =
{
    {"SpherePrimaryHit", pSphereIntersection, pSpherePrimaryHit},
    {"SphereShadowHit",  pSphereIntersection}
};
```

`MaxRecursionDepth` specifies the number of recursive calls from hit or miss shaders, zero means that `TraceRay()` may be used only in ray generation shader,
non-zero values allow you to trace rays from hit shaders and miss shader, the maximum supported value is 32.</br>
`ShaderRecordSize` can be used to specify constants for each shader or instance or geometry, the maximum supported size is 4096 bytes minus shader handle size (32 bytes).</br>
`MaxAttributeSize` and `MaxPayloadSize` are used only in DirectX 12 backend, these values should be as small as possible to minimize memory usage.
```cpp
PSOCreateInfo.RayTracingPipeline.MaxRecursionDepth = MaxRecursionDepth;
PSOCreateInfo.RayTracingPipeline.ShaderRecordSize  = 0;

PSOCreateInfo.MaxAttributeSize = max(sizeof(/*BuiltInTriangleIntersectionAttributes*/float2), sizeof(ProceduralGeomIntersectionAttribs));
PSOCreateInfo.MaxPayloadSize   = max(sizeof(PrimaryRayPayload), sizeof(ShadowRayPayload));

m_pDevice->CreateRayTracingPipelineState(PSOCreateInfo, &m_pRayTracingPSO);
```


## Create shader binding table

Shader binding table (SBT) is used to specify shaders which will be used in ray tracing.
SBT must be created with the same pipeline that will be used with the trace rays command.
```cpp
ShaderBindingTableDesc SBTDesc;
SBTDesc.Name = "SBT";
SBTDesc.pPSO = m_pRayTracingPSO;
m_pDevice->CreateSBT(SBTDesc, &m_pSBT);
```

Ray generation shader is an entry point for ray tracing pipeline.
The `TraceRays()` only dispatches ray-gen shader as well as a computer shader.
```cpp
m_pSBT->BindRayGenShader("Main");
```

Miss shader will be executed if ray doesn't intersect with any object.
We have different behaviour for primary and shadow rays.
```cpp
m_pSBT->BindMissShader("PrimaryMiss", PRIMARY_RAY_INDEX);
m_pSBT->BindMissShader("ShadowMiss",  SHADOW_RAY_INDEX );
```

For primary ray we specify a hit group which will be executed when ray intersects with geometry.</br>
First argument is a TLAS object that used to map instance name to a precalculated location.</br>
Second argument is an instance name for which the hit group will be bound.</br>
The last argument is a hit group name that was defined in `TriangleHitShaders` array in pipeline initialization.
```cpp
m_pSBT->BindHitGroups(m_pTLAS, "Cube Instance 1",  PRIMARY_RAY_INDEX, "CubePrimaryHit"  );
m_pSBT->BindHitGroups(m_pTLAS, "Cube Instance 2",  PRIMARY_RAY_INDEX, "CubePrimaryHit"  );
m_pSBT->BindHitGroups(m_pTLAS, "Cube Instance 3",  PRIMARY_RAY_INDEX, "CubePrimaryHit"  );
m_pSBT->BindHitGroups(m_pTLAS, "Cube Instance 4",  PRIMARY_RAY_INDEX, "CubePrimaryHit"  );
m_pSBT->BindHitGroups(m_pTLAS, "Ground Instance",  PRIMARY_RAY_INDEX, "GroundHit"       );
m_pSBT->BindHitGroups(m_pTLAS, "Glass Instance",   PRIMARY_RAY_INDEX, "GlassPrimaryHit" );
m_pSBT->BindHitGroups(m_pTLAS, "Sphere Instance",  PRIMARY_RAY_INDEX, "SpherePrimaryHit");
```

For shadow ray we disable all hit shader invocation by using an empty shader name or nullptr.
If hit shader invocation for procedural geometry will be skipped then we will see shadow from bounding box except sphere,
so we need to bind the hit group only with the intersection shader.
```cpp
m_pSBT->BindHitGroupForAll(m_pTLAS, SHADOW_RAY_INDEX, "");
m_pSBT->BindHitGroups(m_pTLAS, "Sphere Instance",  SHADOW_RAY_INDEX,  "SphereShadowHit");
```

Also we can create TLAS with `BindingMode = HIT_GROUP_BINDING_MODE_PER_GEOMETRY` and specify hit groups per geometry:
```cpp
m_pSBT->BindHitGroup(m_pTLAS, "Cube Instance 1", "Cube", PRIMARY_RAY_INDEX, "CubePrimaryHit"  );
```
These may be useful to bind unique hit groups for each geometry.

In result SBT contains these table:

| Instance | Geometry | Ray type | Location | Shader group | Shader constants |
|---|---|---|---|---|---|
| Cube Instance 1 | Cube | primary | 0 | CubePrimaryHit | - |
| - | - | shadow | 1 | skip | - |
| Cube Instance 2 | Cube | primary | 2 | CubePrimaryHit | - |
| - | - | shadow | 3 | skip | - |
| Cube Instance 3 | Cube | primary | 4 | CubePrimaryHit | - |
| - | - | shadow | 5 | skip | - |
| Cube Instance 4 | Cube | primary | 6 | CubePrimaryHit | - |
| - | - | shadow | 7 | skip | - |
| Ground Instance | Cube | primary | 8 | GroundHit | - |
| - | - | shadow | 9 | skip | - |
| Glass Instance | Cube | primary | 10 | GlassPrimaryHit | - |
| - | - | shadow | 11 | skip | - |
| Sphere Instance | Box | primary | 12 | SpherePrimaryHit | - |
| - | - | shadow | 13 | SphereShadowHit | - |

'Shader group' and 'Shader constants' is what is actually stored into SBT, other fields in the table are used to calculate shader location.

As alternative you can use `IShaderBindingTable::BindHitGroupByIndex()` to bind hit group to any location
in range from `TLASBuildInfo::FirstContributionToHitGroupIndex` to `TLASBuildInfo::LastContributionToHitGroupIndex`,
call `ITopLevelAS::GetBuildInfo()` to get actual build info.


## Resource binding

Unlike the other pipelines, in the ray tracing pipeline we use multiple shaders with the same type.
Resources with the same name in shaders with the same types will be merged and binding points will be remapped.
In a code below constants bound to a single ray generation shader and for all closest hit shaders.
```cpp
m_pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_GEN, "g_ConstantsCB")->Set(m_ConstantsCB);
m_pRayTracingSRB->GetVariableByName(SHADER_TYPE_RAY_CLOSEST_HIT, "g_ConstantsCB")->Set(m_ConstantsCB);
```

Valid usage:
```hlsl
// in closest_hit_shader1:
ConstantBuffer<CubeAttribs>  g_CubeAttribsCB : register(b0);

// in closest_hit_shader2:
ConstantBuffer<CubeAttribs>  g_CubeAttribsCB : register(b1);
```
Register indices will be ignored and buffers will be merged. 

Invalid usage:
```hlsl
// in closest_hit_shader1:
ConstantBuffer<CubeAttribs>     g_CubeAttribsCB : register(b0);
RaytracingAccelerationStructure g_TLAS;

// in closest_hit_shader2:
ConstantBuffer<CubeAttribs>  g_CubeConstants : register(b0);
RWByteAddressBuffer          g_TLAS;
```
Buffers with different names but with the same register index will not be merged.</br>
Resources with the same name but different types can not be merged and will generate an error.


## Ray tracing

Before rendering we update instance transformations by updating TLAS.
Acceleration structure updating is much faster and takes less memory than building from scratch.
```cpp
UpdateTLAS();
```

Run ray generation shader with dimension of render target.
```cpp
TraceRaysAttribs Attribs;
Attribs.DimensionX        = m_pColorRT->GetDesc().Width;
Attribs.DimensionY        = m_pColorRT->GetDesc().Height;
Attribs.pSBT              = m_pSBT;
Attribs.SBTTransitionMode = RESOURCE_STATE_TRANSITION_MODE_TRANSITION;
m_pImmediateContext->TraceRays(Attribs);
```

Then the result of ray tracing copied to swapchain image using a simple graphics pipeline.
In more complex renderer this pass will be replaced by tone mapping or other post processes.


## Ray generation shader

We use `ExtractViewFrustumPlanesFromMatrix()` to calculate the view frustum planes from the view-projections matrix.
Then normalize planes and calculate intersections between them. In result we have 4 rays which represent the frustum and pass it to the shader.
```cpp
ViewFrustum Frustum;
ExtractViewFrustumPlanesFromMatrix(CameraViewProj, Frustum, false);

// Normalize frustum planes.
for (uint i = 0; i < ViewFrustum::NUM_PLANES; ++i)
{
    Plane3D& plane  = Frustum.GetPlane(static_cast<ViewFrustum::PLANE_IDX>(i));
    float    invlen = 1.0f / length(plane.Normal);
    plane.Normal   *= invlen;
    plane.Distance *= invlen;
}

// Calculate ray formed by the intersection of two planes.
const auto GetPlaneIntersection = [&Frustum](ViewFrustum::PLANE_IDX lhs, ViewFrustum::PLANE_IDX rhs, float4& result) {
    const Plane3D& lp = Frustum.GetPlane(lhs);
    const Plane3D& rp = Frustum.GetPlane(rhs);

    const float3 dir = cross(lp.Normal, rp.Normal);
    const float  len = dot(dir, dir);

    result = dir * (1.0f / sqrt(len));
};

GetPlaneIntersection(ViewFrustum::BOTTOM_PLANE_IDX, ViewFrustum::LEFT_PLANE_IDX,   m_Constants.FrustumRayLB);
GetPlaneIntersection(ViewFrustum::LEFT_PLANE_IDX,   ViewFrustum::TOP_PLANE_IDX,    m_Constants.FrustumRayLT);
GetPlaneIntersection(ViewFrustum::RIGHT_PLANE_IDX,  ViewFrustum::BOTTOM_PLANE_IDX, m_Constants.FrustumRayRB);
GetPlaneIntersection(ViewFrustum::TOP_PLANE_IDX,    ViewFrustum::RIGHT_PLANE_IDX,  m_Constants.FrustumRayRT);
```

In ray generation shader we calculate normalized texture coordinates `uv` and use it to calculate ray direction from frustum rays.
```hlsl
struct Constants
{
    float4   Position;
    float4   FrustumRayLT;
    float4   FrustumRayLB;
    float4   FrustumRayRT;
    float4   FrustumRayRB;
    ...
};
ConstantBuffer<Constants> g_ConstantsCB;
...

float3  rayOrigin = g_ConstantsCB.Position.xyz;
float2  uv        = float2(DispatchRaysIndex().xy) / float2(DispatchRaysDimensions().xy - 1);
float3  rayDir    = normalize(lerp(lerp(g_ConstantsCB.FrustumRayLB, g_ConstantsCB.FrustumRayRB, uv.x),
                                   lerp(g_ConstantsCB.FrustumRayLT, g_ConstantsCB.FrustumRayRT, uv.x), uv.y)).xyz;
```

Setup ray description and start ray tracing.
```hlsl
RayDesc ray;
ray.Origin    = rayOrigin;
ray.Direction = rayDir;
ray.TMin      = g_ConstantsCB.ClipPlanes.x;
ray.TMax      = g_ConstantsCB.ClipPlanes.y;

PrimaryRayPayload payload = CastPrimaryRay(ray, /*recursion number*/0);
```

Primary ray used to calculate the final color of a pixel on screen and used to calculate secondary rays for reflection and refraction.
Therefore the `TraceRay()` wrapped in `CastPrimaryRay()` function for reusability.</br>
Here we check the number of recursions, because GPU device do not skip ray tracing if you exceed the maximum recursion depth.
If you don't check recursion depth then the GPU will crash on shader stack overflow.
```hlsl
RaytracingAccelerationStructure g_TLAS;

PrimaryRayPayload CastPrimaryRay(RayDesc ray, uint Recursion)
{
    PrimaryRayPayload payload = {float3(0, 0, 0), 0.0, Recursion};

    if (Recursion > g_ConstantsCB.MaxRecursion)
    {
        // set pink color for debugging
        payload.Color = float3(0.95, 0.18, 0.95);
        return payload;
    }
    TraceRay(g_TLAS,            // Acceleration structure
             RAY_FLAG_NONE,
             ~0,                // Instance inclusion mask - all instances are visible
             PRIMARY_RAY_INDEX, // Ray contribution to hit group index
             HIT_GROUP_STRIDE,  // Multiplier for geometry contribution to hit group index
             PRIMARY_RAY_INDEX, // Miss shader index
             ray,
             payload);
    return payload;
}
```

At the end of ray generation shader execution we write payload color to UAV texture.
```hlsl
g_ColorBuffer[DispatchRaysIndex().xy] = float4(payload.Color, 1.0);
```


## Ray closest hit shader

Closest hit shader executes when ray intersects with triangle geometry or when custom intersection shader reports hit (see [Intersection shader](#intersection-shader)).
Shader call stack locks like:

![image](rt_callstack_1.png)

The closest hit shader on input takes editable ray payload and geometry intersection information.
```hlsl
[shader("closesthit")]
void main(inout PrimaryRayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
```

Convert 2-component barycentrics to 3-component triangle barycentrics.
```hlsl
float3 barycentrics = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
```

Use the built-in function `PrimitiveIndex()` to get the index of the current primitive and then get 3 vertex indices of this primitive.
```hlsl
uint3  primitive = g_CubeAttribsCB.Primitives[PrimitiveIndex()].xyz;
```

Then we use baricentrics to calculate texture coordinates for intersection point.
```hlsl
float2 uv = g_CubeAttribsCB.UVs[primitive.x].xy * barycentrics.x +
            g_CubeAttribsCB.UVs[primitive.y].xy * barycentrics.y +
            g_CubeAttribsCB.UVs[primitive.z].xy * barycentrics.z;
```

Same for triangle normal, but we need to apply transformation to get normal in world space.
```hlsl
float3 normal = g_CubeAttribsCB.Normals[primitive.x].xyz * barycentrics.x +
                g_CubeAttribsCB.Normals[primitive.y].xyz * barycentrics.y +
                g_CubeAttribsCB.Normals[primitive.z].xyz * barycentrics.z;
normal        = normalize(mul((float3x3) ObjectToWorld3x4(), normal));
```

In ray tracing `Texture2D::Sample()` method is not supported because GPU device can not implicitly calculate derivatives,
so you should calculate texture mipmap level and apply filtering by yourself. In this tutorial we don't do that and just read maximum mip level.
```hlsl
payload.Color = g_Texture[InstanceID()].SampleLevel(g_Texture_sampler, uv, 0).rgb;
```

The last step - calculate lighting.
We use built-in functions to get ray parameters and calculate intersection point.
```hlsl
float3 pos = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
LightingPass(payload.Color, pos, normal, payload.Recursion + 1);
```

Calculate simple diffusion lighting and cast ray to test if light occluded or not.
```hlsl
void LightingPass(inout float3 Color, float3 Pos, float3 Norm, uint Recursion)
{
    float3  col = float3(0.0, 0.0, 0.0);
    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        float  NdotL = max(0.0, dot(Norm, rayDir));
        
        // Optimization - don't trace rays if NdotL is zero
        if (NdotL > 0.0)
        {
            RayDesc ray;

            // Add a small offset to avoid self-intersections.
            ray.Origin    = Pos + Norm * 0.001;
            ray.Direction = normalize(g_ConstantsCB.LightPos[i].xyz - Pos);

            // Limit max ray length by distance to light source.
            ray.TMax      = distance(g_ConstantsCB.LightPos[i].xyz, Pos);
            ray.TMin      = 0.0;

            col += CastShadow(ray, Recursion).Shading * NdotL * Color * g_ConstantsCB.LightColor[i].rgb;
        }
    }
    Color = col / float(NUM_LIGHTS) + g_ConstantsCB.AmbientColor.rgb;
}
```

Here we use new payload type `ShadowRayPayload` only with 2 components to minimize memory usage.</br>
For optimizations we use some flags:</br>
`RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH` - stops ray processing if any geometry hit is encountered.</br>
`RAY_FLAG_FORCE_OPAQUE` - skip any-hit shader invocation and has no effect because we are not using any-hit shaders here.</br>
In SBT we bind null hit group for shadow ray for triangle geometry, for procedural geometry we use only intersection shader, so we don't have any closest hit shaders
and `RAY_FLAG_SKIP_CLOSEST_HIT_SHADER` flag may have no effect, but if you use the same shaders as for primary ray you may have significantly performance improvement.</br>
We use an instance mask `OPAQUE_GEOM_MASK` to skip transparent cube with refractions. Light calculation for objects with refractions is much more complex task and not considered here.</br>
The initial `Shading` value is 0 that means the object is completely shadowed, it will keep if intersection was found or will be overwritten by 1 in miss shader that means no shadows.
```hlsl
ShadowRayPayload CastShadow(RayDesc ray, uint Recursion)
{
    ShadowRayPayload payload = {0.0, Recursion};
    if (Recursion > g_ConstantsCB.MaxRecursion)
    {
        payload.Shading = 1.0;
        return payload;
    }
    TraceRay(g_TLAS,            // Acceleration structure
             RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
             OPAQUE_GEOM_MASK,  // Instance inclusion mask - only opaque instances are visible
             SHADOW_RAY_INDEX,  // Ray contribution to hit group index
             HIT_GROUP_STRIDE,  // Multiplier for geometry contribution to hit group index
             SHADOW_RAY_INDEX,  // Miss shader index
             ray,
             payload);
    return payload;
}
```

For some shading techniques this optimization can not be applied. For example: for ambient occlusion you may need correct distance to nearest occluder
instead of information about the presence or absence of intersection, so you need to allow closest hit shader invocation and write distance from `RayTCurrent()`.

With shadows shader call stack locks like:

![image](rt_callstack_2.png)

Some hardware does not support ray recursion, but you can use a loop in the ray generation shader instead.


## Ray miss shader

Miss shader executed if no one intersection was found.

![image](rt_callstack_3.png)

On input miss shader takes only editable ray payload.
```hlsl
[shader("miss")]
void main(inout PrimaryRayPayload payload)
```

Miss shader for primary ray generates sky color.</br>
Miss shader for shadow ray just writes 1.0 into payload.


## Intersection shader

Intersection shader can be used only with procedural geometry, for triangle geometry used built-in hardware accelerated intersection test.
Intersection shader executed for each ray that intersects with AABB, depends on implementation intersection shader may be executed even if AABB occluded by another geometry,
so the intersection test should be as fast as possible.

![image](rt_callstack_4.png)

Intersection shader does not take any input arguments.
```cpp
[shader("intersection")]
void main()
```

We don't have any information about intersection, so we need to get the same AABB which was used to build BLAS and calculate intersection point in the shader.
Then send intersection information to the closest hit shader.
```cpp
ProceduralGeomIntersectionAttribs attr;
...
ReportHit(hitT, RAY_KIND_PROCEDURAL_FRONT_FACE, attr);
```


## Performance

With default settings the execution time of shaders for all rays is approximately the same.

![image](rt_performance_1.jpg)


When we increase the number of rays per pixel then performance for some pixels increases dramatically.
So you should minimize multiple rays per pixel and use temporal techniques like a TAA or denoising.

![image](rt_performance_2.jpg)


## Further Reading

[DirectX spec](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html)</br>
[GLSL spec](https://github.com/KhronosGroup/GLSL/blob/master/extensions/ext/GLSL_EXT_ray_tracing.txt)</br>
[Tips and Tricks: Ray Tracing Best Practices](https://developer.nvidia.com/blog/rtx-best-practices/)</br>
[Best Practices: Using NVIDIA RTX Ray Tracing](https://developer.nvidia.com/blog/best-practices-using-nvidia-rtx-ray-tracing/)</br>
