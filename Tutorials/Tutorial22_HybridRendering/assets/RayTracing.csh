
#ifdef METAL
#    include "RayQueryMtl.fxh"
#endif

#include "Structures.fxh"
#include "Utils.fxh"

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
#    define TEXTURE_ARRAY(Name, Size)                        const array<texture2d<float>, Size> Name
#    define SAMPLER_ARRAY(Name, Size)                        const array<sampler, Size> Name
#    define BUFFER(Name, Type)                               const device Type* Name
#else
#    define TextureSample(Texture, Sampler, f2Coord, fLevel) Texture.SampleLevel(Sampler, f2Coord, fLevel)
#    define TextureLoad(Texture, u2Coord)                    Texture.Load(int3(u2Coord, 0))
#    define TextureStore(Texture, u2Coord, f4Value)          Texture[u2Coord] = f4Value
#    define TEXTURE_ARRAY(Name, Size)                        Texture2D<float4> Name[Size]
#    define SAMPLER_ARRAY(Name, Size)                        SamplerState Name[Size]
#    define BUFFER(Name, Type)                               StructuredBuffer<Type> Name
#endif

// Return 0 when occluder is found, and 1 otherwise
float CastShadow(float3 Origin, float3 RayDir, float MaxRayLength, RaytracingAccelerationStructure TLAS)
{
    RayDesc ShadowRay;
    ShadowRay.Origin    = Origin;
    ShadowRay.Direction = RayDir;
    ShadowRay.TMin      = 0.0;
    ShadowRay.TMax      = MaxRayLength;

    // Cull front faces to avaid self-intersections.
    // We don't use distance to occluder, so ray query can find any intersection and end search.
    RayQuery<RAY_FLAG_CULL_FRONT_FACING_TRIANGLES | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH> ShadowQuery;

    // Setup ray tracing query
    ShadowQuery.TraceRayInline(TLAS,            // Acceleration Structure
                                RAY_FLAG_NONE,  // Ray Flags
                                ~0,             // Instance Inclusion Mask
                                ShadowRay);

    // Find the first intersection.
    // If a scene contains non-opaque objects then Proceed() may return TRUE until all intersections are processed or Abort() is called.
    // This behaviour is not supported by Metal RayQuery emulation, so Proceed() already returns FALSE.
    ShadowQuery.Proceed();
        
    // The scene contains only triangles, so we don't need to check COMMITTED_PROCEDURAL_PRIMITIVE_HIT
    return ShadowQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT ? 0.0 : 1.0;
}

struct ReflectionInputAttribs
{
    float3 Origin;
    float3 ReflectionRayDir;
    float  MaxReflectionRayLength;
    float  MaxShadowRayLength;
    float3 CameraPos;
    float3 LightDir;
};
struct ReflectionResult
{
    float4 BaseColor;
    float  NdotL;
    bool   Found;
};
ReflectionResult Reflection(TEXTURE_ARRAY(Textures,      NUM_TEXTURES   ),
                            SAMPLER_ARRAY(Samplers,      NUM_SAMPLERS   ),
                            BUFFER(       VertexBuffer,  Vertex         ),
                            BUFFER(       IndexBuffer,   uint           ),
                            BUFFER(       Objects,       ObjectAttribs  ),
                            BUFFER(       Materials,     MaterialAttribs),
                            RaytracingAccelerationStructure TLAS,
                            ReflectionInputAttribs          In)
{
    RayDesc ReflRay;
    ReflRay.Origin    = In.Origin;
    ReflRay.Direction = In.ReflectionRayDir;
    ReflRay.TMin      = 0.0;
    ReflRay.TMax      = In.MaxReflectionRayLength;
        
    // Rasterization PSO uses back face culling, so we use the same culling for ray traced reflections.
    RayQuery<RAY_FLAG_CULL_BACK_FACING_TRIANGLES> ReflQuery;

    ReflQuery.TraceRayInline(TLAS,           // Acceleration Structure
                             RAY_FLAG_NONE,  // Ray Flags
                             ~0,             // Instance Inclusion Mask
                             ReflRay);
    ReflQuery.Proceed();
        
    ReflectionResult Result;
    Result.BaseColor = float4(0.0, 0.0, 0.0, 0.0);
    Result.NdotL     = 0.0;
    Result.Found     = false;

    // Sample texture at the intersection point
    if (ReflQuery.CommittedStatus() == COMMITTED_TRIANGLE_HIT)
    {
        // Read object and material attribs by InstanceID
        uint            InstId = ReflQuery.CommittedInstanceID();
        ObjectAttribs   Obj    = Objects[InstId];
        MaterialAttribs Mtr    = Materials[Obj.MaterialId];

        // Read triangle vertices
        uint  PrimInd     = ReflQuery.CommittedPrimitiveIndex();
        uint3 TriangleInd = uint3(IndexBuffer[Obj.FirstIndex + PrimInd * 3 + 0],
                                  IndexBuffer[Obj.FirstIndex + PrimInd * 3 + 1],
                                  IndexBuffer[Obj.FirstIndex + PrimInd * 3 + 2]);
        Vertex Vert0 = VertexBuffer[TriangleInd.x + Obj.FirstVertex];
        Vertex Vert1 = VertexBuffer[TriangleInd.y + Obj.FirstVertex];
        Vertex Vert2 = VertexBuffer[TriangleInd.z + Obj.FirstVertex];

        // Calculate triangle barycetric coordinates
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

        // Transform normal to world space
        Norm = normalize(mul(Norm, (float3x3)Obj.NormalMat));

        // Ray tracing shaders don't support LOD calculation, so we explicitly specify LOD and apply filtering
        const float DefaultLOD = 0.0;
        Result.BaseColor = Mtr.BaseColorMask *
            TextureSample(Textures[NonUniformResourceIndex(Mtr.BaseColorTexInd)],
                          Samplers[NonUniformResourceIndex(Mtr.SampInd)],
                          UV,
                          DefaultLOD);
       
        Result.NdotL = max(0.0, dot(In.LightDir, Norm));
        Result.Found = true;
            
        // Cast shadow
        if (Result.NdotL > 0.0)
        {
            // Calculate world-space position for intersection point which will be used as ray origin for ray traced shadow
            float3 ReflWPos = ReflRay.Origin + ReflRay.Direction * ReflQuery.CommittedRayT();

            Result.NdotL *= CastShadow(ReflWPos + Norm * SMALL_OFFSET * length(ReflWPos - In.CameraPos),
                                       In.LightDir,
                                       In.MaxShadowRayLength,
                                       TLAS);
        }
    }

    return Result;
}


#ifdef METAL
kernel
void CSMain(uint2                                       DTid              [[thread_position_in_grid]],
            // m_pRayTracingSceneResourcesSign
            instance_acceleration_structure             g_TLAS            [[buffer(0)]],
            constant GlobalConstants&                   g_Constants       [[buffer(1)]],
            const device ObjectAttribs *                g_ObjectAttribs   [[buffer(2)]],
            const device MaterialAttribs *              g_MaterialAttribs [[buffer(3)]],
            const device Vertex *                       g_VertexBuffer    [[buffer(4)]],
            const device uint *                         g_IndexBuffer     [[buffer(5)]],
            const array<texture2d<float>, NUM_TEXTURES> g_Textures        [[texture(0)]],
            const array<sampler, NUM_SAMPLERS>          g_Samplers        [[sampler(0)]],
            // m_pRayTracingScreenResourcesSign
            texture2d<float, access::write>             g_RayTracedTex    [[texture(5)]],
            texture2d<float>                            g_GBuffer_Normal  [[texture(6)]],
            texture2d<float>                            g_GBuffer_Depth   [[texture(7)]]
            )
{
    uint2 Dim = uint2(g_RayTracedTex.get_width(), g_RayTracedTex.get_height());
#else

RaytracingAccelerationStructure   g_TLAS;
Texture2D<float4>                 g_Textures[NUM_TEXTURES];
SamplerState                      g_Samplers[NUM_SAMPLERS];
RWTexture2D<float4>               g_RayTracedTex;
Texture2D<float>                  g_GBuffer_Depth;
Texture2D<float4>                 g_GBuffer_Normal;
ConstantBuffer<GlobalConstants>   g_Constants;
StructuredBuffer<MaterialAttribs> g_MaterialAttribs;
StructuredBuffer<ObjectAttribs>   g_ObjectAttribs;
StructuredBuffer<Vertex>          g_VertexBuffer;
StructuredBuffer<uint>            g_IndexBuffer;

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
        NdotL *= CastShadow(WPos.xyz + WNormal.xyz * SMALL_OFFSET * DisToCamera,
                            LightDir,
                            g_Constants.MaxRayLength,
                            g_TLAS);
    }

    // Reflection
    {
        ReflectionInputAttribs Attribs;
        Attribs.Origin                 = WPos.xyz + WNormal.xyz * SMALL_OFFSET * DisToCamera;
        Attribs.ReflectionRayDir       = reflect(ViewRayDir, WNormal);
        Attribs.MaxReflectionRayLength = g_Constants.MaxRayLength;
        Attribs.MaxShadowRayLength     = g_Constants.MaxRayLength;
        Attribs.CameraPos              = g_Constants.CameraPos.xyz;
        Attribs.LightDir               = LightDir;

        ReflectionResult Refl = Reflection(g_Textures,
                                           g_Samplers,
                                           g_VertexBuffer,
                                           g_IndexBuffer,
                                           g_ObjectAttribs,
                                           g_MaterialAttribs,
                                           g_TLAS,
                                           Attribs);

        if (Refl.Found)
            Color = Refl.BaseColor * max(g_Constants.AmbientLight, Refl.NdotL);
        else
            Color = GetSkyColor(Attribs.ReflectionRayDir, g_Constants.LightDir.xyz);
    }

    Color.a = max(g_Constants.AmbientLight, NdotL);

    TextureStore(g_RayTracedTex, DTid, Color);
}
