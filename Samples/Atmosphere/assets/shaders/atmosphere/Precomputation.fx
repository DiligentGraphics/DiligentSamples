// Precomputation.fx
// Precomputes optical depth, single and multiple scattering look-up tables

#include "AtmosphereShadersCommon.fxh"

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 16
#endif

cbuffer cbParticipatingMediaScatteringParams
{
    AirScatteringAttribs g_MediaParams;
}

cbuffer cbMiscDynamicParams
{
    MiscDynamicParams g_MiscParams;
}

Texture2D<float2> g_tex2DOccludedNetDensityToAtmTop;
SamplerState g_tex2DOccludedNetDensityToAtmTop_sampler;

Texture3D<float3> g_tex3DPreviousSctrOrder;
SamplerState g_tex3DPreviousSctrOrder_sampler;


Texture3D<float3> g_tex3DPointwiseSctrRadiance;
SamplerState g_tex3DPointwiseSctrRadiance_sampler;

Texture2D<float3> g_tex2DSphereRandomSampling;

Texture3D<float3> g_tex3DSingleSctrLUT;
Texture3D<float3> g_tex3DHighOrderSctrLUT;

Texture3D<float3> g_tex3DMultipleSctrLUT;
SamplerState g_tex3DMultipleSctrLUT_sampler;

#include "LookUpTables.fxh"
#include "ScatteringIntegrals.fxh"

RWTexture3D</*format = rgba16f*/float3> g_rwtex3DSingleScattering;
RWTexture3D</*format = rgba32f*/float3> g_rwtex3DSctrRadiance;
RWTexture3D</*format = rgba32f*/float3> g_rwtex3DInsctrOrder;
RWTexture3D</*format = rgba16f*/float3> g_rwtex3DMultipleSctr;
RWTexture3D</*format = rgba16f*/float3> g_rwtex3DHighOrderSctr;

float2 IntegrateParticleDensity(in float3 f3Start, 
                                in float3 f3End,
                                in float3 f3EarthCentre,
                                float fNumSteps )
{
    float3 f3Step = (f3End - f3Start) / fNumSteps;
    float fStepLen = length(f3Step);
        
    float fStartHeightAboveSurface = abs( length(f3Start - f3EarthCentre) - g_MediaParams.fEarthRadius );
    float2 f2PrevParticleDensity = exp( -fStartHeightAboveSurface / g_MediaParams.f2ParticleScaleHeight );

    float2 f2ParticleNetDensity = float2(0.0, 0.0);
    for(float fStepNum = 1.0; fStepNum <= fNumSteps; fStepNum += 1.0)
    {
        float3 f3CurrPos = f3Start + f3Step * fStepNum;
        float fHeightAboveSurface = abs( length(f3CurrPos - f3EarthCentre) - g_MediaParams.fEarthRadius );
        float2 f2ParticleDensity = exp( -fHeightAboveSurface / g_MediaParams.f2ParticleScaleHeight );
        f2ParticleNetDensity += (f2ParticleDensity + f2PrevParticleDensity) * fStepLen / 2.0;
        f2PrevParticleDensity = f2ParticleDensity;
    }
    return f2ParticleNetDensity;
}

float2 IntegrateParticleDensityAlongRay(in float3 f3Pos, 
                                        in float3 f3RayDir,
                                        float3 f3EarthCentre, 
                                        const float fNumSteps,
                                        const bool bOccludeByEarth)
{
    if( bOccludeByEarth )
    {
        // If the ray intersects the Earth, return huge optical depth
        float2 f2RayEarthIsecs; 
        GetRaySphereIntersection(f3Pos, f3RayDir, f3EarthCentre, g_MediaParams.fEarthRadius, f2RayEarthIsecs);
        if( f2RayEarthIsecs.x > 0.0 )
            return float2(1e+20, 1e+20);
    }

    // Get intersection with the top of the atmosphere (the start point must always be under the top of it)
    //      
    //                     /
    //                .   /  . 
    //      .  '         /\         '  .
    //                  /  f2RayAtmTopIsecs.y > 0
    //                 *
    //                   f2RayAtmTopIsecs.x < 0
    //                  /
    //      
    float2 f2RayAtmTopIsecs;
    GetRaySphereIntersection(f3Pos, f3RayDir, f3EarthCentre, g_MediaParams.fAtmTopRadius, f2RayAtmTopIsecs);
    float fIntegrationDist = f2RayAtmTopIsecs.y;

    float3 f3RayEnd = f3Pos + f3RayDir * fIntegrationDist;

    return IntegrateParticleDensity(f3Pos, f3RayEnd, f3EarthCentre, fNumSteps);
}

void PrecomputeNetDensityToAtmTopPS( ScreenSizeQuadVSOutput VSOut,
                                     out float2 f2Density : SV_Target0 )
{
    float2 f2UV = NormalizedDeviceXYToTexUV(VSOut.m_f2PosPS);
    // Do not allow start point be at the Earth surface and on the top of the atmosphere
    float fStartHeight = clamp( lerp(0.0, g_MediaParams.fAtmTopHeight, f2UV.x), 10.0, g_MediaParams.fAtmTopHeight-10.0 );

    float fCosTheta = f2UV.y * 2.0 - 1.0;
    float fSinTheta = sqrt( saturate(1.0 - fCosTheta*fCosTheta) );
    float3 f3RayStart = float3(0.0, 0.0, fStartHeight);
    float3 f3RayDir = float3(fSinTheta, 0.0, fCosTheta);
    
    float3 f3EarthCentre = float3(0.0, 0.0, -g_MediaParams.fEarthRadius);

    const float fNumSteps = 200.0;
    f2Density = IntegrateParticleDensityAlongRay(f3RayStart, f3RayDir, f3EarthCentre, fNumSteps, true);
}



float4 LUTCoordsFromThreadID( uint3 ThreadId )
{
    float4 f4Corrds;
    f4Corrds.xy = (float2( ThreadId.xy ) + float2(0.5,0.5) ) / PRECOMPUTED_SCTR_LUT_DIM.xy;

    uint uiW = ThreadId.z % uint(PRECOMPUTED_SCTR_LUT_DIM.z);
    uint uiQ = ThreadId.z / uint(PRECOMPUTED_SCTR_LUT_DIM.z);
    f4Corrds.zw = ( float2(uiW, uiQ) + float2(0.5,0.5) ) / PRECOMPUTED_SCTR_LUT_DIM.zw;
    return f4Corrds;
}

// This shader pre-computes the radiance of single scattering at a given point in given
// direction.
[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void PrecomputeSingleScatteringCS(uint3 ThreadId  : SV_DispatchThreadID)
{
    // Get attributes for the current point
    float4 f4LUTCoords = LUTCoordsFromThreadID(ThreadId);
    float fHeight, fCosViewZenithAngle, fCosSunZenithAngle, fCosSunViewAngle;
    InsctrLUTCoords2WorldParams( f4LUTCoords, fHeight, fCosViewZenithAngle, fCosSunZenithAngle, fCosSunViewAngle );
    float3 f3EarthCentre =  - float3(0.0, 1.0, 0.0) * EARTH_RADIUS;
    float3 f3RayStart = float3(0.0, fHeight, 0.0);
    float3 f3ViewDir = ComputeViewDir(fCosViewZenithAngle);
    float3 f3DirOnLight = ComputeLightDir(f3ViewDir, fCosSunZenithAngle, fCosSunViewAngle);
  
    // Intersect view ray with the top of the atmosphere and the Earth
    float4 f4Isecs;
    GetRaySphereIntersection2( f3RayStart, f3ViewDir, f3EarthCentre, 
                               float2(EARTH_RADIUS, ATM_TOP_RADIUS), 
                               f4Isecs);
    float2 f2RayEarthIsecs  = f4Isecs.xy;
    float2 f2RayAtmTopIsecs = f4Isecs.zw;

    if(f2RayAtmTopIsecs.y <= 0.0)
    {
        // This is just a sanity check and should never happen
        // as the start point is always under the top of the 
        // atmosphere (look at InsctrLUTCoords2WorldParams())
        g_rwtex3DSingleScattering[ThreadId] = F3ZERO;
        return;
    }

    // Set the ray length to the distance to the top of the atmosphere
    float fRayLength = f2RayAtmTopIsecs.y;
    // If ray hits Earth, limit the length by the distance to the surface
    if(f2RayEarthIsecs.x > 0.0)
        fRayLength = min(fRayLength, f2RayEarthIsecs.x);
    
    float3 f3RayEnd = f3RayStart + f3ViewDir * fRayLength;

    // Integrate single-scattering
    float3 f3Extinction, f3Inscattering;
    IntegrateUnshadowedInscattering(f3RayStart, 
                                    f3RayEnd,
                                    f3ViewDir,
                                    f3EarthCentre,
                                    f3DirOnLight.xyz,
                                    100.0,
                                    f3Inscattering,
                                    f3Extinction);

    g_rwtex3DSingleScattering[ThreadId] = f3Inscattering;
}


// This shader pre-computes the radiance of light scattered at a given point in given
// direction. It multiplies the previous order in-scattered light with the phase function 
// for each type of particles and integrates the result over the whole set of directions,
// see eq. (7) in [Bruneton and Neyret 08].
[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void ComputeSctrRadianceCS(uint3 ThreadId  : SV_DispatchThreadID)
{
    // Get attributes for the current point
    float4 f4LUTCoords = LUTCoordsFromThreadID(ThreadId);
    float fHeight, fCosViewZenithAngle, fCosSunZenithAngle, fCosSunViewAngle;
    InsctrLUTCoords2WorldParams( f4LUTCoords, fHeight, fCosViewZenithAngle, fCosSunZenithAngle, fCosSunViewAngle );
    float3 f3EarthCentre =  - float3(0.0, 1.0, 0.0) * EARTH_RADIUS;
    float3 f3RayStart = float3(0.0, fHeight, 0.0);
    float3 f3ViewDir = ComputeViewDir(fCosViewZenithAngle);
    float3 f3DirOnLight = ComputeLightDir(f3ViewDir, fCosSunZenithAngle, fCosSunViewAngle);
    
    // Compute particle density scale factor
    float2 f2ParticleDensity = exp( -float2(fHeight,fHeight) / PARTICLE_SCALE_HEIGHT );
    
    float3 f3SctrRadiance = F3ZERO;
    // Go through a number of samples randomly distributed over the sphere
    for(int iSample = 0; iSample < NUM_RANDOM_SPHERE_SAMPLES; ++iSample)
    {
        // Get random direction
        float3 f3RandomDir = normalize( g_tex2DSphereRandomSampling.Load(int3(iSample,0,0)) );
        // Get the previous order in-scattered light when looking in direction f3RandomDir (the light thus goes in direction -f3RandomDir)
        float4 f4UVWQ = -F4ONE;
        float3 f3PrevOrderSctr = LookUpPrecomputedScattering(f3RayStart, f3RandomDir, f3EarthCentre, f3DirOnLight.xyz, g_tex3DPreviousSctrOrder, g_tex3DPreviousSctrOrder_sampler, f4UVWQ); 
        
        // Apply phase functions for each type of particles
        // Note that total scattering coefficients are baked into the angular scattering coeffs
        float3 f3DRlghInsctr = f2ParticleDensity.x * f3PrevOrderSctr;
        float3 f3DMieInsctr  = f2ParticleDensity.y * f3PrevOrderSctr;
        float fCosTheta = dot(f3ViewDir, f3RandomDir);
        ApplyPhaseFunctions(f3DRlghInsctr, f3DMieInsctr, fCosTheta);

        f3SctrRadiance += f3DRlghInsctr + f3DMieInsctr;
    }
    // Since we tested N random samples, each sample covered 4*Pi / N solid angle
    // Note that our phase function is normalized to 1 over the sphere. For instance,
    // uniform phase function would be p(theta) = 1 / (4*Pi).
    // Notice that for uniform intensity I if we get N samples, we must obtain exactly I after
    // numeric integration
    f3SctrRadiance *= 4.0*PI / float(NUM_RANDOM_SPHERE_SAMPLES);

    g_rwtex3DSctrRadiance[ThreadId] = f3SctrRadiance;
}


// This shader computes in-scattering order for a given point and direction. It performs integration of the 
// light scattered at particular point along the ray, see eq. (11) in [Bruneton and Neyret 08].
[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void ComputeScatteringOrderCS(uint3 ThreadId  : SV_DispatchThreadID)
{
    // Get attributes for the current point
    float4 f4LUTCoords = LUTCoordsFromThreadID(ThreadId);
    float fHeight, fCosViewZenithAngle, fCosSunZenithAngle, fCosSunViewAngle;
    InsctrLUTCoords2WorldParams(f4LUTCoords, fHeight, fCosViewZenithAngle, fCosSunZenithAngle, fCosSunViewAngle );
    float3 f3EarthCentre =  - float3(0.0, 1.0, 0.0) * EARTH_RADIUS;
    float3 f3RayStart = float3(0.0, fHeight, 0.0);
    float3 f3ViewDir = ComputeViewDir(fCosViewZenithAngle);
    float3 f3DirOnLight = ComputeLightDir(f3ViewDir, fCosSunZenithAngle, fCosSunViewAngle);
    
    // Intersect the ray with the atmosphere and Earth
    float4 f4Isecs;
    GetRaySphereIntersection2( f3RayStart, f3ViewDir, f3EarthCentre, 
                               float2(EARTH_RADIUS, ATM_TOP_RADIUS), 
                               f4Isecs);
    float2 f2RayEarthIsecs  = f4Isecs.xy;
    float2 f2RayAtmTopIsecs = f4Isecs.zw;

    if(f2RayAtmTopIsecs.y <= 0.0)
    {
        // This is just a sanity check and should never happen
        // as the start point is always under the top of the 
        // atmosphere (look at InsctrLUTCoords2WorldParams())
        g_rwtex3DInsctrOrder[ThreadId] = F3ZERO; 
        return;
    }

    float fRayLength = f2RayAtmTopIsecs.y;
    if(f2RayEarthIsecs.x > 0.0)
        fRayLength = min(fRayLength, f2RayEarthIsecs.x);
    
    float3 f3RayEnd = f3RayStart + f3ViewDir * fRayLength;

    const float fNumSamples = 64.0;
    float fStepLen = fRayLength / fNumSamples;

    float4 f4UVWQ = -F4ONE;
    float3 f3PrevSctrRadiance = LookUpPrecomputedScattering(f3RayStart, f3ViewDir, f3EarthCentre, f3DirOnLight.xyz, g_tex3DPointwiseSctrRadiance, g_tex3DPointwiseSctrRadiance_sampler, f4UVWQ); 
    float2 f2PrevParticleDensity = exp( -fHeight / PARTICLE_SCALE_HEIGHT );

    float2 f2NetParticleDensityFromCam = F2ZERO;
    float3 f3Inscattering = F3ZERO;

    for(float fSample=1.0; fSample <= fNumSamples; ++fSample)
    {
        float3 f3Pos = lerp(f3RayStart, f3RayEnd, fSample/fNumSamples);

        float fCurrHeight = length(f3Pos - f3EarthCentre) - EARTH_RADIUS;
        float2 f2ParticleDensity = exp( -fCurrHeight / PARTICLE_SCALE_HEIGHT );

        f2NetParticleDensityFromCam += (f2PrevParticleDensity + f2ParticleDensity) * (fStepLen / 2.0);
        f2PrevParticleDensity = f2ParticleDensity;
        
        // Get optical depth
        float3 f3RlghOpticalDepth = g_MediaParams.f4RayleighExtinctionCoeff.rgb * f2NetParticleDensityFromCam.x;
        float3 f3MieOpticalDepth  = g_MediaParams.f4MieExtinctionCoeff.rgb      * f2NetParticleDensityFromCam.y;
        
        // Compute extinction from the camera for the current integration point:
        float3 f3ExtinctionFromCam = exp( -(f3RlghOpticalDepth + f3MieOpticalDepth) );

        // Get attenuated scattered light radiance in the current point
        float4 f4UVWQ = -F4ONE;
        float3 f3SctrRadiance = f3ExtinctionFromCam * LookUpPrecomputedScattering(f3Pos, f3ViewDir, f3EarthCentre, f3DirOnLight.xyz, g_tex3DPointwiseSctrRadiance, g_tex3DPointwiseSctrRadiance_sampler, f4UVWQ); 
        // Update in-scattering integral
        f3Inscattering += (f3SctrRadiance +  f3PrevSctrRadiance) * (fStepLen/2.0);
        f3PrevSctrRadiance = f3SctrRadiance;
    }

    g_rwtex3DInsctrOrder[ThreadId] = f3Inscattering;
}

Texture3D<float3> g_tex3DCurrentOrderScattering;
[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void InitHighOrderScatteringCS(uint3 ThreadId  : SV_DispatchThreadID)
{
    // Accumulate in-scattering using alpha-blending
    g_rwtex3DHighOrderSctr[ThreadId] = g_tex3DCurrentOrderScattering.Load( int4(ThreadId, 0) );
}

Texture3D<float3> g_tex3DHighOrderOrderScattering;
[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void UpdateHighOrderScatteringCS(uint3 ThreadId  : SV_DispatchThreadID)
{
    // Accumulate in-scattering using alpha-blending
    g_rwtex3DHighOrderSctr[ThreadId] = 
        g_tex3DHighOrderOrderScattering.Load( int4(ThreadId, 0) ) + 
        g_tex3DCurrentOrderScattering.Load( int4(ThreadId, 0) );
}


[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void CombineScatteringOrdersCS(uint3 ThreadId  : SV_DispatchThreadID)
{
    // Combine single & higher order scattering into single look-up table
    g_rwtex3DMultipleSctr[ThreadId] = 
                     g_tex3DSingleSctrLUT.Load( int4(ThreadId, 0) ) + 
                     g_tex3DHighOrderSctrLUT.Load( int4(ThreadId, 0) );
}


void PrecomputeAmbientSkyLightPS(ScreenSizeQuadVSOutput VSOut,
                                 // IMPORTANT: non-system generated pixel shader input
                                 // arguments must have the exact same name as vertex shader 
                                 // outputs and must go in the same order.

                                 out float4 f4SkyLight : SV_Target)
{
    float fU = NormalizedDeviceXYToTexUV(VSOut.m_f2PosPS).x;
    float3 f3RayStart = float3(0.0, 20.0, 0.0);
    float3 f3EarthCentre =  -float3(0.0, 1.0, 0.0) * EARTH_RADIUS;
    float fCosZenithAngle = clamp(fU * 2.0 - 1.0, -1.0, +1.0);
    float3 f3DirOnLight = float3(sqrt(saturate(1.0 - fCosZenithAngle*fCosZenithAngle)), fCosZenithAngle, 0.0);
    f4SkyLight = F4ZERO;
    // Go through a number of random directions on the sphere
    for(int iSample = 0; iSample < NUM_RANDOM_SPHERE_SAMPLES; ++iSample)
    {
        // Get random direction
        float3 f3RandomDir = normalize( g_tex2DSphereRandomSampling.Load(int3(iSample,0,0)) );
        // Reflect directions from the lower hemisphere
        f3RandomDir.y = abs(f3RandomDir.y);
        // Get multiple scattered light radiance when looking in direction f3RandomDir (the light thus goes in direction -f3RandomDir)
        float4 f4UVWQ = -F4ONE;
        float3 f3Sctr = LookUpPrecomputedScattering(f3RayStart, f3RandomDir, f3EarthCentre, f3DirOnLight.xyz, g_tex3DMultipleSctrLUT, g_tex3DMultipleSctrLUT_sampler, f4UVWQ); 
        // Accumulate ambient irradiance through the horizontal plane
        f4SkyLight.rgb += f3Sctr * dot(f3RandomDir, float3(0.0, 1.0, 0.0));
    }
    // Each sample covers 2 * PI / NUM_RANDOM_SPHERE_SAMPLES solid angle (integration is performed over
    // upper hemisphere)
    f4SkyLight.rgb *= 2.0 * PI / float(NUM_RANDOM_SPHERE_SAMPLES);
}
