#ifndef _GEOMETRY_STRUCTURES_FXH_
#define _GEOMETRY_STRUCTURES_FXH_

#ifdef __cplusplus

#   ifndef CHECK_STRUCT_ALIGNMENT
        // Note that defining empty macros causes GL shader compilation error on Mac, because
        // it does not allow standalone semicolons outside of main.
        // On the other hand, adding semicolon at the end of the macro definition causes gcc error.
#       define CHECK_STRUCT_ALIGNMENT(s) static_assert( sizeof(s) % 16 == 0, "sizeof(" #s ") is not multiple of 16" )
#   endif

#endif

#define GEOMETRY_TYPE_SPHERE 0u
#define GEOMETRY_TYPE_AABB   1u

struct ObjectAttribs
{
    float4x4 CurrInvWorldMatrix;
    float4x4 CurrNormalMatrix;
    float4x4 CurrWorldViewProjectMatrix;
    float4x4 PrevWorldTransform;

    uint     Padding0;
    uint     ObjectType;
#ifdef __cplusplus
    uint     ObjectMaterialIdx0;
    uint     ObjectMaterialIdx1;
#else
    uint2    ObjectMaterialIdx;
#endif

#ifdef __cplusplus
    uint     ObjectMaterialDim0;
    uint     ObjectMaterialDim1;
#else
    uint2    ObjectMaterialDim;
#endif

#ifdef __cplusplus
    float     ObjectMaterialFrequency0;
    float     ObjectMaterialFrequency1;
#else
    float2    ObjectMaterialFrequency;
#endif
    float4    Padding[14];
};

#ifdef CHECK_STRUCT_ALIGNMENT
    CHECK_STRUCT_ALIGNMENT(ObjectAttribs);
#endif

struct MaterialAttribs
{
    float4 BaseColor;
    float Roughness;
    float Metalness;
    float Padding0;
    float Padding1;
};

#ifdef CHECK_STRUCT_ALIGNMENT
    CHECK_STRUCT_ALIGNMENT(MaterialAttribs);
#endif

#endif // _GEOMETRY_STRUCTURES_FXH_
