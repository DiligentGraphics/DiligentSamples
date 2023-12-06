#include "BasicStructures.fxh"
#include "ToneMapping.fxh"

cbuffer cbToneMappingAttribs
{
    ToneMappingAttribs g_ToneMappingAttribs;
}

cbuffer cbCameraAttribs
{
    CameraAttribs g_CameraAttibs;
}

Texture2D<float3> g_TextureRadiance;
Texture2D<float4> g_TextureRadianceSSR;
Texture2D<float3> g_TextureNormalBuffer;
Texture2D<float4> g_TextureMaterialBuffer;
Texture2D<float3> g_BRDF_LUT;

SamplerState g_BRDF_LUT_sampler;

float2 ScreenSpaceToNDC(float2 Location, float2 InvDimension)
{
    float2 NDC = 2.0f * Location.xy * InvDimension - 1.0f;
    return float2(NDC.x, -NDC.y);
}

float3 GetDirectionWS(float2 Location)
{
    float2 NDC = ScreenSpaceToNDC(Location, g_CameraAttibs.f4ViewportSize.zw);
    float4 Position0 = mul(float4(NDC, 0.0, 0.0), g_CameraAttibs.mViewProjInv);
    float4 Position1 = mul(float4(NDC, 1.0, 1.0), g_CameraAttibs.mViewProjInv);
    return normalize(Position1.xyz - Position0.xyz);
}

float4 ApplyPostEffectsPS(in float4 Position : SV_Position) : SV_Target
{
    float4 RadianceSSR = g_TextureRadianceSSR.Load(int3(Position.xy, 0));
    RadianceSSR.xyz = lerp(float3(0.0, 0.0, 0.0), float3(RadianceSSR.xyz), RadianceSSR.w);

    float PerceptualRoughness = g_TextureMaterialBuffer.Load(int3(Position.xy, 0)).x;

    float3 ViewWS = -GetDirectionWS(Position.xy);
    float3 NormalWS = normalize(g_TextureNormalBuffer.Load(int3(Position.xy, 0)));

    float NdotV = clamp(dot(ViewWS, NormalWS), 0.0, 1.0);
    float2 BRDF = g_BRDF_LUT.Sample(g_BRDF_LUT_sampler, saturate(float2(NdotV, PerceptualRoughness))).rg;
    float3 Reflectance0 = lerp(float3(0.04, 0.04, 0.04), float3(0.318309, 0.318309, 0.318309), 1);
    float3 Reflectance90 = min(max(Reflectance0.x, max(Reflectance0.y, Reflectance0.z)) * 50, 1.0) * float3(1.0, 1.0, 1.0);

    float3 AccumulatedRadiance = g_TextureRadiance.Load(int3(Position.xy, 0)) + RadianceSSR.xyz * (Reflectance0 * BRDF.x * Reflectance90 * BRDF.y);
    return float4(ToneMap(RadianceSSR.xyz + 0.001 * AccumulatedRadiance, g_ToneMappingAttribs, asfloat(g_ToneMappingAttribs.Padding0)), 1.0);
}
