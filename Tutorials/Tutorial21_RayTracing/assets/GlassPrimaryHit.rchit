
#include "structures.fxh"
#include "RayUtils.fxh"

ConstantBuffer<CubeAttribs>  g_CubeAttribsCB;

[shader("closesthit")]
void main(inout PrimaryRayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // Calculate triangle barycentrics.
    float3 barycentrics = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    
    // Get vertex indices for primitive.
    uint3 face = g_CubeAttribsCB.Primitives[PrimitiveIndex()].xyz;
    
    // Calculate and transform triangle normal.
    float3 normal = g_CubeAttribsCB.Normals[face.x].xyz * barycentrics.x +
                    g_CubeAttribsCB.Normals[face.y].xyz * barycentrics.y +
                    g_CubeAttribsCB.Normals[face.z].xyz * barycentrics.z;
    normal        = normalize(mul((float3x3) ObjectToWorld3x4(), normal));
    
    // Air index of refraction
    const float  AirIOR = 1.0;

    // Enable interferention - simulate rays with different wavelength.
    // For optimization disable interferention after several reflections/refractions.
    if (g_ConstantsCB.GlassEnableInterferention && payload.Recursion < 2)
    {
        float3  AccumColor = float3(0.0, 0.0, 0.0);
        float3  AccumMask  = float3(0.0, 0.0, 0.0);

        RayDesc ray;
        ray.Origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
        ray.TMin   = 0.001;
        ray.TMax   = 100.0;
    
        // Cast multiple rays with different wavelength.
        const int step = MAX_INTERF_SAMPLES / g_ConstantsCB.InterferentionSampleCount;
        for (int i = 0; i < MAX_INTERF_SAMPLES; i += step)
        {
            float3 norm = normal;
            float3 color;

            // Calculate index of refraction for specified wavelength.
            float  GlassIOR = lerp(g_ConstantsCB.GlassIndexOfRefraction.x, g_ConstantsCB.GlassIndexOfRefraction.y, g_ConstantsCB.InterferentionSamples[i].a);

            // Refraction at the interface between air and glass.
            if (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE)
            {
                ray.Direction = refract(WorldRayDirection(), norm, AirIOR / GlassIOR);
            }
            // Refraction at the interface between glass and air.
            else if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
            {
                norm = -norm;
                ray.Direction = refract(WorldRayDirection(), norm, GlassIOR / AirIOR);
            }
    
            // Total internal reflection.
            if (all(ray.Direction == float3(0,0,0)))
            {
                ray.Origin    = WorldRayOrigin() + WorldRayDirection() * RayTCurrent() + norm * 0.01;
                ray.Direction = reflect(WorldRayDirection(), norm);
                ray.TMin      = 0.0;
                ray.TMax      = 100.0;

                PrimaryRayPayload reflPayload = CastPrimaryRay(ray, payload.Recursion + 1);
                color = reflPayload.Color * g_ConstantsCB.InterferentionSamples[i].rgb;

                // Simulate light absorption inside glass.
                if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
                {
                    color *= exp(-reflPayload.Depth * g_ConstantsCB.GlassOpticalDepth) * g_ConstantsCB.GlassMaterialColor.rgb;
                }
            }
            else
            {
                PrimaryRayPayload nextPayload = CastPrimaryRay(ray, payload.Recursion + 1);
                color = nextPayload.Color * g_ConstantsCB.InterferentionSamples[i].rgb;
                
                // Simulate light absorption inside glass.
                if (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE)
                {
                    color *= exp(-nextPayload.Depth * g_ConstantsCB.GlassOpticalDepth) * g_ConstantsCB.GlassMaterialColor.rgb;
                }
            }

            AccumColor += color;
            AccumMask  += g_ConstantsCB.InterferentionSamples[i].rgb;
        }
    
        // Normalize accumulated color.
        payload.Color = AccumColor / AccumMask;
        payload.Depth = RayTCurrent();
        
        // Add reflection part for front face.
        if (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE)
        {
            if (payload.Recursion < 1)
            {
                ray.Origin    = WorldRayOrigin() + WorldRayDirection() * RayTCurrent() + normal * 0.01;
                ray.Direction = reflect(WorldRayDirection(), normal);
                ray.TMin      = 0.0;
                ray.TMax      = 100.0;
                PrimaryRayPayload reflPayload = CastPrimaryRay(ray, payload.Recursion + 1);
                payload.Color += reflPayload.Color * g_ConstantsCB.GlassReflectionColorMask.rgb;
            }
        }
    }
    else
    {
        RayDesc ray;
        ray.Origin    = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
        ray.Direction = WorldRayDirection();
        ray.TMin      = 0.001;
        ray.TMax      = 100.0;
      
        // Refraction at the interface between air and glass.
        if (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE)
        {
            ray.Direction = refract(ray.Direction, normal, AirIOR / g_ConstantsCB.GlassIndexOfRefraction.x);
        }
        // Refraction at the interface between glass and air.
        else if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
        {
            normal = -normal;
            ray.Direction = refract(ray.Direction, normal, g_ConstantsCB.GlassIndexOfRefraction.x / AirIOR);
        }

        // Total internal reflection.
        if (all(ray.Direction == float3(0,0,0)))
        {
            ray.Origin    = WorldRayOrigin() + WorldRayDirection() * RayTCurrent() + normal * 0.01;
            ray.Direction = reflect(WorldRayDirection(), normal);
            ray.TMin      = 0.0;
            ray.TMax      = 100.0;

            PrimaryRayPayload reflPayload = CastPrimaryRay(ray, payload.Recursion + 1);
            payload.Color = reflPayload.Color;
            payload.Depth = RayTCurrent();
            
            // Simulate light absorption inside glass.
            if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
            {
                payload.Color *= exp(-reflPayload.Depth * g_ConstantsCB.GlassOpticalDepth) * g_ConstantsCB.GlassMaterialColor.rgb;
            }
        }
        else
        {
            PrimaryRayPayload nextPayload = CastPrimaryRay(ray, payload.Recursion + 1);

            payload.Color = nextPayload.Color;
            payload.Depth = RayTCurrent();

            if (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE)
            {
                // Simulate light absorption inside glass.
                payload.Color *= exp(-nextPayload.Depth * g_ConstantsCB.GlassOpticalDepth) * g_ConstantsCB.GlassMaterialColor.rgb;
                
                // Add reflection part for front face.
                if (payload.Recursion < 1)
                {
                    ray.Origin    = WorldRayOrigin() + WorldRayDirection() * RayTCurrent() + normal * 0.01;
                    ray.Direction = reflect(WorldRayDirection(), normal);
                    ray.TMin      = 0.0;
                    ray.TMax      = 100.0;
                    PrimaryRayPayload reflPayload = CastPrimaryRay(ray, payload.Recursion + 1);
                    payload.Color += reflPayload.Color * g_ConstantsCB.GlassReflectionColorMask.rgb;
                }
            }
        }
    }
}
