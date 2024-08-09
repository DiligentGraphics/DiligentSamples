
#ifndef GROUP_SIZE
#define GROUP_SIZE 32
#endif

struct DrawTask
{
    float2 BasePos;
    float Scale;
    float randomValue;
};

struct Constants
{
    float4x4 ViewMat;
    float4x4 ViewProjMat;
    float4 Frustum[6];

    float CoTanHalfFov;
    float MSDebugViz;
    uint FrustumCulling;
    uint Padding;
};

// Payload size must be less than 16kb.
struct Payload
{
    // Currently, DXC fails to compile the code when
    // the struct declares float3 Pos, so we have to
    // use struct of arrays
    float PosX[GROUP_SIZE];
    float PosY[GROUP_SIZE];
    float PosZ[GROUP_SIZE];
    float Scale[GROUP_SIZE];
    float MSRand[GROUP_SIZE];
};
