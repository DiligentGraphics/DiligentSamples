
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
#    define TextureDimensions(Texture, Dim)                  Dim=uint2(Texture.get_width(), Texture.get_height())

#    define TEXTURE(Name)                const texture2d<float>              Name
#    define TEXTURE_ARRAY(Name, Size)    const array<texture2d<float>, Size> Name
#    define WTEXTURE(Name)               texture2d<float, access::write>     Name
#    define SAMPLER_ARRAY(Name, Size)    const array<sampler, Size>          Name
#    define BUFFER(Name, Type)           const device Type*                  Name
#    define CONSTANT_BUFFER(Name, Type)  constant GlobalConstants&           Name
#else
#    define TextureSample(Texture, Sampler, f2Coord, fLevel) Texture.SampleLevel(Sampler, f2Coord, fLevel)
#    define TextureLoad(Texture, u2Coord)                    Texture.Load(int3(u2Coord, 0))
#    define TextureStore(Texture, u2Coord, f4Value)          Texture[u2Coord] = f4Value
#    define TextureDimensions(Texture, Dim)                  Texture.GetDimensions(Dim.x, Dim.y)

#    define TEXTURE(Name)                Texture2D<float4>      Name
#    define TEXTURE_ARRAY(Name, Size)    Texture2D<float4>      Name[Size]
#    define WTEXTURE(Name)               RWTexture2D<float4>    Name
#    define SAMPLER_ARRAY(Name, Size)    SamplerState           Name[Size]
#    define BUFFER(Name, Type)           StructuredBuffer<Type> Name
#    define CONSTANT_BUFFER(Name, Type)  ConstantBuffer<Type>   Name
#endif

// Returns 0 when occluder is found, and 1 otherwise
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

#ifdef DXCOMPILER
// Without this struct, DXC fails to compile the shader with the following error:
//
//      error: variable has incomplete type 'SamplerState [2]'
//                SAMPLER_ARRAY(Samplers,      NUM_SAMPLERS   ),
//                              ^
//
// https://github.com/microsoft/DirectXShaderCompiler/issues/4666
struct _DXCBugWorkaround_
{
    SamplerState Samplers[2];
};
#endif

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
        
    // Rasterization PSO uses back-face culling, so we use the same culling for ray traced reflections.
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
#   define BEGIN_SHADER_DECLARATION(Name) kernel void Name(
#   define END_SHADER_DECLARATION(Name, GroupXSize, GroupYSize) uint2 DTid [[thread_position_in_grid]])
#   define MTL_BINDING(type, index) [[type(index)]]
#   define END_ARG ,
#else
#   define BEGIN_SHADER_DECLARATION(Name)
#   define END_SHADER_DECLARATION(Name, GroupXSize, GroupYSize) [numthreads(GroupXSize, GroupYSize, 1)] void Name(uint2 DTid : SV_DispatchThreadID)
#   define MTL_BINDING(type, index)
#   define END_ARG ;
#endif

BEGIN_SHADER_DECLARATION(CSMain)

    // m_pRayTracingSceneResourcesSign
    RaytracingAccelerationStructure g_TLAS                              MTL_BINDING(buffer,  0)  END_ARG
    CONSTANT_BUFFER(                g_Constants,       GlobalConstants) MTL_BINDING(buffer,  1)  END_ARG
    BUFFER(                         g_ObjectAttribs,   ObjectAttribs)   MTL_BINDING(buffer,  2)  END_ARG     
    BUFFER(                         g_MaterialAttribs, MaterialAttribs) MTL_BINDING(buffer,  3)  END_ARG
    BUFFER(                         g_VertexBuffer,    Vertex)          MTL_BINDING(buffer,  4)  END_ARG
    BUFFER(                         g_IndexBuffer,     uint)            MTL_BINDING(buffer,  5)  END_ARG
    TEXTURE_ARRAY(                  g_Textures,        NUM_TEXTURES)    MTL_BINDING(texture, 0)  END_ARG
    SAMPLER_ARRAY(                  g_Samplers,        NUM_SAMPLERS)    MTL_BINDING(sampler, 0)  END_ARG

    // m_pRayTracingScreenResourcesSign
    WTEXTURE(                       g_RayTracedTex)                     MTL_BINDING(texture, 5)  END_ARG
    TEXTURE(                        g_GBuffer_Normal)                   MTL_BINDING(texture, 6)  END_ARG
    TEXTURE(                        g_GBuffer_Depth)                    MTL_BINDING(texture, 7)  END_ARG
   
END_SHADER_DECLARATION(CSMain, 8, 8)
{
    uint2 Dim;
    TextureDimensions(g_RayTracedTex, Dim);

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
