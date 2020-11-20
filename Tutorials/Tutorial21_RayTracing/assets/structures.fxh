
struct CubeAttribs
{
    float4 UVs[24];
    float4 Normals[24];
    uint4  Primitives[12];
};

struct PrimaryRayPayload
{
    float3 Color;
    float  Depth;
    uint   Recursion;
};

struct ShadowRayPayload
{
    float  Shading;
    uint   Recursion;
};

#define NUM_LIGHTS         2
#define MAX_INTERF_SAMPLES 16

struct Constants
{
    // camera
    float4   Position;
    float2   ClipPlanes;
    float2   padding0;
    float4   FrustumRayLT;
    float4   FrustumRayLB;
    float4   FrustumRayRT;
    float4   FrustumRayRB;

    // texturing
    float   Padding1;
    int     ShadowPCF;
    int     MaxRecursion;
    int     padding1;

    // reflection sphere
    float3  SphereReflectionColorMask;
    int     SphereReflectionBlur;

    // refraction cube
    float3  GlassReflectionColorMask;
    float   GlassOpticalDepth;
    float4  GlassMaterialColor;
    float2  GlassIndexOfRefraction;  // min and max IOR
    int     GlassEnableInterference;
    uint    InterferenceSampleCount; // 1..16
    float4  InterferenceSamples[MAX_INTERF_SAMPLES]; // [rgb color] [IOR scale]

    float4  DiscPoints[8]; // packed float2[16]

    // lights
    float4  AmbientColor;
    float4  LightPos[NUM_LIGHTS];
    float4  LightColor[NUM_LIGHTS];
};

struct BoxAttribs
{
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
    float padding0, padding1;
};

struct ProceduralGeomIntersectionAttribs
{
    float3 Normal;
};


// Instance mask.
#define OPAQUE_GEOM_MASK      0x01
#define TRANSPARENT_GEOM_MASK 0x02

// Ray types
#define HIT_GROUP_STRIDE  2
#define PRIMARY_RAY_INDEX 0
#define SHADOW_RAY_INDEX  1


#ifndef __cplusplus

// Small offset between ray intersection and new ray origin to avoid self-intersections.
#    define SMALL_OFFSET 0.001

// For procedural intersections you must add custom hit kind.
#    define RAY_KIND_PROCEDURAL_FRONT_FACE 1
#    define RAY_KIND_PROCEDURAL_BACK_FACE  2

#endif
