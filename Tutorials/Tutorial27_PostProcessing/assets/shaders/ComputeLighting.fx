#include "BasicStructures.fxh"
#include "PBR_Structures.fxh"
#include "FullScreenTriangleVSOutput.fxh"

#define FLT_EPS 5.960464478e-8

cbuffer cbCameraAttribs
{
    CameraAttribs g_Camera;
}

cbuffer cbPBRRendererAttibs
{
    PBRRendererShaderParameters g_PBRRendererAttibs;
}

Texture2D<float3> g_TextureBaseColor;
Texture2D<float2> g_TextureMaterialData;
Texture2D<float4> g_TextureNormal;
Texture2D<float>  g_TextureDepth;
Texture2D<float4> g_TextureSSR;
Texture2D<float>  g_TextureSSAO;

TextureCube<float3> g_TextureEnvironmentMap;
SamplerState        g_TextureEnvironmentMap_sampler;

TextureCube<float3> g_TextureIrradianceMap;
SamplerState        g_TextureIrradianceMap_sampler;

TextureCube<float3> g_TexturePrefilteredEnvironmentMap;
SamplerState        g_TexturePrefilteredEnvironmentMap_sampler;

Texture2D<float2> g_TextureBRDFIntegrationMap;
SamplerState      g_TextureBRDFIntegrationMap_sampler;

struct SurfaceInformation
{
    float3 BaseColor;
    float  Roughness;
    float  Metalness;
    float3 Normal;
};

float3 FresnelSchlickRoughness(float CosTheta, float3 F0, float roughness)
{
    float Alpha = 1.0 - roughness;
    return F0 + (max(float3(Alpha, Alpha, Alpha), F0) - F0) * pow(clamp(1.0 - CosTheta, 0.0, 1.0), 5.0);
}

float3 SampleEnvironmentMap(float3 Coord)
{
    return g_PBRRendererAttibs.IBLScale * g_TextureEnvironmentMap.SampleLevel(g_TextureEnvironmentMap_sampler, float3(+1.0, -1.0, +1.0) * Coord, 0.5);
}

float3 SampleIrradianceMap(float3 Coord)
{
    return g_PBRRendererAttibs.IBLScale * g_TextureIrradianceMap.Sample(g_TextureIrradianceMap_sampler, float3(+1.0, -1.0, +1.0) * Coord);
}

float3 SamplePrefilteredEnvironmentMap(float3 Coord, float MipLevel)
{
    return g_PBRRendererAttibs.IBLScale * g_TexturePrefilteredEnvironmentMap.SampleLevel(g_TexturePrefilteredEnvironmentMap_sampler, float3(+1.0, -1.0, +1.0) * Coord, MipLevel);
}

float2 SampleBRDFIntegrationMap(float2 Coord)
{
    return g_TextureBRDFIntegrationMap.Sample(g_TextureBRDFIntegrationMap_sampler, Coord);
}

float3 ComputeDiffuseIBL(float3 N)
{
    return SampleIrradianceMap(N);
}

float3 ComputeSpecularIBL(float2 Location, float3 F0, float3 N, float3 V, float Roughness)
{
    float NdotV = saturate(dot(N, V));
    float3 R = reflect(-V, N);
    float2 BRDF_LUT = SampleBRDFIntegrationMap(float2(NdotV, Roughness));
    float4 SSR = g_TextureSSR.Load(int3(Location, 0));
    float3 T1 = SamplePrefilteredEnvironmentMap(R, Roughness * g_PBRRendererAttibs.PrefilteredCubeLastMip);
    float3 T2 = (F0 * BRDF_LUT.x + BRDF_LUT.yyy);
    return lerp(T1, SSR.rgb, SSR.w * g_Camera.f4ExtraData[0].x) * T2;
}

SurfaceInformation ExtractGBuffer(FullScreenTriangleVSOutput VSOut)
{
    float2 MaterialData = g_TextureMaterialData.Load(int3(VSOut.f4PixelPos.xy, 0));

    SurfaceInformation Information;
    Information.BaseColor = g_TextureBaseColor.Load(int3(VSOut.f4PixelPos.xy, 0));
    Information.Roughness = MaterialData.x;
    Information.Metalness = MaterialData.y;
    Information.Normal = normalize(g_TextureNormal.Load(int3(VSOut.f4PixelPos.xy, 0)).xyz);
    return Information;
}

float3 CreateViewDir(float2 NormalizedXY)
{
    float4 RayStart = mul(float4(NormalizedXY, DepthToNormalizedDeviceZ(0.0), 1.0f), g_Camera.mViewProjInv);
    float4 RayEnd   = mul(float4(NormalizedXY, DepthToNormalizedDeviceZ(1.0), 1.0f), g_Camera.mViewProjInv);

    RayStart.xyz /= RayStart.w;
    RayEnd.xyz   /= RayEnd.w;
    return normalize(RayStart.xyz - RayEnd.xyz);;
}

float4 ComputeLightingPS(FullScreenTriangleVSOutput VSOut) : SV_Target0
{
    float Depth = g_TextureDepth.Load(int3(VSOut.f4PixelPos.xy, 0));
    if (Depth >= 1.0 - FLT_EPS)
    {
        float3 ViewDirBackground = CreateViewDir(VSOut.f2NormalizedXY + g_Camera.f2Jitter);
        return float4(SampleEnvironmentMap(-ViewDirBackground), 1.0);
    }
      
    float3 ViewDir = CreateViewDir(VSOut.f2NormalizedXY);
    SurfaceInformation SurfInfo = ExtractGBuffer(VSOut);
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), SurfInfo.BaseColor, SurfInfo.Metalness);
    float3 F = FresnelSchlickRoughness(saturate(dot(SurfInfo.Normal, ViewDir)), F0, SurfInfo.Roughness);

    float3 kS = F;
    float3 kD = (float3(1.0, 1.0, 1.0) - kS) * (1.0 - SurfInfo.Metalness);

    float3 Diffuse = SurfInfo.BaseColor * ComputeDiffuseIBL(SurfInfo.Normal);
    float3 Specular = ComputeSpecularIBL(VSOut.f4PixelPos.xy, F0, SurfInfo.Normal, ViewDir, SurfInfo.Roughness);
    float Occlusion = lerp(1.0, g_TextureSSAO.Load(int3(VSOut.f4PixelPos.xy, 0)), g_Camera.f4ExtraData[0].y);

    // It's noteworthy that we apply ambient occlusion to the entire sum (Diffuse + Specular), which is not physically accurate, as
    // ambient occlusion should only be applied to the diffuse component. Currently, we do not compute a visibility function for the specular component,
    // so multiplying the entire sum by ambient occlusion is better than nothing
    return float4((kD * Diffuse + Specular) * Occlusion, 1.0);
}
