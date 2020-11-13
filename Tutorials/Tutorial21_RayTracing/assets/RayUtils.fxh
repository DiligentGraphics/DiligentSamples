
RaytracingAccelerationStructure g_TLAS;
ConstantBuffer<Constants>       g_ConstantsCB;


PrimaryRayPayload CastPrimaryRay(RayDesc ray, uint Recursion)
{
    PrimaryRayPayload payload = {float3(0, 0, 0), 0.0, Recursion};

    // Check recursion depth, because driver doesn't count number of recursions.
    if (Recursion > g_ConstantsCB.MaxRecursion)
    {
        // pink color for debugging
        payload.Color = float3(0.95, 0.18, 0.95);
        return payload;
    }
    TraceRay(g_TLAS,            // Acceleration Structure
             RAY_FLAG_NONE,
             ~0,                // Instance Inclusion Mask
             PRIMARY_RAY_INDEX, // Ray Contribution To Hit Group Index
             HIT_GROUP_STRIDE,  // Multiplier For Geometry Contribution To Hit Group Index
             PRIMARY_RAY_INDEX, // Miss Shader Index
             ray,
             payload);
    return payload;
}

ShadowRayPayload CastShadow(RayDesc ray, uint Recursion)
{
    // By default initialize Shading with 0.
    // With RAY_FLAG_SKIP_CLOSEST_HIT_SHADER only intersection and any-hit shaders are executed.
    // Any-hit shader is not used in this tutorial, intersection shader can not write to payload, so on intersection payload.Shading is always 0,
    // on miss shader payload.Shading will be initialized with 1.
    // With this flags shadow casting executed as fast as possible.
    ShadowRayPayload payload = {0.0, Recursion};
    
    // Check recursion depth, because driver doesn't count number of recursions.
    if (Recursion > g_ConstantsCB.MaxRecursion)
    {
        payload.Shading = 1.0;
        return payload;
    }
    TraceRay(g_TLAS,            // Acceleration Structure
             RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH,
             OPAQUE_GEOM_MASK,  // Instance Inclusion Mask
             SHADOW_RAY_INDEX,  // Ray Contribution To Hit Group Index
             HIT_GROUP_STRIDE,  // Multiplier For Geometry Contribution To Hit Group Index
             SHADOW_RAY_INDEX,  // Miss Shader Index
             ray,
             payload);
    return payload;
}

// Calculate perpendicular to specified direction.
void GetRayPerpendicular(float3 dir, out float3 outLeft, out float3 outUp)
{
    const float3 a    = abs(dir);
    const float2 c    = float2(1.0, 0.0);
    const float3 axis = a.x < a.y ? (a.x < a.z ? c.xyy : c.yyx) :
                                    (a.y < a.z ? c.xyx : c.yyx);
    outLeft = normalize(cross(dir, axis));
    outUp   = normalize(cross(dir, outLeft));
}

// Returns random ray within a cone.
float3 DirectionWithinCone(float3 dir, float2 offset)
{
    float3 left, up;
    GetRayPerpendicular(dir, left, up);
    return normalize(dir + left * offset.x + up * offset.y);
}

// Calculate lighting.
void LightingPass(inout float3 Color, float3 Pos, float3 Norm, uint Recursion)
{
    RayDesc ray;
    float3  col = float3(0.0, 0.0, 0.0);

    for (int i = 0; i < NUM_LIGHTS; ++i)
    {
        ray.Origin = Pos + Norm * 0.01;
        ray.TMin   = 0.0;
        ray.TMax   = distance(g_ConstantsCB.LightPos[i].xyz, Pos) * 1.01;

        float3 rayDir = normalize(g_ConstantsCB.LightPos[i].xyz - Pos);
        float  NdotL   = max(0.0, dot(Norm, rayDir));
        float  shading = 0.0;

        if (NdotL > 0.0)
        {
            // Cast multiple rays that distributed within a cone.
            const int PCF = Recursion > 1 ? min(1, g_ConstantsCB.ShadowPCF) : g_ConstantsCB.ShadowPCF;
            for (int j = 0; j < PCF; ++j)
            {
                float2 offset = float2(g_ConstantsCB.DiscPoints[j / 2][(j % 2) * 2], g_ConstantsCB.DiscPoints[j / 2][(j % 2) * 2 + 1]);
                ray.Direction = DirectionWithinCone(rayDir, offset * 0.002);
                shading       += saturate(CastShadow(ray, Recursion).Shading);
            }
            shading = PCF > 0 ? shading / float(PCF) : 1.0;
        }
        col += Color * g_ConstantsCB.LightColor[i].rgb * NdotL * shading;
    }
    Color = col * (1.f / float(NUM_LIGHTS)) + g_ConstantsCB.AmbientColor.rgb;
}
