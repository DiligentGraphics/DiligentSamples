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
    float3 f3SamplePos   = ScreenToWorld(float2(ThreadId.xy) + float2(0.5, 0.5), fDepth, f2ScreenSize, g_Constants.ViewProjInvMat);

    float3 f3Albedo   = g_Albedo.Load(int3(ThreadId.xy, 0)).xyz;
    float3 f3Emissive = g_Emissive.Load(int3(ThreadId.xy, 0)).xyz;
    float3 f3Normal   = normalize(g_Normal.Load(int3(ThreadId.xy, 0)).xyz * 2.0 - 1.0);

    float3 f3Radiance = float3(0.0, 0.0, 0.0);
    if (g_Constants.fLastSampleCount > 0)
        f3Radiance += g_Radiance[ThreadId.xy].rgb;

    uint2 Seed = ThreadId.xy + uint2(g_Constants.uFrameSeed1, g_Constants.uFrameSeed2);

    float3 LightIntensity = float3(1.0, 1.0, 1.0) * 25.0;

    for (int i = 0; i < g_Constants.iNumSamplesPerFrame; ++i)
    {
        f3Radiance += f3Emissive;

        float3 f3LightPos = SampleLight(hash22(Seed));
        float3 f3DirToLight = f3LightPos - f3SamplePos;
        float fDistToLightSqr = dot(f3DirToLight, f3DirToLight);
        f3DirToLight /= sqrt(fDistToLightSqr);

        RayInfo ShadowRay;
        ShadowRay.Origin = f3SamplePos;
        ShadowRay.Dir    = f3DirToLight;

        f3Radiance += TestShadow(ShadowRay) * f3Albedo * max(dot(f3DirToLight, f3Normal), 0.0) * LightIntensity / fDistToLightSqr;

        Seed += uint2(17, 123);
    }

    g_Radiance[ThreadId.xy] = float4(f3Radiance, 0.0);
}
