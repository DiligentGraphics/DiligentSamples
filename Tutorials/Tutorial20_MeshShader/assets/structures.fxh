#ifndef GROUP_SIZE
#define GROUP_SIZE 32
#endif

#ifndef MAX_VOXELS_PER_GROUP
#define MAX_VOXELS_PER_GROUP 2
#endif

// 32 bytes
struct DrawTask
{
    float4 BasePosAndScale;     // [x, y, z, xyzScale]
    float4 RandomValue;         // [rand, alignedDrawTaskSize, drawTaskCountPadding, 0]
};

// 168 bytes
struct Constants
{
    float4x4 ViewMat;           // 4 * 16 = 64
    float4x4 ViewProjMat;       // 4 * 16 = 64
    float4 Frustum[6];          // 4 * 6 = 24

    float CoTanHalfFov;         // 4
    float MSDebugViz;           // 4
    float OctreeDebugViz;       // 4
    uint FrustumCulling;        // 4
    uint OcclusionCulling;      // 4
    
    uint3 Padding;
};

// 32 bytes
struct GPUOctreeNode
{
    float4 minAndIsFull;        // [x, y, z, isFull]
    float4 max;
    int childrenStartIndex;
    int numChildren;
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
