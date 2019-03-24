
#include "HostSharedTerrainStructs.fxh"
#include "ToneMappingStructures.fxh"
#include "EpipolarLightScatteringStructures.fxh"
#include "TerrainShadersCommon.fxh"

cbuffer cbTerrainAttribs
{
    TerrainAttribs g_TerrainAttribs;
};

cbuffer cbCameraAttribs
{
    CameraAttribs g_CameraAttribs;
}

cbuffer cbLightAttribs
{
    LightAttribs g_LightAttribs;
};

cbuffer cbParticipatingMediaScatteringParams
{
    AirScatteringAttribs g_MediaParams;
}

Texture2D< float2 > g_tex2DOccludedNetDensityToAtmTop;
SamplerState        g_tex2DOccludedNetDensityToAtmTop_sampler;

Texture2D< float3 > g_tex2DAmbientSkylight;
SamplerState        g_tex2DAmbientSkylight_sampler;

void GetSunLightExtinctionAndSkyLight(in float3 f3PosWS,
                                      out float3 f3Extinction,
                                      out float3 f3AmbientSkyLight)
{
    float3 f3EarthCentre = float3(0.0, -g_MediaParams.fEarthRadius, 0.0);
    float3 f3DirFromEarthCentre = f3PosWS - f3EarthCentre;
    float fDistToCentre = length(f3DirFromEarthCentre);
    f3DirFromEarthCentre /= fDistToCentre;
    float fHeightAboveSurface = fDistToCentre - g_MediaParams.fEarthRadius;
    float fCosZenithAngle = dot(f3DirFromEarthCentre, -g_LightAttribs.f4Direction.xyz);

    float fRelativeHeightAboveSurface = fHeightAboveSurface / g_MediaParams.fAtmTopHeight;
    float2 f2ParticleDensityToAtmTop = g_tex2DOccludedNetDensityToAtmTop.SampleLevel( g_tex2DOccludedNetDensityToAtmTop_sampler, float2(fRelativeHeightAboveSurface, fCosZenithAngle*0.5 + 0.5), 0 );
    
    float3 f3RlghOpticalDepth = g_MediaParams.f4RayleighExtinctionCoeff.rgb * f2ParticleDensityToAtmTop.x;
    float3 f3MieOpticalDepth  = g_MediaParams.f4MieExtinctionCoeff.rgb      * f2ParticleDensityToAtmTop.y;
        
    // And total extinction for the current integration point:
    f3Extinction = exp( -(f3RlghOpticalDepth + f3MieOpticalDepth) );
    
    f3AmbientSkyLight = g_tex2DAmbientSkylight.SampleLevel( g_tex2DAmbientSkylight_sampler, float2(fCosZenithAngle*0.5 + 0.5, 0.5), 0 );
}


void HemisphereVS(in float3 f3PosWS : ATTRIB0,
                  in float2 f2MaskUV0 : ATTRIB1,
                  out float4 f4PosPS : SV_Position,
                  out HemisphereVSOutput VSOut
                  // IMPORTANT: non-system generated pixel shader input
                  // arguments must have the exact same name as vertex shader 
                  // outputs and must go in the same order.
                 )
{
    VSOut.TileTexUV = f3PosWS.xz;

    f4PosPS = mul( float4(f3PosWS,1.0), g_CameraAttribs.mViewProj);
    
    float4 ShadowMapSpacePos = mul( float4(f3PosWS,1.0), g_LightAttribs.ShadowAttribs.mWorldToLightView);
    VSOut.f3PosInLightViewSpace = ShadowMapSpacePos.xyz / ShadowMapSpacePos.w;
    VSOut.fCameraSpaceZ = f4PosPS.w;
    VSOut.f2MaskUV0 = f2MaskUV0;
    float3 f3Normal = normalize(f3PosWS - float3(0.0, -g_TerrainAttribs.m_fEarthRadius, 0.0));
    VSOut.f3Normal = f3Normal;
    VSOut.f3Tangent = normalize( cross(f3Normal, float3(0.0,0.0,1.0)) );
    VSOut.f3Bitangent = normalize( cross(VSOut.f3Tangent, f3Normal) );

    GetSunLightExtinctionAndSkyLight(f3PosWS, VSOut.f3SunLightExtinction, VSOut.f3AmbientSkyLight);
}
