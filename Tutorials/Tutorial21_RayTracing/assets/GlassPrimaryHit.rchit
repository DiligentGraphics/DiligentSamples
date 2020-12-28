
#include "structures.fxh"
#include "RayUtils.fxh"

ConstantBuffer<CubeAttribs>  g_CubeAttribsCB;


// Simulate light absorption inside glass.
float3 LightAbsorption(float3 color1, float depth)
{
    float  factor1 = depth * 0.25;
    float  factor2 = pow(depth * g_ConstantsCB.GlassAbsorption, 2.2) * 0.25;
    float  factor  = clamp(factor1 + factor2 + 0.05, 0.0, 1.0); 
    float3 color2  = color1 * g_ConstantsCB.GlassMaterialColor.rgb;
    return lerp(color1, color2, factor);
}

float3 BlendWithReflection(float3 srcColor, float3 reflectionColor, float factor)
{
    return lerp(srcColor, reflectionColor * g_ConstantsCB.GlassReflectionColorMask.rgb, factor);
}

// Optimized fresnel calculation.
float Fresnel(float eta, float cosThetaI)
{
    cosThetaI = clamp(cosThetaI, -1.0, 1.0);
    if (cosThetaI < 0.0)
    {
        eta = 1.0 / eta;
        cosThetaI = -cosThetaI;
    }

    float sinThetaTSq = eta * eta * (1.0 - cosThetaI * cosThetaI);
    if (sinThetaTSq > 1.0)
        return 1.0;

    float cosThetaT = sqrt(1.0 - sinThetaTSq);

    float Rs = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);
    float Rp = (eta * cosThetaT - cosThetaI) / (eta * cosThetaT + cosThetaI);

    return 0.5 * (Rs * Rs + Rp * Rp);
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
    const float  AirIOR      = 1.0;
    float3       resultColor = float3(0.0, 0.0, 0.0);

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
            float  relIOR = 1.0;

            // Calculate index of refraction for specified wavelength.
            float  glassIOR = lerp(g_ConstantsCB.GlassIndexOfRefraction.x, g_ConstantsCB.GlassIndexOfRefraction.y, g_ConstantsCB.DispersionSamples[i].a);
            float3 colorMask = g_ConstantsCB.DispersionSamples[i].rgb; // RGB color for wavelength
            float3 rayDir    = WorldRayDirection();

            // Refraction at the interface between air and glass.
            if (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE)
            {
                relIOR = AirIOR / glassIOR;
                rayDir = refract(rayDir, norm, relIOR);
            }
            // Refraction at the interface between glass and air.
            else if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
            {
                relIOR = glassIOR / AirIOR;
                norm   = -norm;
                rayDir = refract(rayDir, norm, relIOR);
            }
    
            float  fresnel  = Fresnel(relIOR, dot(WorldRayDirection(), -norm));
            float3 curColor = float3(0.0, 0.0, 0.0);
            float3 reflColor;

            // Reflection
            {
                ray.Origin    = WorldRayOrigin() + WorldRayDirection() * RayTCurrent() + norm * SMALL_OFFSET;
                ray.Direction = reflect(WorldRayDirection(), norm);

                PrimaryRayPayload reflPayload = CastPrimaryRay(ray, payload.Recursion + 1);
                reflColor = reflPayload.Color;

                if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
                {
                    reflColor = LightAbsorption(reflColor, reflPayload.Depth);
                }
            }

            // Refraction
            if (fresnel < 1.0)
            {
                ray.Origin    = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
                ray.Direction = rayDir;

                PrimaryRayPayload nextPayload = CastPrimaryRay(ray, payload.Recursion + 1);
                curColor = nextPayload.Color;
                
                if (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE || payload.Recursion == 0)
                {
                    curColor = LightAbsorption(curColor, nextPayload.Depth);
                }
            }
            
            curColor    = BlendWithReflection(curColor, reflColor, fresnel);
            AccumColor += curColor * colorMask;
            AccumMask  += colorMask;
        }
    
        // Normalize accumulated color.
        resultColor = AccumColor / AccumMask;
    }
    else
    {
        RayDesc ray;
        ray.Direction = WorldRayDirection();
        ray.TMin      = SMALL_OFFSET;
        ray.TMax      = 100.0;
      
        float3 rayDir = WorldRayDirection();
        float  relIOR = 1.0;

        // Refraction at the interface between air and glass.
        if (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE)
        {
            relIOR = AirIOR / g_ConstantsCB.GlassIndexOfRefraction.x;
            rayDir = refract(rayDir, normal, relIOR);
        }
        // Refraction at the interface between glass and air.
        else if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
        {
            relIOR = g_ConstantsCB.GlassIndexOfRefraction.x / AirIOR;
            normal = -normal;
            rayDir = refract(rayDir, normal, relIOR);
        }
        
        float  fresnel = Fresnel(relIOR, dot(WorldRayDirection(), -normal));
        float3 reflColor;
        
        // Reflection
        {
            ray.Origin    = WorldRayOrigin() + WorldRayDirection() * RayTCurrent() + normal * SMALL_OFFSET;
            ray.Direction = reflect(WorldRayDirection(), normal);

            PrimaryRayPayload reflPayload = CastPrimaryRay(ray, payload.Recursion + 1);
            reflColor = reflPayload.Color;
            
            if (HitKind() == HIT_KIND_TRIANGLE_BACK_FACE)
            {
                reflColor = LightAbsorption(reflColor, reflPayload.Depth);
            }
        }
        
        // Refraction
        if (fresnel < 1.0)
        {
            ray.Origin    = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
            ray.Direction = rayDir;

            PrimaryRayPayload nextPayload = CastPrimaryRay(ray, payload.Recursion + 1);
            resultColor = nextPayload.Color;
            
            if (HitKind() == HIT_KIND_TRIANGLE_FRONT_FACE || payload.Recursion == 0)
            {
                resultColor = LightAbsorption(resultColor, nextPayload.Depth);
            }
        }
        
        resultColor = BlendWithReflection(resultColor, reflColor, fresnel);
    }

    payload.Color = resultColor;
    payload.Depth = RayTCurrent();
}
