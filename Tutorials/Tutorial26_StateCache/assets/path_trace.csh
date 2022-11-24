#include "structures.fxh"
#include "scene.fxh"
#include "hash.fxh"
#include "PBR_Common.fxh"

Texture2D g_BaseColor;
Texture2D g_Normal;
Texture2D g_Emittance;
Texture2D g_PhysDesc;
Texture2D g_Depth;

RWTexture2D<float4 /*format = rgba32f*/> g_Radiance;

cbuffer cbConstants
{
    ShaderConstants g_Constants;
}

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 8
#endif

// Returns a random cosine-weighted direction on the hemisphere around z = 1.
void SampleDirectionCosineHemisphere(in  float2 UV,  // Normal random variables
                                     out float3 Dir, // Direction
                                     out float  Prob // Probability of the generated direction
                                     )
{
    Dir.x = cos(2.0 * PI * UV.x) * sqrt(1.0 - UV.y);
    Dir.y = sin(2.0 * PI * UV.x) * sqrt(1.0 - UV.y);
    Dir.z = sqrt(UV.y);

    // Avoid zero probability
    Prob = max(Dir.z, 1e-6) / PI;
}

// Returns a random cosine-weighted direction on the hemisphere around N.
void SampleDirectionCosineHemisphere(in  float3 N,   // Normal
                                     in  float2 UV,  // Normal random variables
                                     out float3 Dir, // Direction
                                     out float  Prob // Probability of the generated direction
                                    )
{
	float3 T = normalize(cross(N, abs(N.y) > 0.5 ? float3(1, 0, 0) : float3(0, 1, 0)));
	float3 B = cross(T, N);
    SampleDirectionCosineHemisphere(UV, Dir, Prob);
    Dir = normalize(Dir.x * T + Dir.y * B + Dir.z * N);
}

SurfaceReflectanceInfo GetReflectanceInfo(Material Mat)
{
    float3 f0 = float3(0.04, 0.04, 0.04);

    SurfaceReflectanceInfo SrfInfo;
    SrfInfo.PerceptualRoughness = Mat.Roughness;
    SrfInfo.DiffuseColor        = Mat.BaseColor.rgb * (float3(1.0, 1.0, 1.0) - f0) * (1.0 - Mat.Metallic);
    SrfInfo.Reflectance0        = lerp(f0, Mat.BaseColor.rgb, Mat.Metallic);

    float reflectance = max(max(SrfInfo.Reflectance0.r, SrfInfo.Reflectance0.g), SrfInfo.Reflectance0.b);
    // Anything less than 2% is physically impossible and is instead considered to be shadowing. Compare to "Real-Time-Rendering" 4th editon on page 325.
    SrfInfo.Reflectance90 = clamp(reflectance * 50.0, 0.0, 1.0) * float3(1.0, 1.0, 1.0);

    return SrfInfo;
}

float3 BRDF(HitInfo Hit, float3 OutDir, float3 InDir)
{
    SurfaceReflectanceInfo SrfInfo = GetReflectanceInfo(Hit.Mat);

    float3 DiffuseContrib;
    float3 SpecContrib;
    float  NdotL;
    BRDF(InDir, // To light
         Hit.Normal,
         OutDir,
         SrfInfo,
         DiffuseContrib,
         SpecContrib,
         NdotL
    );

    return DiffuseContrib + SpecContrib;
}


void SampleBRDFDirection(HitInfo Hit, float3 View, float2 rnd2, out float3 Dir, out float3 Reflectance)
{
    float Prob;
    SampleDirectionCosineHemisphere(Hit.Normal, rnd2, Dir, Prob);

    float CosTheta = dot(Hit.Normal, Dir);
    Reflectance = BRDF(Hit, View, Dir) * CosTheta / Prob;
}

// Reconstructs primary ray from the G-buffer
void GetPrimaryRay(in    uint2   ScreenXY,
                   out   HitInfo Hit,
                   out   RayInfo Ray)
{
    float  fDepth          = g_Depth.Load(int3(ScreenXY, 0)).r;
    float4 f4BaseCol_Type0 = g_BaseColor.Load(int3(ScreenXY, 0));
    float2 f2PhysDesc      = g_PhysDesc.Load(int3(ScreenXY, 0)).rg;


    float3 HitPos = ScreenToWorld(float2(ScreenXY) + float2(0.5, 0.5),
                                  fDepth, 
                                  g_Constants.f2ScreenSize,
                                  g_Constants.ViewProjInvMat);

    Ray.Origin = g_Constants.CameraPos.xyz;
    float3 RayDir = HitPos - Ray.Origin;
    float  RayLen = length(RayDir);
    Ray.Dir = RayDir / RayLen;

    Hit.Mat.BaseColor = f4BaseCol_Type0.rgb;
    Hit.Mat.Emittance = g_Emittance.Load(int3(ScreenXY, 0)).rgb;
    Hit.Mat.Metallic  = f2PhysDesc.x;
    Hit.Mat.Roughness = f2PhysDesc.y;
    Hit.Mat.Type      = int(f4BaseCol_Type0.a * 255.0);

    Hit.Normal   = normalize(g_Normal.Load(int3(ScreenXY, 0)).xyz * 2.0 - 1.0);
    Hit.Distance = RayLen;
}

void SampleLight(LightAttribs Light, float2 rnd2, float3 f3HitPos, out float3 f3LightRadiance, out float3 f3DirToLight)
{
    float  fLightArea       = (Light.f2SizeXZ.x * 2.0) * (Light.f2SizeXZ.y * 2.0);
    float3 f3LightIntensity = Light.f4Intensity.rgb * Light.f4Intensity.a;
    float3 f3LightNormal    = Light.f4Normal.xyz;

    float3 f3LightSample = GetLightSamplePos(Light, rnd2);
    f3DirToLight  = f3LightSample - f3HitPos;
    float fDistToLightSqr = dot(f3DirToLight, f3DirToLight);
    f3DirToLight /= sqrt(fDistToLightSqr);

    // Trace shadow ray towards the light sample
    RayInfo ShadowRay;
    ShadowRay.Origin = f3HitPos;
    ShadowRay.Dir    = f3DirToLight;
    float fLightVisibility = TestShadow(ShadowRay);

    // In Monte-Carlo integration, we pretend that each sample speaks for the full light
    // source surface, so we project the entire light surface area onto the hemisphere
    // around the shaded point and see how much solid angle it covers.
    float fLightProjectedArea = fLightArea * max(dot(-f3DirToLight, f3LightNormal), 0.0) / fDistToLightSqr;
    // Notice that when not using NEE, we randomly sample the hemisphere and will
    // eventually cover the same solid angle.

    f3LightRadiance = fLightProjectedArea * f3LightIntensity * fLightVisibility;
}

void Reflect(HitInfo Hit, float3 f3HitPos, inout RayInfo Ray, inout float3 f3Througput)
{
    Ray.Origin = f3HitPos;
    Ray.Dir    = reflect(Ray.Dir, Hit.Normal);
    f3Througput *= Hit.Mat.BaseColor;
}


// Fresnel term
// https://en.wikipedia.org/wiki/Fresnel_equations
//
//             cosThetaI
//           |      .'
//           |    .'
//           |  .'      Ri
//   ________|.'__________    eta = Ri/Rt
//          /|
//         / |          Rt
//        /  |
//       /   |
//  cosThetaT
//
float Fresnel(float eta, float cosThetaI)
{
    cosThetaI = clamp(cosThetaI, -1.0, 1.0);
    if (cosThetaI < 0.0)
    {
        eta = 1.0 / eta;
        cosThetaI = -cosThetaI;
    }

    // Snell's law:
    // Ri * sin(ThetaI) = Rt * sin(ThetaT)
    float sinThetaTSq = eta * eta * (1.0 - cosThetaI * cosThetaI);
    if (sinThetaTSq >= 1.0)
    {
        // Total internal reflection
        return 1.0;
    }

    float cosThetaT = sqrt(1.0 - sinThetaTSq);

    float Rs = (eta * cosThetaI - cosThetaT) / (eta * cosThetaI + cosThetaT);
    float Rp = (eta * cosThetaT - cosThetaI) / (eta * cosThetaT + cosThetaI);

    return 0.5 * (Rs * Rs + Rp * Rp);
}

void Refract(HitInfo Hit, float3 f3HitPos, inout RayInfo Ray, inout float3 f3Througput, float rnd)
{
    // Compute fresnel term
    float AirIOR    = 1.0;
    float GlassIOR  = 1.52;
    float relIOR    = AirIOR / GlassIOR;
    float cosThetaI = dot(-Ray.Dir, Hit.Normal);
    if (cosThetaI < 0.0)
    {
        Hit.Normal *= -1.0;
        cosThetaI  *= -1.0;
        relIOR = 1.0 / relIOR;
    }
    float F = Fresnel(relIOR, cosThetaI);

    if (rnd <= F)
    {
        // Note that technically we need to multiply throughput by F,
        // but also by 1/P, but since P==F they cancel out.
        Reflect(Hit, f3HitPos, Ray, f3Througput);
    }
    else
    {
        Ray.Origin = f3HitPos;
        Ray.Dir    = refract(Ray.Dir, Hit.Normal, relIOR);

        // Note that refraction also changes the differential solid angle of the flux
        f3Througput /= relIOR * relIOR;
    }
}

[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void main(uint3 ThreadId : SV_DispatchThreadID)
{
    if (ThreadId.x >= g_Constants.u2ScreenSize.x ||
        ThreadId.y >= g_Constants.u2ScreenSize.y)
        return; // Outside of the screen

    HitInfo Hit0;
    RayInfo Ray0;
    GetPrimaryRay(ThreadId.xy, Hit0, Ray0);

    if (Hit0.Mat.Type == MAT_TYPE_NONE)
    {
        // Background
        g_Radiance[ThreadId.xy] = float4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    float3 f3Radiance = float3(0.0, 0.0, 0.0);

    // Rendering equation
    //
    //   L(x->v)         L(x<-w)
    //      '.           .'
    //        '.       .'
    //        v '.   .' w
    //            '.'
    //             x
    //
    //      L(x->v) = E(x) + Integral{ BRDF(x, v, w) * L(x<-w) * (n, w) * dw }
    //
    // Monte-Carlo integration:
    //
    //   L(x1->v)         x2
    //       .           .'.
    //        '.       .'   '.
    //        v '.   .' w1    '.w2
    //            '.'           '. 
    //             x1             x3
    //
    //      L(x1->v) = 1/N * Sum{ E(x1) + BRDF(x1, v1, w1) * [E(x2) + BRDF(x2, -w1, w2) * (...) * (n2, w2) * 1/p(w2)]  * (n1, w1) * 1/p(w1) }
    //
    //  This can be rewritten as
    //
    //      L(x1->v) = 1/N * { T0 * E(x1) + T1 * E(x2) + T2 * E(x3) + ... }
    //
    //  where Ti is the throughput after i bounces:
    //
    //      T0 = 1
    //      Ti = Ti-1 * BRDF(xi, vi, wi) * (ni, wi) / p(wi)

    // Make sure the seed is unique for each sample
    uint2 Seed = ThreadId.xy * uint2(11417, 7801) + uint2(g_Constants.uFrameSeed1, g_Constants.uFrameSeed2);
    for (int i = 0; i < g_Constants.iNumSamplesPerFrame; ++i)
    {
        // Each path starts with the primary camera ray
        HitInfo Hit = Hit0;
        RayInfo Ray = Ray0;

        // Total contribution of this path
        float3 f3PathContrib = float3(0.0, 0.0, 0.0);
        if (g_Constants.iUseNEE != 0)
        {
            // We need to add emittance from the first hit, which is like performing
            // light source sampling for the primary ray origin (aka "0-th" hit).
            f3PathContrib += Hit0.Mat.Emittance;
        }

        // Path throughput, or the maximum possible remaining contribution after all bounces so far.
        float3 f3Throughput = float3(1.0, 1.0, 1.0);
        // Note that when using next event estimation, we sample light source at each bounce.
        // To compensate for that, we add extra bounce when not using NEE.
        for (int j = 0; j < g_Constants.iNumBounces + (1 - g_Constants.iUseNEE); ++j)
        {
            if (g_Constants.iShowOnlyLastBounce != 0)
                f3PathContrib = float3(0.0, 0.0, 0.0);

            if (Hit.Mat.Type == MAT_TYPE_NONE)
                break;

            float3 f3HitPos = Ray.Origin + Ray.Dir * Hit.Distance;

            // Get random sample on the light source surface.
            float2 rnd2 = hash22(Seed);
            // Update the seed
            Seed += uint2(129, 1725);

            if (Hit.Mat.Type == MAT_TYPE_MIRROR)
            {
                Reflect(Hit, f3HitPos, Ray, f3Throughput);
                // Note: if NEE is enabled, we need to perform light sampling here.
                //       However, since we need to sample the light in the same reflected direction,
                //       we will add its contribution later when we find the next hit point.
            }
            else if (Hit.Mat.Type == MAT_TYPE_GLASS)
            {
                Refract(Hit, f3HitPos, Ray, f3Throughput, rnd2.x);
                // As with mirror, we will perform light sampling later
            }
            else
            {
                if (g_Constants.iUseNEE != 0)
                {
                    // Sample light source
                    float3 f3LightRadiance;
                    float3 f3DirToLight;
                    SampleLight(g_Constants.Light, rnd2, f3HitPos, f3LightRadiance, f3DirToLight);
                    float NdotL = max(dot(f3DirToLight, Hit.Normal), 0.0);

                    f3PathContrib +=
                        f3Throughput
                        * BRDF(Hit, -Ray.Dir, f3DirToLight)
                        * NdotL
                        * f3LightRadiance;
                }
                else
                {
                    f3PathContrib += f3Throughput * Hit.Mat.Emittance;
                }

                // NEE effectively performs one additional bounce
                if (j == g_Constants.iNumBounces - g_Constants.iUseNEE)
                {
                    // Last bounce - complete the loop
                    break; 
                }

                // Sample the BRDF
                float3 Reflectance;
                float3 Dir;
                SampleBRDFDirection(Hit, -Ray.Dir, rnd2, Dir, Reflectance);

                f3Throughput *= Reflectance;

                Ray.Origin = f3HitPos;
                Ray.Dir    = Dir;
            }

            // We did not perform next event estimation for the mirror surface and
            // we need to add emittance of the next hit point.
            bool AddEmittance = (g_Constants.iUseNEE != 0) && (Hit.Mat.Type == MAT_TYPE_MIRROR || Hit.Mat.Type == MAT_TYPE_GLASS);

            // Trace the scene in the selected direction
            Hit = IntersectScene(Ray, g_Constants.Light);

            if (AddEmittance)
                f3PathContrib += f3Throughput * Hit.Mat.Emittance;
        }

        // Combine contributions
        f3Radiance += f3PathContrib;
    }

    // Add the total radiance to the accumulation buffer
    if (g_Constants.fLastSampleCount > 0)
        f3Radiance += g_Radiance[ThreadId.xy].rgb;
    g_Radiance[ThreadId.xy] = float4(f3Radiance, 0.0);
}
