
#include "structures.fxh"
#include "RayUtils.fxh"

ConstantBuffer<CubeAttribs>  g_CubeAttribsCB;


// Simulate light absorption inside glass.
float3 LightAbsoption(float3 color, float depth)
{
    float factor = saturate(pow(depth * g_ConstantsCB.GlassOpticalDepth, 1.8) + 0.5);
    return lerp(color, color * g_ConstantsCB.GlassMaterialColor.rgb, factor);
}

float3 BlendWithReflection(float3 srcColor, float3 reflectionColor)
{
    return lerp(srcColor, reflectionColor * g_ConstantsCB.GlassReflectionColorMask.rgb, 0.3);
}

[shader("closesthit")]
void main(inout PrimaryRayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // Calculate triangle barycentrics.
    float3 barycentrics = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    
    // Get vertex indices for primitive.
    uint3 primitive = g_CubeAttribsCB.Primitives[PrimitiveIndex()].xyz;
    
    // Calculate and transform triangle normal.
    float3 normal = g_CubeAttribsCB.Normals[primitive.x].xyz * barycentrics.x +
                    g_CubeAttribsCB.Normals[primitive.y].xyz * barycentrics.y +
                    g_CubeAttribsCB.Normals[primitive.z].xyz * barycentrics.z;
    normal        = normalize(mul((float3x3) ObjectToWorld3x4(), normal));
    
    // Air index of refraction
    const float  AirIOR = 1.0;

    // Enable dispersion - simulate rays with different wavelengths.
    // For optimization, disable dispersion after several reflections/refractions.
    if (g_ConstantsCB.GlassEnableDispersion && payload.Recursion == 0)
    {
        float3  AccumColor = float3(0.0, 0.0, 0.0);
        float3  AccumMask  = float3(0.0, 0.0, 0.0);

        RayDesc ray;
        ray.TMin = SMALL_OFFSET;
        ray.TMax = 100.0;
    
        // Cast multiple rays with different wavelengths.
        const int step = MAX_DISPERS_SAMPLES / g_ConstantsCB.DispersionSampleCount;
        for (int i = 0; i < MAX_DISPERS_SAMPLES; i += step)
        {
            float3 norm = normal;
            float3 color;

            // Calculate index of refraction for specified wavelength.
            float  GlassIOR = lerp(g_ConstantsCB.GlassIndexOfRefraction.x, g_ConstantsCB.GlassIndexOfRefraction.y, g_ConstantsCB.DispersionSamples[i].a);

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
                ray.Origin    = WorldRayOrigin() + WorldRayDirection() * RayTCurrent() + norm * SMALL_OFFSET;
                ray.Direction = reflect(WorldRayDirection(), norm);

                PrimaryRayPayload reflPayload = CastPrimaryRay(ray, payload.Recursion + 1);
                color = reflPayload.Color * g_ConstantsCB.DispersionSamples[i].rgb;

                if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
                {
                    color = LightAbsoption(color, reflPayload.Depth);
                }
            }
            else
            {
                ray.Origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

                PrimaryRayPayload nextPayload = CastPrimaryRay(ray, payload.Recursion + 1);
                color = nextPayload.Color * g_ConstantsCB.DispersionSamples[i].rgb;
                
                if (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE)
                {
                    color = LightAbsoption(color, nextPayload.Depth);
                }
            }

            AccumColor += color;
            AccumMask  += g_ConstantsCB.DispersionSamples[i].rgb;
        }
    
        // Normalize accumulated color.
        payload.Color = AccumColor / AccumMask;
        payload.Depth = RayTCurrent();
        
        // Add a reflection part for the front face.
        if (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE)
        {
            if (payload.Recursion < 1)
            {
                ray.Origin    = WorldRayOrigin() + WorldRayDirection() * RayTCurrent() + normal * SMALL_OFFSET;
                ray.Direction = reflect(WorldRayDirection(), normal);

                PrimaryRayPayload reflPayload = CastPrimaryRay(ray, payload.Recursion + 1);
                payload.Color = BlendWithReflection(payload.Color, reflPayload.Color);
            }
        }
    }
    else
    {
        RayDesc ray;
        ray.Direction = WorldRayDirection();
        ray.TMin      = SMALL_OFFSET;
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
            ray.Origin    = WorldRayOrigin() + WorldRayDirection() * RayTCurrent() + normal * SMALL_OFFSET;
            ray.Direction = reflect(WorldRayDirection(), normal);

            PrimaryRayPayload reflPayload = CastPrimaryRay(ray, payload.Recursion + 1);
            payload.Color = reflPayload.Color;
            payload.Depth = RayTCurrent();
            
            if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
            {
                payload.Color = LightAbsoption(payload.Color, reflPayload.Depth);
            }
        }
        else
        {
            ray.Origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();

            PrimaryRayPayload nextPayload = CastPrimaryRay(ray, payload.Recursion + 1);

            payload.Color = nextPayload.Color;
            payload.Depth = RayTCurrent();

            if (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE)
            {
                payload.Color = LightAbsoption(payload.Color, nextPayload.Depth);

                // Add a reflection part for the front face.
                if (payload.Recursion < 1)
                {
                    ray.Direction = reflect(WorldRayDirection(), normal);
                    PrimaryRayPayload reflPayload = CastPrimaryRay(ray, payload.Recursion + 1);
                    payload.Color = BlendWithReflection(payload.Color, reflPayload.Color);
                }
            }
        }
    }
}
