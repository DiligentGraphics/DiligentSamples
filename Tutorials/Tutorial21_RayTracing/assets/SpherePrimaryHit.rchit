
#include "structures.fxh"
#include "RayUtils.fxh"

[shader("closesthit")]
void main(inout PrimaryRayPayload payload, in ProceduralGeomIntersectionAttribs attr)
{
    // Transform sphere normal.
    float3 normal = normalize(mul((float3x3) ObjectToWorld3x4(), attr.Normal));

    // Reflect normal.
    float3 rayDir = reflect(WorldRayDirection(), normal);

    RayDesc ray;
    ray.Origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent() + normal * SMALL_OFFSET;
    ray.TMin   = 0.0;
    ray.TMax   = 100.0;

    // Cast multiple rays that are distributed within a cone.
    float3    color    = float3(0.0, 0.0, 0.0);
    const int ReflBlur = payload.Recursion > 1 ? 1 : g_ConstantsCB.SphereReflectionBlur;
    for (int j = 0; j < ReflBlur; ++j)
    {
        float2 offset = float2(g_ConstantsCB.DiscPoints[j / 2][(j % 2) * 2], g_ConstantsCB.DiscPoints[j / 2][(j % 2) * 2 + 1]);
        ray.Direction = DirectionWithinCone(rayDir, offset * 0.01);
        color += CastPrimaryRay(ray, payload.Recursion + 1).Color;
    }

    color /= float(ReflBlur);

    // Apply color mask for reflected color.
    color *= g_ConstantsCB.SphereReflectionColorMask;

    payload.Color = color;
    payload.Depth = RayTCurrent();
}
