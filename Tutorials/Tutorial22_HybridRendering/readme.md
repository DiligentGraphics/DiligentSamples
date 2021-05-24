# Tutorial 22 - Hybrid rendering

This tutorial demonstrates how to organize scene with rasterization and ray tracing rendering.

## Scene

For ray tracing we need to pass all scene resources into the shader, so we use bindless resources, which means all resources are binded once and used in all draw calls.
In this tutorial we use a single vertex and index buffer for multiple meshes, but there are no restrictions to use buffer arrays for multiple index and vertex buffers.

Each scene objects described by this structure:
```cpp
struct ObjectAttribs
{
    float4x4 ModelMat;
    float4x3 NormalMat;
    uint     MaterialId;
    uint     FirstIndex;
    uint     FirstVertex;
};
```
`ModelMat` and `NormalMat` transform object local space into world space for position and normal. `MaterialId` indicates object material.
`FirstIndex` and `FirstVertex` specify the position of the first index and vertex into buffers.

For ray tracing each mesh contains bottom-level acceleration structure (BLAS). In some cases may better to merge all static meshes into single BLAS,
 it can improve ray tracing performance and speed up top-level AS building.
```cpp
struct Mesh
{
    RefCntAutoPtr<IBottomLevelAS> BLAS;
    RefCntAutoPtr<IBuffer>        VertexBuffer;
    RefCntAutoPtr<IBuffer>        IndexBuffer;
    ...
};
```

To decrease the number of draw calls objects with the same mesh merged and drawn with instancing.
```cpp
struct InstancedObjects
{
    Uint32 MeshInd             = 0;
    Uint32 ObjectAttribsOffset = 0;
    Uint32 NumObjects          = 0;
};
```
In shaders if we read resource by index we need to use NonUniformResourceIndex() qualifier, otherwise the compiler can assume that this index is constant during draw call and it can apply optimizations which cause undefined behaviour.
```cpp
void main(in  PSInput  PSIn,
          out PSOutput PSOut)
{
    MaterialAttribs Mtr = g_MaterialAttribs[NonUniformResourceIndex(PSIn.MatId)];

    PSOut.Color = Mtr.BaseColorMask * g_Textures[NonUniformResourceIndex(Mtr.BaseColorTexInd)].Sample(g_Samplers[NonUniformResourceIndex(Mtr.SampInd)], PSIn.UV);
    PSOut.Norm  = float4(normalize(PSIn.Norm), 0.0);
}
```

## Metal ray tracing

All shaders are written on HLSL, for compatibility with MSL we add a wrapper on top of Metal `raytracing::intersector<...>` which emulates `RayQuery` in HLSL.
Wrapper supports a minimal set of functions which are used in tutorial, for example there is no support for non-opaque objects which requires iterating on intersections.
Instead of using builtin `RayQuery::CommittedObjectToWorld4x3()`, as in the previous tutorial, we use matrices from `ObjectAttribs`.

`RayQuery` wrapper can be extended to use TLASInstancesBuffer:
```cpp
const device MTLAccelerationStructureInstanceDescriptor* g_TLASInstances [[buffer(0)]]
...
// in RayQuery structure
uint     CommittedInstanceIndex()                       { return g_TLASInstances[m_LastIntersection.instance_id].accelerationStructureIndex; }
uint     CommittedInstanceContributionToHitGroupIndex() { return g_TLASInstances[m_LastIntersection.instance_id].intersectionFunctionTableOffset; }
float4x3 CommittedObjectToWorld4x3()                    { return g_TLASInstances[m_LastIntersection.instance_id].transformationMatrix; }
```


## Hybrid rendering

First pass - rendering into G-Buffer. In G-Buffer we need only color and normals, world space position will be restored from the depth buffer.

Second pass - compute shader with inline ray tracing.
Function `CastShadow(...)` searches for any intersection and returns 0 if intersection found or 1 otherwise.
Function `Reflection(...)` searches for closest intersection with scene, applied material and calculates lighting.
This function can be used not only for reflections, but for example for adaptive temporal anti-aliasing to restore pixels which cannot be reprojected from previous frame,
or for screen space reflections when it failed to find intersection on screen. 

Third pass - post process pass which companies G-Buffer color and reflections.
