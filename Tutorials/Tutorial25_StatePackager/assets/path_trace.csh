#include "structures.fxh"
#include "scene.fxh"
#include "hash.fxh"

Texture2D g_Albedo;
Texture2D g_Normal;
Texture2D g_Emissive;
Texture2D g_Depth;

RWTexture2D<float4 /*format = rgba32f*/> g_Radiance;

cbuffer cbConstants
{
    ShaderConstants g_Constants;
}

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 8
#endif

// https://github.com/TheRealMJP/DXRPathTracer (MIT)
// Returns a random cosine-weighted direction on the hemisphere around z = 1
float3 SampleDirectionCosineHemisphere(float2 UV)
{
    // Square -> disk
    float2 scale = abs(UV);
    scale /= max(scale.x, scale.y);
    UV /= length(scale);

    float u = UV.x;
    float v = UV.y;

    // Project samples on the disk to the hemisphere to get a
    // cosine weighted distribution
    float3 Dir;
    float r = u * u + v * v;
    Dir.x = u;
    Dir.y = v;
    Dir.z = sqrt(max(0.0, 1.0 - r));

    return Dir;
}

// Returns a random cosine-weighted direction on the hemisphere around N
float3 SampleDirectionCosineHemisphere(float3 N, float2 UV)
{
	float3 T = normalize(cross(N, abs(N.y) > 0.5 ? float3(1, 0, 0) : float3(0, 1, 0)));
	float3 B = cross(T, N);
    float3 Dir = SampleDirectionCosineHemisphere(UV);
    return normalize(Dir.x * T + Dir.y * B + Dir.z * N);
}



[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void main(uint3 ThreadId : SV_DispatchThreadID)
{
    if (ThreadId.x >= g_Constants.uScreenWidth || ThreadId.y >= g_Constants.uScreenHeight)
        return;

    float fDepth = g_Depth.Load(int3(ThreadId.xy, 0)).x;
    if (fDepth == 1.0)
    {
        g_Radiance[ThreadId.xy] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    float2 f2ScreenSize = float2(g_Constants.fScreenWidth, g_Constants.fScreenHeight);
    float3 f3SamplePos0 = ScreenToWorld(float2(ThreadId.xy) + float2(0.5, 0.5), fDepth, f2ScreenSize, g_Constants.ViewProjInvMat);
    float3 f3Albedo0    = g_Albedo.Load(int3(ThreadId.xy, 0)).xyz;
    float3 f3Normal0    = normalize(g_Normal.Load(int3(ThreadId.xy, 0)).xyz * 2.0 - 1.0);

    float3 f3Emissive = g_Emissive.Load(int3(ThreadId.xy, 0)).xyz;
    float3 f3Radiance = float3(0.0, 0.0, 0.0);

    uint2 Seed = ThreadId.xy + uint2(g_Constants.uFrameSeed1, g_Constants.uFrameSeed2);
    for (int i = 0; i < g_Constants.iNumSamplesPerFrame; ++i)
    {
        float3 f3SamplePos = f3SamplePos0;
        float3 f3Albedo    = f3Albedo0;
        float3 f3Normal    = f3Normal0;

        float3 f3Attenuation = float3(1.0, 1.0, 1.0);
        for (int j = 0; j < g_Constants.iNumBounces; ++j)
        {
            float2 rnd2 = hash22(Seed);
            float3 f3LightPos = SampleLight(rnd2);
            float3 f3DirToLight = f3LightPos - f3SamplePos;
            float fDistToLightSqr = dot(f3DirToLight, f3DirToLight);
            f3DirToLight /= sqrt(fDistToLightSqr);

            RayInfo ShadowRay;
            ShadowRay.Origin = f3SamplePos;
            ShadowRay.Dir    = f3DirToLight;

            f3Attenuation *= f3Albedo;
            f3Radiance += f3Attenuation * TestShadow(ShadowRay) * max(dot(f3DirToLight, f3Normal), 0.0) * g_Constants.f4LightIntensity.rgb * g_Constants.f4LightIntensity.a / fDistToLightSqr;

            Seed += uint2(17, 123);

            if (j == g_Constants.iNumBounces-1)
                break;

            RayInfo Ray;
            Ray.Origin = f3SamplePos;
            Ray.Dir    = SampleDirectionCosineHemisphere(f3Normal, rnd2);

            HitInfo Hit = IntersectScene(Ray);
            if (Hit.Type != HIT_TYPE_LAMBERTIAN)
                break;

            f3SamplePos = Ray.Origin + Ray.Dir * Hit.Distance;
            f3Albedo    = Hit.Color;
            f3Normal    = Hit.Normal;
        }

        f3Radiance += f3Emissive;
    }

    if (g_Constants.fLastSampleCount > 0)
        f3Radiance += g_Radiance[ThreadId.xy].rgb;
    g_Radiance[ThreadId.xy] = float4(f3Radiance, 0.0);
}
