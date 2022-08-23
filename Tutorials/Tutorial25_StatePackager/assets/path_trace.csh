#include "structures.fxh"
#include "scene.fxh"

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
    float3 f3WorldPos   = ScreenToWorld(float2(ThreadId.xy) + float2(0.5, 0.5), fDepth, f2ScreenSize, g_Constants.ViewProjInvMat);

    float3 f3Albedo   = g_Albedo.Load(int3(ThreadId.xy, 0)).xyz;
    float3 f3Emissive = g_Emissive.Load(int3(ThreadId.xy, 0)).xyz;
    float3 f3Normal   = normalize(g_Normal.Load(int3(ThreadId.xy, 0)).xyz * 2.0 - 1.0);

    float3 f3Ambient = float3(0.05, 0.05, 0.05);
    float3 I = f3Albedo * f3Ambient + f3Emissive;

    float3 LightDir = normalize(float3(0.4, -0.7, 0.3));
    RayInfo ShadowRay;
    ShadowRay.Origin = f3WorldPos;
    ShadowRay.Dir    = -LightDir;
    I += TestShadow(ShadowRay) * f3Albedo * max(dot(-LightDir, f3Normal), 0.0) * float3(2.0, 2.0, 2.0);

    I *= float(g_Constants.iNumSamplesPerFrame);
    if (g_Constants.fLastSampleCount > 0)
        I += g_Radiance[ThreadId.xy].rgb;

    g_Radiance[ThreadId.xy] = float4(I, 0.0);
}
