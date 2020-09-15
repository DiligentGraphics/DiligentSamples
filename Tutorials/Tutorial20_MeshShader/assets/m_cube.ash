#version 460
#extension GL_NV_mesh_shader : require

#define GROUP_SIZE 32

layout(local_size_x = GROUP_SIZE) in;

struct DrawTask
{
    vec2  BasePos;  // read-only
    float Scale;    // read-only
    uint  Time;     // read-write
};

layout(std140) buffer DrawTasks
{
    DrawTask g_DrawTasks[];
};

layout(std140) uniform CubeData
{
    vec4  sphereRadius;
    vec4  pos[24];
    vec4  uv[24];
    uvec4 indices[36 / 4];
} g_Cube;

layout(std140) uniform Constants
{
    mat4x4 g_ViewMat;
    mat4x4 g_ViewProjMat;
    vec4   g_Frustum[6];
    float  g_CoTanHalfFov;
    bool   g_FrustumCulling;
    bool   g_Animate;
};

layout(std140) buffer Statistics
{
    uint g_VisibleCubes; // atomic
};


bool IsVisible(vec3 cubeCenter, float radius)
{
    vec4 center = vec4(cubeCenter, 1.0);

    for (int i = 0; i < 6; ++i)
    {
        if (dot(g_Frustum[i], center) < -radius)
            return false;
    }
    return true;
}

float CalcDetailLevel(vec3 cubeCenter, float radius)
{
    // https://stackoverflow.com/questions/21648630/radius-of-projected-sphere-in-screen-space
    float dist  = (g_ViewMat * vec4(cubeCenter, 0.0)).z;
    float size  = g_CoTanHalfFov * radius / sqrt(dist * dist - radius * radius); // sphere size in screen space
    float level = clamp(1.0 - size, 0.0, 1.0);
    return level;
}

taskNV out Task {
    vec4  pos[GROUP_SIZE];
    float LODs[GROUP_SIZE];
} Output;

shared uint s_TaskCount;


void main()
{
    // initialize shared variable
    if (gl_LocalInvocationIndex == 0)
    {
        s_TaskCount = 0;
    }

    // flush cache and synchronize
    memoryBarrierShared();
    barrier();

    uint     gid   = gl_WorkGroupID.x * gl_WorkGroupSize.x + gl_LocalInvocationIndex;
    DrawTask task  = g_DrawTasks[gid];
    vec3     pos   = vec3(task.BasePos, 0.0).xzy;
    float    scale = task.Scale;
    uint     time  = task.Time;

    // simulation
    pos.y = sin(float(time) * 0.001);

    if (g_Animate)
    {
        g_DrawTasks[gid].Time = time + 1;
    }
    
    // frustum culling
    if (!g_FrustumCulling || IsVisible(pos, g_Cube.sphereRadius.x * scale))
    {
        uint index = atomicAdd(s_TaskCount, 1);

        Output.pos[index]  = vec4(pos, scale);
        Output.LODs[index] = CalcDetailLevel(pos, g_Cube.sphereRadius.x * scale);
    }

    // all thread must complete thir work that we can read s_TaskCount
    barrier();

    // invalidate cache to read actual value from s_TaskCount
    memoryBarrierShared();

    if (gl_LocalInvocationIndex == 0)
    {
        gl_TaskCountNV = s_TaskCount;
        
        // update statistics
        atomicAdd(g_VisibleCubes, s_TaskCount);
    }
}
