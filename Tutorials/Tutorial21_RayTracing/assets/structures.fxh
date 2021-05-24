
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
    float  Shading;   // 0 - fully shadowed, 1 - fully in light, 0..1 - for semi-transparent objects
    uint   Recursion; // Current recusrsion depth
};

#define NUM_LIGHTS          2
#define MAX_DISPERS_SAMPLES 16

struct Constants
{
    // Camera world position
    float4   CameraPos;
    float4x4 InvViewProj;

    // Near and far clip plane distances
    float2   ClipPlanes;
    float2   Padding0;

    // The number of shadow PCF samples
    int      ShadowPCF; 
    // Maximum ray recursion depth
    int      MaxRecursion;
    float2   Padding2;

    // Reflection sphere properties
    float3  SphereReflectionColorMask;
    int     SphereReflectionBlur;

    // Refraction cube properties
    float3  GlassReflectionColorMask;
    float   GlassAbsorption;
    float4  GlassMaterialColor;
    float2  GlassIndexOfRefraction;  // min and max IOR
    int     GlassEnableDispersion;
    uint    DispersionSampleCount; // 1..16
    float4  DispersionSamples[MAX_DISPERS_SAMPLES]; // [rgb color] [IOR scale]

    float4  DiscPoints[8]; // packed float2[16]

    // Light properties
    float4  AmbientColor;
    float4  LightPos[NUM_LIGHTS];
    float4  LightColor[NUM_LIGHTS];
};

struct BoxAttribs
{
    float minX, minY, minZ;
    float maxX, maxY, maxZ;
    float padding0, padding1;

#ifdef __cplusplus
    BoxAttribs() {}

    BoxAttribs(float _minX, float _minY, float _minZ,
               float _maxX, float _maxY, float _maxZ) :
        minX{_minX}, minY{_minY}, minZ{_minZ},
        maxX{_maxX}, maxY{_maxY}, maxZ{_maxZ},
        padding0{0.0f}, padding1{0.0f}
    {}
#endif
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
#    define SMALL_OFFSET 0.0001

// For procedural intersections you must add custom hit kind.
#    define RAY_KIND_PROCEDURAL_FRONT_FACE 1
#    define RAY_KIND_PROCEDURAL_BACK_FACE  2

#endif
