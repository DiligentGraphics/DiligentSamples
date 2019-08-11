#include "structures.fxh"
#include "particles.fxh"

cbuffer Constants
{
    GlobalConstants g_Constants;
};

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 64
#endif

StructuredBuffer<ParticleAttribs>   g_InParticles;
RWStructuredBuffer<ParticleAttribs> g_OutParticles;
Buffer<int>                         g_ParticleListHead;
Buffer<int>                         g_ParticleLists;

bool TestCapsuleSphereIntesection(float2 f2CapsuleStart, float2 f2CapsuleDir, float CapsuleRadius, float2 f2SphereCenter, float SphereRadius)
{
    float fCapsuleLen = length(f2CapsuleDir);
    f2CapsuleDir /= fCapsuleLen;

    float2 DirToSphere = f2SphereCenter - f2CapsuleStart;
    float d0 = dot(DirToSphere, f2CapsuleDir);
    float d1_sq = dot(DirToSphere, DirToSphere) - d0*d0;
    d0 = max(max(-d0, d0 - fCapsuleLen), 0.0);
    return d0*d0 + d1_sq < (CapsuleRadius + SphereRadius) * (CapsuleRadius + SphereRadius);
}

// https://en.wikipedia.org/wiki/Elastic_collision
bool CollideParticles(in ParticleAttribs P0, in ParticleAttribs P1, float2 f2Scale, inout float2 f2PosDelta, inout float2 f2SpeedDelta)
{
    P0.f2Pos.xy /= f2Scale.xy;
    P1.f2Pos.xy /= f2Scale.xy;
    float2 R01 = P1.f2Pos.xy - P0.f2Pos.xy;
    float d01 = length(R01);
    R01 /= d01;
    if (d01 < P0.fSize + P1.fSize)
    {
        // If two particles intersect, update their positions, but do NOT update speed.
        f2PosDelta += -R01 * (P0.fSize + P1.fSize - d01) * f2Scale.xy * 0.5;
        return true;
    }
    // If particles don't intersect, extrapolate their movement and check if they intersect after the next update
    else if (TestCapsuleSphereIntesection(P0.f2Pos.xy, (P0.f2Speed - P1.f2Speed) * g_Constants.fDeltaTime, P0.fSize, P1.f2Pos.xy, P1.fSize))
    {
        float v0 = dot(P0.f2Speed.xy, R01);
        float v1 = dot(P1.f2Speed.xy, R01);

        float m0 = P0.fSize * P0.fSize;
        float m1 = P1.fSize * P1.fSize;

        float new_v0 = ((m0 - m1) * v0 + 2.0 * m1 * v1) / (m0 + m1);
        f2SpeedDelta += (new_v0 - v0) * R01;

        return true;
    }
    else
    {
        return false;
    }
}

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID)
{
    uint uiGlobalThreadIdx = Gid.x * uint(THREAD_GROUP_SIZE) + GTid.x;
    if (uiGlobalThreadIdx >= g_Constants.uiNumParticles)
        return;

    int iParticleIdx = int(uiGlobalThreadIdx);
    ParticleAttribs Particle = g_InParticles[iParticleIdx];
    
    int2 i2GridPos = GetGridLocation(Particle.f2Pos, g_Constants.i2ParticleGridSize).xy;
    int GridWidth  = g_Constants.i2ParticleGridSize.x;
    int GridHeight = g_Constants.i2ParticleGridSize.y;
    float2 f2Scale  = g_Constants.f2Scale;

    float2 f2PosDelta   = float2(0.0, 0.0);
    float2 f2SpeedDelta = float2(0.0, 0.0);

    for (int y = max(i2GridPos.y - 1, 0); y <= min(i2GridPos.y + 1, GridHeight-1); ++y)
    {
        for (int x = max(i2GridPos.x - 1, 0); x <= min(i2GridPos.x + 1, GridWidth-1); ++x)
        {
            int AnotherParticleIdx = g_ParticleListHead.Load(x + y * GridWidth);
            while (AnotherParticleIdx >= 0)
            {
                if (iParticleIdx != AnotherParticleIdx)
                {
                    ParticleAttribs AnotherParticle = g_InParticles[AnotherParticleIdx];
                    if (CollideParticles(Particle, AnotherParticle, f2Scale, f2PosDelta, f2SpeedDelta))
                    {
                        Particle.fTemperature = 1.0;
                    }
                }

                AnotherParticleIdx = g_ParticleLists.Load(AnotherParticleIdx);
            }
        }
    }
    Particle.f2Pos   += f2PosDelta;
    Particle.f2Speed += f2SpeedDelta;
    ClampParticlePosition(Particle, f2Scale);
    g_OutParticles[iParticleIdx] = Particle;
}
