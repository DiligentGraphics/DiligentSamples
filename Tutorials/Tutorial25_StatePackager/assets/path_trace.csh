#include "structures.fxh"
#include "scene.fxh"
#include "hash.fxh"

Texture2D g_Albedo;
Texture2D g_Normal;
Texture2D g_Emittance;
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


float3 BRDF(HitInfo Hit, float3 OutDir, float3 InDir)
{
    // Lambertian BRDF
    return Hit.Albedo / PI;
}

void SampleBRDFDirection(HitInfo Hit, float2 rnd2, out float3 Dir, out float Prob)
{
    SampleDirectionCosineHemisphere(Hit.Normal, rnd2, Dir, Prob);
}

// Reconstructs primary ray from the G-buffer
void GetPrimaryRay(in  uint2   ScreenXY,
                   out HitInfo Hit,
                   out RayInfo Ray)
{
    float  fDepth         = g_Depth.Load(int3(ScreenXY, 0)).x;
    float4 f4Albedo_Type0 = g_Albedo.Load(int3(ScreenXY, 0));

    float3 HitPos = ScreenToWorld(float2(ScreenXY) + float2(0.5, 0.5),
                                  fDepth, 
                                  g_Constants.f2ScreenSize,
                                  g_Constants.ViewProjInvMat);

    Ray.Origin = g_Constants.CameraPos.xyz;
    float3 RayDir = HitPos - Ray.Origin;
    float  RayLen = length(RayDir);
    Ray.Dir = RayDir / RayLen;

    Hit.Albedo    = f4Albedo_Type0.rgb;
    Hit.Emittance = g_Emittance.Load(int3(ScreenXY, 0)).rgb;
    Hit.Normal    = normalize(g_Normal.Load(int3(ScreenXY, 0)).xyz * 2.0 - 1.0);
    Hit.Distance  = RayLen;
    Hit.Type      = int(f4Albedo_Type0.a * 255.0);
}


// Sample a random point on the light source surface and evaluate terms
// required for the next event estimation.
//
//     L(x->v)      L(x<-l)
//          '.   .'
//        v   '.'   l
//             x
// 
//  L(x->v) = L(x->l) * V(x, l) * BRDF(x, v, l) *  (n, l) / p(l)
//
struct LightSampleAttribs
{
    float3 f3Emittance;  // L(x->l)
    float3 f3Dir;        // l
    float  fVisibility;  // V(l)
    float  Prob;         // p(l)
};
LightSampleAttribs SampleLightSource(in  LightAttribs Light, 
                                     in  float2       rnd2,
                                     in  float3       f3SrfPos // x
                                     )
{
    LightSampleAttribs Sample;

    Sample.f3Emittance = Light.f4Intensity.rgb * Light.f4Intensity.a;

    float3 f3SamplePos = GetLightSamplePos(Light, rnd2);
    Sample.f3Dir       = f3SamplePos - f3SrfPos;
    float fDistToLightSqr = dot(Sample.f3Dir, Sample.f3Dir);
    Sample.f3Dir /= sqrt(fDistToLightSqr);

    // Trace shadow ray towards the light
    RayInfo ShadowRay;
    ShadowRay.Origin   = f3SrfPos;
    ShadowRay.Dir      = Sample.f3Dir;
    Sample.fVisibility = TestShadow(ShadowRay);

    // We need to compute p(l) which is the probability density of the direction l
    // and is equal to 1 over the solid angle spanned by the light source:
    //
    //        ______
    //       /    .'
    //      /   .'
    //     /  .'
    //    / .'
    //   /.'
    //
    float  fLightArea    = (Light.f2SizeXZ.x * 2.0) * (Light.f2SizeXZ.y * 2.0);
    float3 f3LightNormal = Light.f4Normal.xyz;
    float  fSolidAngle   = fLightArea * max(dot(-Sample.f3Dir, f3LightNormal), 0.0) / fDistToLightSqr;

    Sample.Prob = fSolidAngle > 0.0 ? 1.0 / fSolidAngle : 0.0;

    return Sample;
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

    if (Hit0.Type == HIT_TYPE_NONE)
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
    //      L(x->v) = E(x) + Integral{ BRDF(x, v, w) * L(x<-w) * (n, w) * dw }      (1)
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
    //      L(x1->v) = 1/N * Sum{ E(x1) + BRDF(x1, v1, w1) * [E(x2) + BRDF(x2, -w1, w2) * (...) * (n2, w2) * 1/p(w2)]  * (n1, w1) * 1/p(w1) }   (2)
    //
    //  This can be rewritten as
    //
    //      L(x1->v) = 1/N * { T0 * E(x1) + T1 * E(x2) + T2 * E(x3) + ... }     (3)
    //
    //  where Ti is the throughput after i bounces:
    //
    //      T0 = 1                                              (4)
    //      Ti = Ti-1 * BRDF(xi, vi, wi) * (ni, wi) / p(wi)     (5)

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
            f3PathContrib += Hit0.Emittance;
        }

        // Path throughput, or the maximum possible remaining contribution after all bounces so far.
        float3 f3Throughput = float3(1.0, 1.0, 1.0); // T0

        // Note that when using next event estimation, we sample light source at each bounce.
        // To compensate for that, we add extra bounce when not using NEE.
        for (int j = 0; j < g_Constants.iNumBounces + (1 - g_Constants.iUseNEE); ++j)
        {
            if (g_Constants.iShowOnlyLastBounce != 0)
                f3PathContrib = float3(0.0, 0.0, 0.0);

            if (Hit.Type == HIT_TYPE_NONE)
                break;

            float3 f3HitPos = Ray.Origin + Ray.Dir * Hit.Distance;

            // Get uniform random variables
            float2 rnd2 = hash22(Seed);
            Seed += uint2(129, 1725);

            if (g_Constants.iUseNEE != 0)
            {
                // Trace a ray towards the light source to estimate E(xi) instead of using E(xi) from the hit:
                // 
                //      E(xi) = BRDF(xi, vi, li) * (ni, li) * L(xi, li) * V(xi, li) / p(li)
                //
                // Notice how this way we start with E(x2) and skip E(x1) in (2), which is why we needed to add
                // the primary ray hit emittance above.

                LightSampleAttribs LightSample = SampleLightSource(g_Constants.Light, rnd2, f3HitPos);
                if (LightSample.Prob > 0)
                {
                    float NdotL = max(dot(LightSample.f3Dir, Hit.Normal), 0.0);
                    f3PathContrib +=
                        f3Throughput                             // Ti-1
                        * BRDF(Hit, -Ray.Dir, LightSample.f3Dir) // BRDF(xi, vi, li)
                        * NdotL                                  // (ni, li)
                        * LightSample.f3Emittance                // L(xi, li)
                        * LightSample.fVisibility                // V(xi, li)
                        / LightSample.Prob;                      // p(li)

                    // Note that L in the rendering equation is radiance, however
                    // L * dl ~ Irradiance aka Emittance
                }
            }
            else
            {
                f3PathContrib += 
                    f3Throughput * // Ti-1
                    Hit.Emittance; // E(xi)
            }

            // NEE effectively performs one additional bounce
            if (j == g_Constants.iNumBounces - g_Constants.iUseNEE)
            {
                // Last bounce - complete the loop
                break; 
            }

            // Sample the BRDF - in our case this is a cosine-weighted hemispherical distribution.
            float3 Dir;
            float  Prob;
            SampleBRDFDirection(Hit, rnd2, Dir, Prob);

            float CosTheta = dot(Hit.Normal, Dir);
            // Ti = Ti-1 * BRDF(xi, vi, wi) * (ni, wi) / p(wi)
            f3Throughput = 
                f3Throughput                // Ti-1
                * BRDF(Hit, -Ray.Dir, Dir)  // BRDF(xi, vi, wi)
                * CosTheta                  // (ni, wi)
                / Prob;                     // p(wi)

            // Note that in case of a cosine-weighted distribution, ray direction
            // probability is cos(theta) / PI, and it cancels out with CosTheta and
            // 1/PI factor from BRDF. We however keep them here for the sake of generality.

            // Trace the scene in the selected direction
            Ray.Origin = f3HitPos;
            Ray.Dir    = Dir;
            Hit = IntersectScene(Ray, g_Constants.Light);
        }

        // Combine contributions
        f3Radiance += f3PathContrib;
    }

    // Add the total radiance to the accumulation buffer
    if (g_Constants.fLastSampleCount > 0)
        f3Radiance += g_Radiance[ThreadId.xy].rgb;
    g_Radiance[ThreadId.xy] = float4(f3Radiance, 0.0);
}
