#version 430

layout (points) in;
layout (triangle_strip, max_vertices = 36) out;

layout(location=0) in  int  in_VertexID[];

layout(location=0) out vec2 out_UV;
layout(location=1) out vec4 out_Color;

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

vec4 Rainbow(float factor)
{
    float h   = factor / 1.45;
    vec3  col = vec3(abs(h * 6.0 - 3.0) - 1.0, 2.0 - abs(h * 6.0 - 2.0), 2.0 - abs(h * 6.0 - 4.0));
    return vec4(clamp(col, vec3(0.0), vec3(1.0)), 1.0);
}

float CalcDetailLevel(vec3 cubeCenter, float radius)
{
    // https://stackoverflow.com/questions/21648630/radius-of-projected-sphere-in-screen-space
    float dist  = (g_ViewMat * vec4(cubeCenter, 0.0)).z;
    float size  = g_CoTanHalfFov * radius / sqrt(dist * dist - radius * radius); // sphere size in screen space
    float level = clamp(1.0 - size, 0.0, 1.0);
    return level;
}


void main()
{
    int      gid   = in_VertexID[0];
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
    if (g_FrustumCulling && !IsVisible(pos, g_Cube.sphereRadius.x * scale))
        return;

    float LOD   = CalcDetailLevel(pos, g_Cube.sphereRadius.x * scale);
    vec4  color = Rainbow(LOD);

    // indices stored for triangle lists, so we need to emit 12 triangles
    for (int i = 0; i < 36;)
    {
        uint idx    = g_Cube.indices[i >> 2][i & 3];
        gl_Position = g_ViewProjMat * vec4(pos + g_Cube.pos[idx].xyz * scale, 1.0);
        out_UV      = g_Cube.uv[idx].xy;
        out_Color   = color;
        EmitVertex();
        ++i;

        idx         = g_Cube.indices[i >> 2][i & 3];
        gl_Position = g_ViewProjMat * vec4(pos + g_Cube.pos[idx].xyz * scale, 1.0);
        out_UV      = g_Cube.uv[idx].xy;
        out_Color   = color;
        EmitVertex();
        ++i;

        idx         = g_Cube.indices[i >> 2][i & 3];
        gl_Position = g_ViewProjMat * vec4(pos + g_Cube.pos[idx].xyz * scale, 1.0);
        out_UV      = g_Cube.uv[idx].xy;
        out_Color   = color;
        EmitVertex();
        ++i;

        EndPrimitive();
    }

    // update statistics
    atomicAdd(g_VisibleCubes, 1);
}
