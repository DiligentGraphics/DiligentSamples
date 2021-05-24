
#ifdef METAL
#    include "RayQuery.fxh"
#endif
#include "structures.fxh"

// Vulkan and DirectX:
//   Resource indices are not allowed to vary within the wave by default.
//   When dynamic indexing is required, we have to use NonUniformResourceIndex() qualifier to avoid undefined behavior.
// Metal:
//   NonUniformResourceIndex() qualifier is not needed.
#ifndef DXCOMPILER
#    define NonUniformResourceIndex(x) x
#endif

#ifdef METAL
#    define TextureSample(Texture, Sampler, f2Coord, fLevel) Texture.sample(Sampler, f2Coord, level(fLevel))
#    define TextureLoad(Texture, u2Coord)                    Texture.read(u2Coord)
#    define TextureStore(Texture, u2Coord, f4Value)          Texture.write(f4Value, u2Coord)
#    define mul(lhs, rhs)                                    ((lhs) * (rhs))
#    define lerp(x, y, factor)                               mix((x), (y), (factor))
#else
#    define TextureSample(Texture, Sampler, f2Coord, fLevel) Texture.SampleLevel(Sampler, f2Coord, fLevel)
#    define TextureLoad(Texture, u2Coord)                    Texture.Load(int3(u2Coord, 0))
#    define TextureStore(Texture, u2Coord, f4Value)          Texture[u2Coord] = f4Value
#endif
#include "Utils.fxh"


#ifdef METAL
kernel
void CSMain(uint2                                       DTid              [[thread_position_in_grid]],
            // m_pRayTracingSceneResourcesSign
            constant GlobalConstants&                   g_Constants       [[buffer(0)]],
            const device ObjectAttribs *                g_ObjectAttribs   [[buffer(1)]],
            const device MaterialAttribs *              g_MaterialAttribs [[buffer(2)]],
            const device Vertex *                       g_VertexBuffers_0 [[buffer(3)]],
            const device Vertex *                       g_VertexBuffers_1 [[buffer(4)]],
            const device uint *                         g_IndexBuffers_0  [[buffer(5)]],
            const device uint *                         g_IndexBuffers_1  [[buffer(6)]],
            instance_acceleration_structure             g_TLAS            [[buffer(7)]],
            const array<texture2d<float>, NUM_TEXTURES> g_Textures        [[texture(0)]],
            const array<sampler, NUM_SAMPLERS>          g_Samplers        [[sampler(0)]],
            // m_pRayTracingScreenResourcesSign
            texture2d<float, access::write>             g_RayTracedTex    [[texture(5)]],
            texture2d<float>                            g_GBuffer_Normal  [[texture(6)]],
            texture2d<float>                            g_GBuffer_Depth   [[texture(7)]]
            )
{
    uint2 Dim = uint2(g_RayTracedTex.get_width(), g_RayTracedTex.get_height());

#    if NUM_MESHES != 2
#        error Update g_VertexBuffers_* and g_IndexBuffers_*
#    endif

    // Array of buffers is not supported in MSL, but we can emulate it through the array of pointers.
    const device Vertex* g_VertexBuffers[] =
    {
        g_VertexBuffers_0,
        g_VertexBuffers_1
    };
    const device uint* g_IndexBuffers[] =
    {
        g_IndexBuffers_0,
        g_IndexBuffers_1
    };
#else

RaytracingAccelerationStructure   g_TLAS;
Texture2D                         g_Textures[NUM_TEXTURES];
SamplerState                      g_Samplers[NUM_SAMPLERS];
RWTexture2D<float4>               g_RayTracedTex;
Texture2D<float>                  g_GBuffer_Depth;
Texture2D<float4>                 g_GBuffer_Normal;
ConstantBuffer<GlobalConstants>   g_Constants;
StructuredBuffer<MaterialAttribs> g_MaterialAttribs;
StructuredBuffer<ObjectAttribs>   g_ObjectAttribs;
StructuredBuffer<Vertex>          g_VertexBuffers[NUM_MESHES];
StructuredBuffer<uint>            g_IndexBuffers[NUM_MESHES];

[numthreads(8, 8, 1)]
void CSMain(uint2 DTid : SV_DispatchThreadID)
{
    uint2 Dim;
    g_RayTracedTex.GetDimensions(Dim.x, Dim.y);

#endif // METAL


    if (DTid.x >= Dim.x || DTid.y >= Dim.y)
        return;

    // Early exit for background objects
    float  Depth = TextureLoad(g_GBuffer_Depth, DTid).x;
    if (Depth == 1.0)
    {
        TextureStore(g_RayTracedTex, DTid, float4(0.0, 0.0, 0.0, 1.0));
        return;
    }

    float3 WPos        = ScreenPosToWorldPos((float2(DTid) + float2(0.5, 0.5)) / float2(Dim), Depth, g_Constants.ViewProjInv);
    float3 LightDir    = g_Constants.LightDir.xyz;
    float3 ViewRayDir  = WPos.xyz - g_Constants.CameraPos.xyz;
    float  DisToCamera = length(ViewRayDir);
    ViewRayDir        /= DisToCamera;
    float4 Color       = float4(0.0, 0.0, 0.0, 1.0);
    float3 WNormal     = normalize(TextureLoad(g_GBuffer_Normal, DTid).xyz);
    float  NdotL       = max(0.0, dot(LightDir, WNormal));

    // Cast shadow
    if (NdotL > 0.0)
    {
        RayDesc ShadowRay;
        ShadowRay.Origin    = WPos.xyz + WNormal.xyz * SMALL_OFFSET * DisToCamera;
        ShadowRay.Direction = LightDir;
        ShadowRay.TMin      = 0.0;
        ShadowRay.TMax      = g_Constants.MaxRayLength;

        // Cull front faces to avaid self-intersections.
        RayQuery<RAY_FLAG_CULL_FRONT_FACING_TRIANGLES> ShadowQuery;

        // Setup ray tracing query
        ShadowQuery.TraceRayInline(g_TLAS,         // Acceleration Structure
                                   RAY_FLAG_NONE,  // Ray Flags
                                   ~0,             // Instance Inclusion Mask
                                   ShadowRay);

        // Find the first intersection.
        // If a scene contains non-opaque objects then Proceed() may return TRUE until all intersections are processed or Abort() is called.
        // This behaviour is not supported by Metal RayQuery emulation, so Proceed() already returns FALSE.
        ShadowQuery.Proceed();

        // Scene contains only triangle BLAS so we don't need to check COMMITTED_PROCEDURAL_PRIMITIVE_HIT
        if (ShadowQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
            NdotL = 0.0; // found intersection
    }

    // Reflection
    {
        RayDesc ReflRay;
        ReflRay.Origin    = WPos.xyz + WNormal.xyz * SMALL_OFFSET * DisToCamera;
        ReflRay.Direction = reflect(ViewRayDir, WNormal);
        ReflRay.TMin      = 0.0;
        ReflRay.TMax      = g_Constants.MaxRayLength;

        // Rasterization PSO uses back face culling, so we use the same culling for ray traced reflections.
        RayQuery<RAY_FLAG_CULL_BACK_FACING_TRIANGLES> ReflQuery;

        ReflQuery.TraceRayInline(g_TLAS,         // Acceleration Structure
                                 RAY_FLAG_NONE,  // Ray Flags
                                 ~0,             // Instance Inclusion Mask
                                 ReflRay);
        ReflQuery.Proceed();

        // Sample texture in intersection point
        if (ReflQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
        {
            // Read object and material attribs by InstanceID
            uint            InstId = ReflQuery.CommittedInstanceID();
            ObjectAttribs   Obj    = g_ObjectAttribs[InstId];
            MaterialAttribs Mtr    = g_MaterialAttribs[Obj.MaterialId];

            // Read triangle vertices
            uint   PrimInd       = ReflQuery.CommittedPrimitiveIndex();
            uint3  TriangleInd   = uint3(g_IndexBuffers[Obj.MeshId][Obj.FirstIndex + PrimInd * 3 + 0],
                                         g_IndexBuffers[Obj.MeshId][Obj.FirstIndex + PrimInd * 3 + 1],
                                         g_IndexBuffers[Obj.MeshId][Obj.FirstIndex + PrimInd * 3 + 2]);
            Vertex Vert0 = g_VertexBuffers[Obj.MeshId][TriangleInd.x];
            Vertex Vert1 = g_VertexBuffers[Obj.MeshId][TriangleInd.y];
            Vertex Vert2 = g_VertexBuffers[Obj.MeshId][TriangleInd.z];

            // Calculate triangle barycentric coordinates.
            float3 Barycentrics;
            Barycentrics.yz = ReflQuery.CommittedTriangleBarycentrics();
            Barycentrics.x  = 1.0 - Barycentrics.y - Barycentrics.z;

            // Calculate UV and normal for the intersection point.
            float2 UV   = float2(Vert0.U, Vert0.V) * Barycentrics.x +
                          float2(Vert1.U, Vert1.V) * Barycentrics.y +
                          float2(Vert2.U, Vert2.V) * Barycentrics.z;
            float3 Norm = float3(Vert0.NormX, Vert0.NormY, Vert0.NormZ) * Barycentrics.x +
                          float3(Vert1.NormX, Vert1.NormY, Vert1.NormZ) * Barycentrics.y +
                          float3(Vert2.NormX, Vert2.NormY, Vert2.NormZ) * Barycentrics.z;

            // Transform normal to world space.
            Norm = normalize(mul(Norm, (float3x3)Obj.NormalMat));

            // Ray tracing shaders don't support LOD calculation, so we must specify LOD and apply filtering.
            const float DefaultLOD = 1.0;
            float4 BaseColor = Mtr.BaseColorMask * TextureSample(g_Textures[NonUniformResourceIndex(Mtr.BaseColorTexInd)], g_Samplers[NonUniformResourceIndex(Mtr.SampInd)], UV, DefaultLOD);

            float  ReflNdotL = max(0.0, dot(LightDir, Norm));

            // Cast shadow
            if (ReflNdotL > 0.0)
            {
                // Calculate world-space position for intersection point, which will be used as ray origin for ray traced shadow
                float3 ReflWPos  = ReflRay.Origin + ReflRay.Direction * ReflQuery.CommittedRayT();

                RayDesc ShadowRay;
                ShadowRay.Origin    = ReflWPos + Norm * SMALL_OFFSET * length(ReflWPos - g_Constants.CameraPos.xyz);
                ShadowRay.Direction = LightDir;
                ShadowRay.TMin      = 0.0;
                ShadowRay.TMax      = g_Constants.MaxRayLength;

                RayQuery<RAY_FLAG_CULL_FRONT_FACING_TRIANGLES> ShadowQuery;

                ShadowQuery.TraceRayInline(g_TLAS,         // Acceleration Structure
                                           RAY_FLAG_NONE,  // Ray Flags
                                           ~0,             // Instance Inclusion Mask
                                           ShadowRay);
                ShadowQuery.Proceed();

                if (ShadowQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
                    ReflNdotL = 0.0; // found intersection
            }

            Color = BaseColor * max(g_Constants.AmbientLight, ReflNdotL);
        }
        else
            Color = GetSkyColor(ReflRay.Direction, g_Constants.LightDir.xyz);
    }

    Color.a = max(g_Constants.AmbientLight, NdotL);

    TextureStore(g_RayTracedTex, DTid, Color);
}