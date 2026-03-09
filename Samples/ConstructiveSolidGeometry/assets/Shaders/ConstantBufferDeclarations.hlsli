#ifndef _CONSTANT_BUFFER_DECLARATIONS_HLSLI_
#define _CONSTANT_BUFFER_DECLARATIONS_HLSLI_

#ifdef __cplusplus

#   ifndef CHECK_STRUCT_ALIGNMENT
        // Note that defining empty macros causes GL shader compilation error on Mac, because
        // it does not allow standalone semicolons outside of main.
        // On the other hand, adding semicolon at the end of the macro definition causes gcc error.
#       define CHECK_STRUCT_ALIGNMENT(s) static_assert( sizeof(s) % 16 == 0, "sizeof(" #s ") is not multiple of 16" )
#   endif

#endif

// Geometry types
#define GEOMETRY_TYPE_SPHERE 0u
#define GEOMETRY_TYPE_AABB   1u
#define GEOMETRY_TYPE_MESH   2u

// CSG operation types
#define CSG_OP_UNION                 0u
#define CSG_OP_SUBTRACTION           1u
#define CSG_OP_INTERSECTION          2u
#define CSG_OP_SYMMETRIC_DIFFERENCE  3u

// Object & Material structures (shared between CPU and GPU)
struct ObjectAttribs
{
    float4x4 CurrInvWorldMatrix;
    float4x4 CurrNormalMatrix;
    float4x4 CurrWorldViewProjectMatrix;
    float4x4 CurrWorldMatrix;

    uint     ObjectID;
    uint     ObjectType;
    uint     MaterialIdx;
    uint     Padding0;
    
    uint     Padding1[60]; 
};

#ifdef CHECK_STRUCT_ALIGNMENT
    CHECK_STRUCT_ALIGNMENT(ObjectAttribs);
#endif

struct MaterialAttribs
{
    float4 BaseColor;
    float  Roughness;
    float  Metalness;
    float  Padding0;
    float  Padding1;
};

#ifdef CHECK_STRUCT_ALIGNMENT
    CHECK_STRUCT_ALIGNMENT(MaterialAttribs);
#endif

// CSG operation structure (shared between CPU and GPU)
struct CSGOperationAttribs
{
    uint PrimaryObjectID;
    uint SecondaryObjectID;
    uint Operation;
    uint PrimaryMaterialIdx;
};

#ifdef CHECK_STRUCT_ALIGNMENT
    CHECK_STRUCT_ALIGNMENT(CSGOperationAttribs);
#endif


// A-Buffer face flags
#define ABUFFER_FACE_ENTRY 0u
#define ABUFFER_FACE_EXIT  1u

// A-Buffer fragment structure
struct ABufferFragment
{
    uint  PackedData; // NormalX(8) | NormalY(8) | MaterialIdx(5) | ObjectID(8) | IsExit(1) | unused(2)
    float Depth;
    uint  Next;
};

// A-Buffer structures (shared between CPU and GPU)
struct ABufferConstants
{
    uint2  ScreenSize;
    uint   MaxNodeCount;
    uint   OperationCount;

    float4 LightDirection;
    float4 LightColor;
    float4 AmbientColor;
};

#ifdef CHECK_STRUCT_ALIGNMENT
    CHECK_STRUCT_ALIGNMENT(ABufferConstants);
#endif

#endif // _CONSTANT_BUFFER_DECLARATIONS_HLSLI_
