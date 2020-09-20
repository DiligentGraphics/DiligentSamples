
#ifndef GROUP_SIZE
#define GROUP_SIZE 32
#endif

#ifdef VULKAN
#define float2   vec2
#define float4   vec4
#define uint4    uvec4
#define float4x4 layout(row_major) mat4x4 // row major matrices for compatibility with DirectX
#define cbuffer  layout(std140) uniform
#endif

struct DrawTask
{
    float2 BasePos; // read-only
    float  Scale;   // read-only
    float  Time;    // read-write
};

cbuffer CubeData
{
    float4 g_SphereRadius;
    float4 g_Positions[24];
    float4 g_UVs[24];
    uint4  g_Indices[36 / 3]; // 3 indices per element
};

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

#ifndef VULKAN

// Payload size must be less than 16kb.
struct Payload
{
    float PosX[GROUP_SIZE];
    float PosY[GROUP_SIZE];
    float PosZ[GROUP_SIZE];
    float Scale[GROUP_SIZE];
    float LODs[GROUP_SIZE];
};

#endif
