#version 460
#extension GL_NV_mesh_shader : require

#ifndef GROUP_SIZE
#   define GROUP_SIZE 32
#endif

layout(local_size_x = GROUP_SIZE) in;

struct DrawTask
{
    vec2  BasePos;  // read-only
    float Scale;    // read-only
    float Time;     // read-write
};

layout(std140) buffer DrawTasks
{
    DrawTask g_DrawTasks[];
};

layout(std140) uniform Constants
{
    layout(row_major) mat4x4 g_ViewMat;
    layout(row_major) mat4x4 g_ViewProjMat;

    vec4   g_Frustum[6];
    float  g_CoTanHalfFov;
    float  g_ElapsedTime;
    bool   g_FrustumCulling;
    bool   g_Animate;
};

layout(std140) uniform CubeData
{
    vec4  sphereRadius;
    vec4  pos[24];
    vec4  uv[24];
    uvec4 indices[36 / 3];
} g_Cube;

layout(std140) buffer Statistics
{
    uint g_VisibleCubes; // atomic
};


// The sphere is visible when the distance from each plane is greater than or equal to the radius of the sphere.
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
    // cubeCenter is also the center of the sphere. 
    // radius - radius of circumscribed sphere

    // get position in view space
    vec3  pos   = (g_ViewMat * vec4(cubeCenter, 1.0)).xyz;

    // square of distance from camera to circumscribed sphere
    float dist2 = dot(pos, pos);

    // calculate sphere size in screen space
    float size  = g_CoTanHalfFov * radius / sqrt(dist2 - radius * radius);

    // calculate detail level
    float level = clamp(1.0 - size, 0.0, 1.0);
    return level;
}

// Task shader output data. Must be less than 16Kb.
taskNV out Task {
    vec4  pos[GROUP_SIZE];
    float LODs[GROUP_SIZE];
} Output;

// This value used to calculate the number of cubes that will be rendered after the frustum culling
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
    float    time  = task.Time;

    // Simple animation
    pos.y = sin(time);

    if (g_Animate)
    {
        g_DrawTasks[gid].Time = time + g_ElapsedTime;
    }
    
    // frustum culling
    if (!g_FrustumCulling || IsVisible(pos, g_Cube.sphereRadius.x * scale))
    {
        // Acquire index that will be used to safely access shader output.
        // Each thread has unique index.
        uint index = atomicAdd(s_TaskCount, 1);

        Output.pos[index]  = vec4(pos, scale);
        Output.LODs[index] = CalcDetailLevel(pos, g_Cube.sphereRadius.x * scale);
    }

    // all threads must complete their work so that we can read s_TaskCount
    barrier();

    // invalidate cache to read actual value from s_TaskCount
    memoryBarrierShared();

    if (gl_LocalInvocationIndex == 0)
    {
        // Set the number of mesh shader workgroups.
        gl_TaskCountNV = s_TaskCount;
        
        // update statistics
        atomicAdd(g_VisibleCubes, s_TaskCount);
    }
}
