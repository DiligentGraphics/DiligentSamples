#define GROUP_SIZE 32

struct DrawTask
{
    float2 BasePos;  // read-only
    float  Scale;    // read-only
    float  Time;     // read-write
};

RWStructuredBuffer<DrawTask> DrawTasks;

cbuffer CubeData
{
    float4 g_SphereRadius;
    float4 g_Positions[24];
    float4 g_UVs[24];
    uint4  g_Indices[36 / 3];
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

RWByteAddressBuffer  Statistics;

struct Payload
{
    float PosX[GROUP_SIZE];
    float PosY[GROUP_SIZE];
    float PosZ[GROUP_SIZE];
    float Scale[GROUP_SIZE];
    float LODs[GROUP_SIZE];
};

groupshared Payload s_Payload;
groupshared uint    s_TaskCount;


bool IsVisible(float3 cubeCenter, float radius)
{
    float4 center = float4(cubeCenter, 1.0);

    for (int i = 0; i < 6; ++i)
    {
        if (dot(g_Frustum[i], center) < -radius)
            return false;
    }
    return true;
}

float CalcDetailLevel(float3 cubeCenter, float radius)
{
    // https://stackoverflow.com/questions/21648630/radius-of-projected-sphere-in-screen-space
    float3 pos   = mul(float4(cubeCenter, 1.0), g_ViewMat).xyz;
    float  dist2 = dot(pos, pos);
    float  size  = g_CoTanHalfFov * radius / sqrt(dist2 - radius * radius); // sphere radius in screen space
    float  level = clamp(1.0 - size, 0.0, 1.0);
    return level;
}


[numthreads(GROUP_SIZE,1,1)]
void main(in uint I : SV_GroupIndex,
          in uint wg : SV_GroupID)
{
    // initialize shared variable
    if (I == 0)
    {
        s_TaskCount = 0;
    }

    // flush cache and synchronize
    GroupMemoryBarrierWithGroupSync();

    const uint gid   = wg * GROUP_SIZE + I;
    DrawTask   task  = DrawTasks[gid];
    float3     pos   = float3(task.BasePos, 0.0).xzy;
    float      scale = task.Scale;
    float      time  = task.Time;

    // simulation
    pos.y = sin(time);

    if (g_Animate)
    {
        DrawTasks[gid].Time = time + g_ElapsedTime;
    }

    // frustum culling
    if (!g_FrustumCulling || IsVisible(pos, g_SphereRadius.x * scale))
    {
        uint index = 0;
        InterlockedAdd(s_TaskCount, 1, index);
        
        s_Payload.PosX[index]  = pos.x;
        s_Payload.PosY[index]  = pos.y;
        s_Payload.PosZ[index]  = pos.z;
        s_Payload.Scale[index] = scale;
        s_Payload.LODs[index]  = CalcDetailLevel(pos, g_SphereRadius.x * scale);
    }
    
    // all thread must complete thir work that we can read s_TaskCount
    GroupMemoryBarrierWithGroupSync();
    
    DispatchMesh(s_TaskCount, 1, 1, s_Payload);

    if (I == 0)
    {
        // update statistics
        uint origin;
        Statistics.InterlockedAdd(0, s_TaskCount, origin);
    }
}
