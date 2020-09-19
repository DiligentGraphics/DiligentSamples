# Tutorial20 - Mesh shader

This tutorial deomstrates how to use mesh shaders for frustum culling and LOD calculation.

Mesh shader is designed to replace the old primitive generation pipeline with a vertex attribute fetch stage, vertex shader, tessellation shaders and geometry shader.</br>
Amplification (task) shader adds a second indirection level, it can generate up to 65k mesh shader invocations and can be used for culling, LOD selection, etc.</br>
Mesh shaders supported in DirectX 12.2 and as an extension in Vulkan.

## Amplification shader

The amplification shader looks like a compute shader. We use 32 threads per group.
```hlsl
#define GROUP_SIZE 32

[numthreads(GROUP_SIZE,1,1)]
void main(in uint I : SV_GroupIndex,
          in uint wg : SV_GroupID)
{
```

Shared memory used to store data that will be used in mesh shaders. To avoid data races each thread accesses a unique index in the array.
```hlsl
struct Payload
{
    float PosX[GROUP_SIZE];
    float PosY[GROUP_SIZE];
    float PosZ[GROUP_SIZE];
    float Scale[GROUP_SIZE];
    float LODs[GROUP_SIZE];
};
groupshared Payload s_Payload;
```

We need a view matrix and cotangent of half FOV to calculate cube detail level (LOD).
Six frustum planes used for frustum culling. Elapsed time used to update cube animation.
```hlsl
cbuffer Constants
{
    float4x4 g_ViewMat;
    float4x4 g_ViewProjMat;
    float4   g_Frustum[6];
    float    g_CoTanHalfFov;
    float    g_ElapsedTime;
    bool     g_FrustumCulling;
    bool     g_Animate;
};
```

Because mesh and amplification shaders don't have any built-in input attributes except thread ID and group ID, we need a way to pass draw commands to them.
In real applications for each render object we need at least transformation, mesh ID and material ID, in this tutorial for simplification we have much less information.
`BasePos` - is cube position in 2D grid, 3D position will be calculated depending on `Time`, `Scale` - is cube scale.
```hlsl
struct DrawTask
{
    float2 BasePos;  // read-only
    float  Scale;    // read-only
    float  Time;     // read-write
};
RWStructuredBuffer<DrawTask> DrawTasks;

void main(...)
{
    ...

    const uint gid   = wg * GROUP_SIZE + I;
    DrawTask   task  = DrawTasks[gid];
    float3     pos   = float3(task.BasePos, 0.0).xzy;
    float      scale = task.Scale;
    float      time  = task.Time;

    // simulation
    pos.y = sin(time);

    if (g_Animate)
    {
        // write new time to draw task
        DrawTasks[gid].Time = time + g_ElapsedTime;
    }
```

In the amplification shader we need just a radius of the circumscribed sphere for the cube. Radius will be used for frustum culling.
```hlsl
cbuffer CubeData
{
    float4 g_SphereRadius;
    ...
};
```

Buffer with a single `uint` variable used to count the number of visible cubes after frustum culling. Value is not used in shaders, but used in UI.
```hlsl
RWByteAddressBuffer  Statistics;
```

Shared variable `s_TaskCount` used to generate a unique index for each thread after frustum culling.
To avoid data races only one thread initialize value by zero, then add memory barrier to make changes visible to other threads
and add an execution barrier to wait until all threads reach this point.
```hlsl
groupshared uint s_TaskCount;

[numthreads(GROUP_SIZE,1,1)]
void main(in uint I : SV_GroupIndex,
          in uint wg : SV_GroupID)
{
    // initialize shared variable
    if (I == 0)
    {
        s_TaskCount = 0;
    }

    // flush cache and synchronize
    GroupMemoryBarrierWithGroupSync();
    ...
```

After frustum culling we atomically increase value in `s_TaskCount`, in `index` we get a unique index to access arrays in the payload.
```hlsl
    if (!g_FrustumCulling || IsVisible(pos, g_SphereRadius.x * scale))
    {
        uint index = 0;
        InterlockedAdd(s_TaskCount, 1, index);
        
        s_Payload.PosX[index]  = pos.x;
        s_Payload.PosY[index]  = pos.y;
        s_Payload.PosZ[index]  = pos.z;
        s_Payload.Scale[index] = scale;
        s_Payload.LODs[index]  = CalcDetailLevel(pos, g_SphereRadius.x * scale);
    }
```

At the end we wait until all threads reach this point, then we can safely read value from `s_TaskCount`.
Only one thread writes value to the `Statistics` buffer, this is much faster then writes from each thread.
Then we call `DispatchMesh()` with a number of groups and payload. For compatibility with Vulkan API you should use only X group count.
The `DispatchMesh()` function must be called exactly once per amplification shader.
The `DispatchMesh()` call implies a `GroupMemoryBarrierWithGroupSync()`, and ends the amplification shader group's execution.
```hlsl
    GroupMemoryBarrierWithGroupSync();

    if (I == 0)
    {
        // update statistics
        uint origin;
        Statistics.InterlockedAdd(0, s_TaskCount, origin);
    }
    
    DispatchMesh(s_TaskCount, 1, 1, s_Payload);
```

For frustum culling we calculate distance from each plane to a sphere.
The `g_Frustum[i].xyz` contains plane normal, `g_Frustum[i].w` contains distance to plane.
The sphere is visible when the distance from each plane is greater than or equal to the radius of the sphere.
```hlsl
bool IsVisible(float3 cubeCenter, float radius)
{
    float4 center = float4(cubeCenter, 1.0);

    for (int i = 0; i < 6; ++i)
    {
        if (dot(g_Frustum[i], center) < -radius)
            return false;
    }
    return true;
}
```

For LOD calculation we calculate sphere radius in screen space.
For a detailed analysis of the algorithm, see the link in [Further Reading](#further-reading).
```hlsl
float CalcDetailLevel(float3 cubeCenter, float radius)
{
    // cubeCenter is also the center of the sphere. 
    // radius - radius of circumscribed sphere
    
    // get position in view space
    float3 pos   = mul(float4(cubeCenter, 1.0), g_ViewMat).xyz;
    
    // square of distance from camera to circumscribed sphere
    float  dist2 = dot(pos, pos);
    
    // calculate sphere size in screen space
    float  size  = g_CoTanHalfFov * radius / sqrt(dist2 - radius * radius);
    
    // calculate detail level
    float  level = clamp(1.0 - size, 0.0, 1.0);
    return level;
}
```

## Mesh shader

We use only 24 threads of 32 maximum available.
We produce 24  vertices and 12 primitives with 36 indices.
`SV_GroupIndex` used to access the mesh shader output.
`SV_GroupID` used to access amplification shader output
```hlsl
[numthreads(24,1,1)]
[outputtopology("triangle")]
void main(in uint I   : SV_GroupIndex,
          in uint gid : SV_GroupID,
          in  payload  Payload  payload,
          out indices  uint3    tris[12],
          out vertices PSInput  verts[24])
{
    // Only the input values from the the first active thread are used.
    SetMeshOutputCounts(24, 12);
```

Read amplification shader output.
```hlsl
float3 pos;
float  scale = payload.Scale[gid];
float  LOD   = payload.LODs[gid];
pos.x = payload.PosX[gid];
pos.y = payload.PosY[gid];
pos.z = payload.PosZ[gid];
```

In the mesh shader we read vertex attributes and indices.
```hlsl
cbuffer CubeData
{
    ...
    float4 g_Positions[24];
    float4 g_UVs[24];
    uint4  g_Indices[36 / 3]; // 3 indices per element
};
```

Each thread writes to only one vertex.
```hlsl
verts[I].Pos = mul(float4(pos + g_Positions[I].xyz * scale, 1.0), g_ViewProjMat);
verts[I].UV  = g_UVs[I].xy;
```

LOD doesn't affect the vertices count, just show LOD with different colors.
```hlsl
float4 Rainbow(float factor)
{
    float  h   = factor / 1.35;
    float3 col = float3(abs(h * 6.0 - 3.0) - 1.0, 2.0 - abs(h * 6.0 - 2.0), 2.0 - abs(h * 6.0 - 4.0));
    return float4(clamp(col, float3(0.0, 0.0, 0.0), float3(1.0, 1.0, 1.0)), 1.0);
}
...

verts[I].Color = Rainbow(LOD);
```

Only 12 threads write indices. You must not access arrays outside of its ranges.
```hlsl
if (I < 12)
{
    tris[I] = g_Indices[I].xyz;
}
```


## Creating cube

Cube creation has been changed - now we don’t need vertex and index buffers, mesh shader will read data from shader resource.
We use a constant buffer because we have only one small mesh, if we have a big number of large meshes we should use an unordered access buffer.
All elements in the array in constant buffer must be aligned to 16 bytes.
Also we store the radius of the circumscribed sphere for the cube.
```cpp
struct CubeData
{
    float4 sphereRadius;
    float4 pos[24];
    float4 uv[24];
    uint4  indices[36 / 3];
};

const float4 CubePos[] =
{
    float4(-1,-1,-1,0), float4(-1,+1,-1,0), float4(+1,+1,-1,0), float4(+1,-1,-1,0),
    float4(-1,-1,-1,0), float4(-1,-1,+1,0), float4(+1,-1,+1,0), float4(+1,-1,-1,0),
    float4(+1,-1,-1,0), float4(+1,-1,+1,0), float4(+1,+1,+1,0), float4(+1,+1,-1,0),
    float4(+1,+1,-1,0), float4(+1,+1,+1,0), float4(-1,+1,+1,0), float4(-1,+1,-1,0),
    float4(-1,+1,-1,0), float4(-1,+1,+1,0), float4(-1,-1,+1,0), float4(-1,-1,-1,0),
    float4(-1,-1,+1,0), float4(+1,-1,+1,0), float4(+1,+1,+1,0), float4(-1,+1,+1,0)
};

const float4 CubeUV[] = 
{
    float4(0,1,0,0), float4(0,0,0,0), float4(1,0,0,0), float4(1,1,0,0),
    float4(0,1,0,0), float4(0,0,0,0), float4(1,0,0,0), float4(1,1,0,0),
    float4(0,1,0,0), float4(1,1,0,0), float4(1,0,0,0), float4(0,0,0,0),
    float4(0,1,0,0), float4(0,0,0,0), float4(1,0,0,0), float4(1,1,0,0),
    float4(1,0,0,0), float4(0,0,0,0), float4(0,1,0,0), float4(1,1,0,0),
    float4(1,1,0,0), float4(0,1,0,0), float4(0,0,0,0), float4(1,0,0,0)
};

const uint4 Indices[] =
{
    uint4{2,0,1,0},    uint4{2,3,0,0},
    uint4{4,6,5,0},    uint4{4,7,6,0},
    uint4{8,10,9,0},   uint4{8,11,10,0},
    uint4{12,14,13,0}, uint4{12,15,14,0},
    uint4{16,18,17,0}, uint4{16,19,18,0},
    uint4{20,21,22,0}, uint4{20,22,23,0}
};

CubeData Data;
Data.sphereRadius = float4{length(CubePos[0] - CubePos[1]) * sqrt(3.0f) * 0.5f, 0, 0, 0};
std::memcpy(Data.pos, CubePos, sizeof(CubePos));
std::memcpy(Data.uv, CubeUV, sizeof(CubeUV));
std::memcpy(Data.indices, Indices, sizeof(Indices));

BufferDesc BuffDesc;
BuffDesc.Name          = "Cube vertex & index buffer";
BuffDesc.Usage         = USAGE_STATIC;
BuffDesc.BindFlags     = BIND_UNIFORM_BUFFER;
BuffDesc.uiSizeInBytes = sizeof(Data);

BufferData BufData;
BufData.pData    = &Data;
BufData.DataSize = sizeof(Data);

m_pDevice->CreateBuffer(BuffDesc, &BufData, &m_CubeBuffer);
```

## Draw task initialization

Here we initialize draw tasks that will be used in the amplification shader.
Just distribute cubes on a 2D grid and add random scale and animation time offset.
```cpp
struct DrawTask
{
    float2 BasePos;
    float  Scale;
    float  Time;
};

const int2          GridDim{128, 128};
FastRandReal<float> Rnd{0, 0.f, 1.f};

std::vector<DrawTask> DrawTasks;
DrawTasks.resize(GridDim.x * GridDim.y);

for (int y = 0; y < GridDim.y; ++y)
{
    for (int x = 0; x < GridDim.x; ++x)
    {
        int   idx = x + y * GridDim.x;
        auto& dst = DrawTasks[idx];

        dst.BasePos.x = (x - GridDim.x / 2) * 4.f + (Rnd() * 2.f - 1.f);
        dst.BasePos.y = (y - GridDim.y / 2) * 4.f + (Rnd() * 2.f - 1.f);
        dst.Scale     = Rnd() * 0.5f + 0.5f; // 0.5 .. 1
        dst.Time      = Rnd() * PI_F;
    }
}
```

We use an unordered access buffer because data size may be greater than supported by a constant buffer.
```cpp
BufferDesc BuffDesc;
BuffDesc.Name          = "Draw tasks buffer";
BuffDesc.Usage         = USAGE_DEFAULT;
BuffDesc.BindFlags     = BIND_UNORDERED_ACCESS;
BuffDesc.Mode          = BUFFER_MODE_RAW;
BuffDesc.uiSizeInBytes = sizeof(DrawTasks[0]) * Uint32(DrawTasks.size());

BufferData BufData;
BufData.pData    = DrawTasks.data();
BufData.DataSize = BuffDesc.uiSizeInBytes;

m_pDevice->CreateBuffer(BuffDesc, &BufData, &m_pDrawTasks);
```

## Initializing the Pipeline State

The creation of shaders is the same, it differs only in the types of shaders.
Some fields of the `GraphicsPipeline` like `LayoutElements` and `PrimitiveTopology` are not supported by mesh shaders and will be ignored.
```cpp
PSODesc.PipelineType  PIPELINE_TYPE_MESH;

RefCntAutoPtr<IShader> pAS;
{
    ShaderCI.Desc.ShaderType = SHADER_TYPE_AMPLIFICATION;
    ShaderCI.EntryPoint      = "main";
    ShaderCI.Desc.Name       = "Mesh shader - AS";
    ShaderCI.FilePath        = "cube.ash";
    m_pDevice->CreateShader(ShaderCI, &pAS);
}

RefCntAutoPtr<IShader> pMS;
{
    ShaderCI.Desc.ShaderType = SHADER_TYPE_MESH;
    ShaderCI.EntryPoint      = "main";
    ShaderCI.Desc.Name       = "Mesh shader - MS";
    ShaderCI.FilePath        = "cube.msh";
    m_pDevice->CreateShader(ShaderCI, &pMS);
}

RefCntAutoPtr<IShader> pPS;
{
    ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
    ShaderCI.EntryPoint      = "main";
    ShaderCI.Desc.Name       = "Mesh shader - PS";
    ShaderCI.FilePath        = "cube.psh";
    m_pDevice->CreateShader(ShaderCI, &pPS);
}
    
PSODesc.GraphicsPipeline.pAS = pAS;
PSODesc.GraphicsPipeline.pMS = pMS;
PSODesc.GraphicsPipeline.pPS = pPS;
m_pDevice->CreatePipelineState(PSOCreateInfo, &m_pPSO);
```

## Rendering

Calculate field of view (FOV) and cotangent of half FOV, these values are used to build projection matrix and to calculate LODs in shader.
```cpp
const float m_FOV            = PI_F / 4.0f;
const float m_CoTanHalfFov   = 1.0f / std::tan(m_FOV * 0.5f);
```

Upload values that are used in shaders.
```cpp
MapHelper<Constants> CBConstants(m_pImmediateContext, m_pConstants, MAP_WRITE, MAP_FLAG_DISCARD);
CBConstants->ViewMat        = m_ViewMatrix.Transpose();
CBConstants->ViewProjMat    = m_ViewProjMatrix.Transpose();
CBConstants->CoTanHalfFov   = m_LodScale * m_CoTanHalfFov;
CBConstants->FrustumCulling = m_FrustumCulling;
CBConstants->ElapsedTime    = m_ElapsedTime;
CBConstants->Animate        = m_Animate;
```

`ExtractViewFrustumPlanesFromMatrix()` calculates unnormalized frustum planes from the view-projection matrix.
We need to normalize them to test with spheres.
```cpp
ViewFrustum Frustum;
ExtractViewFrustumPlanesFromMatrix(m_ViewProjMatrix, Frustum, false);

for (uint i = 0; i < _countof(CBConstants->Frustum); ++i)
{
    Plane3D plane  = Frustum.GetPlane(ViewFrustum::PLANE_IDX(i));
    float   invlen = 1.0f / length(plane.Normal);
    plane.Normal *= invlen;
    plane.Distance *= invlen;

    CBConstants->Frustum[i] = plane;
}
```

Amplification shader has 32 threads per group, so we need to dispatch just a `m_DrawTaskCount / 32` groups.
```cpp
DrawMeshAttribs drawAttrs(m_DrawTaskCount / 32, DRAW_FLAG_VERIFY_ALL);
m_pImmediateContext->DrawMesh(drawAttrs);
```

## Further Reading
[Introduction to Turing Mesh Shaders](https://developer.nvidia.com/blog/introduction-turing-mesh-shaders/)</br>
[Vulkan spec](https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html#shaders-task)</br>
[GLSL spec](https://github.com/KhronosGroup/GLSL/blob/master/extensions/nv/GLSL_NV_mesh_shader.txt)</br>
[DirectX spec](https://microsoft.github.io/DirectX-Specs/d3d/MeshShader.html)</br>
[Radius of projected sphere in screen space](https://stackoverflow.com/a/21649403)</br>
