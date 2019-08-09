#include "structures.fxh"

cbuffer Constants
{
    GlobalConstants g_Constants;
};

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 64
#endif

RWStructuredBuffer<ParticleAttribs> g_InParticles;
RWStructuredBuffer<ParticleAttribs> g_OutParticles;
RWBuffer<int>                       g_ParticleListHead;
RWBuffer<int>                       g_ParticleLists;

void ClampParticlePosition(inout ParticleAttribs Particle, float2 f2Scale)
{
    if (Particle.f2Pos.x + Particle.fSize * f2Scale.x > 1.0)
    {
        Particle.f2Pos.x -= Particle.f2Pos.x + Particle.fSize * f2Scale.x - 1.0;
        Particle.f2Speed.x *= -1.0;
    }

    if (Particle.f2Pos.x - Particle.fSize * f2Scale.x < -1.0)
    {
        Particle.f2Pos.x += -1.0 - (Particle.f2Pos.x - Particle.fSize * f2Scale.x);
        Particle.f2Speed.x *= -1.0;
    }

    if (Particle.f2Pos.y + Particle.fSize * f2Scale.y > 1.0)
    {
        Particle.f2Pos.y -= Particle.f2Pos.y + Particle.fSize * f2Scale.y - 1.0;
        Particle.f2Speed.y *= -1.0;
    }

    if (Particle.f2Pos.y - Particle.fSize * f2Scale.y < -1.0)
    {
        Particle.f2Pos.y += -1.0 - (Particle.f2Pos.y - Particle.fSize * f2Scale.y);
        Particle.f2Speed.y *= -1.0;
    }
}

void CollideParticles(inout ParticleAttribs P0, in ParticleAttribs P1, float2 f2Scale)
{
    P0.f2Pos.xy /= f2Scale.xy;
    P1.f2Pos.xy /= f2Scale.xy;
    P0.f2Speed.xy /= f2Scale.xy;
    P1.f2Speed.xy /= f2Scale.xy;
    float2 R01 = P1.f2Pos.xy - P0.f2Pos.xy;
    float d01 = length(R01);
    if (d01 < P0.fSize + P0.fSize)
    {
        R01 /= d01;
        P0.f2Pos.xy -= R01 * (P0.fSize + P0.fSize - d01);
        P0.f2Speed.xy -= 2.0*max(dot(P0.f2Speed.xy, R01), 0.0) * R01;
        P0.fTemperature = 1.0;
    }
    P0.f2Pos.xy   *= f2Scale;
    P0.f2Speed.xy *= f2Scale;
}

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID)
{
    float2 f2Scale = saturate(float2(1.0 / g_Constants.fAspectRatio, g_Constants.fAspectRatio));
    int GridWidth  = int(sqrt(float(g_Constants.uiNumParticles) * f2Scale.x));
    int GridHeight = int(g_Constants.uiNumParticles) / GridWidth;

    uint uiGlobalThreadIdx = Gid.x * uint(THREAD_GROUP_SIZE) + GTid.x;
    if (uiGlobalThreadIdx < GridWidth * GridHeight)
        g_ParticleListHead[uiGlobalThreadIdx] = -1;

    DeviceMemoryBarrier();

    if (uiGlobalThreadIdx >= g_Constants.uiNumParticles)
        return;

    uint uiParticleIdx = uiGlobalThreadIdx;

    ParticleAttribs Particle = g_InParticles[uiParticleIdx];
    // Update particle positions
    Particle.f2Pos += Particle.f2Speed * g_Constants.fDeltaTime;
    Particle.fTemperature -= Particle.fTemperature * g_Constants.fDeltaTime * 1.0;

    ClampParticlePosition(Particle, f2Scale);
    g_InParticles[uiParticleIdx] = Particle;

    DeviceMemoryBarrier();

    // Bin particles
    int GridX = clamp(int((Particle.f2Pos.x + 1.0) * 0.5 * float(GridWidth)),  0, GridWidth-1);
    int GridY = clamp(int((Particle.f2Pos.y + 1.0) * 0.5 * float(GridHeight)), 0, GridHeight-1);
    int GridIdx = GridX + GridY * GridWidth;

    int OriginalListIdx;
    InterlockedExchange(g_ParticleListHead[GridIdx], int(uiParticleIdx), OriginalListIdx);
    g_ParticleLists[uiParticleIdx] = OriginalListIdx;

    DeviceMemoryBarrier();

    for (int y=max(GridY-1, 0); y <= min(GridY + 1, GridHeight-1); ++y)
    {
        for (int x=max(GridX-1, 0); x <= min(GridX + 1, GridWidth-1); ++x)
        {
            int AnotherParticleIdx = g_ParticleListHead[x + y * GridWidth];
            while (AnotherParticleIdx >= 0)
            {
                if (uiParticleIdx != uint(AnotherParticleIdx))
                {
                    ParticleAttribs AnotherParticle = g_InParticles[AnotherParticleIdx];
                    CollideParticles(Particle, AnotherParticle, f2Scale);
                }

                AnotherParticleIdx = g_ParticleLists[AnotherParticleIdx];
            }
        }
    }

    ClampParticlePosition(Particle, f2Scale);
    g_OutParticles[uiParticleIdx] = Particle;
}
