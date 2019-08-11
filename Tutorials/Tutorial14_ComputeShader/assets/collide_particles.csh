#include "structures.fxh"
#include "particles.fxh"

cbuffer Constants
{
    GlobalConstants g_Constants;
};

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 64
#endif

#ifndef UPDATE_SPEED
#   define UPDATE_SPEED 0
#endif

RWStructuredBuffer<ParticleAttribs> g_Particles;
Buffer<int>                         g_ParticleListHead;
Buffer<int>                         g_ParticleLists;

bool CheckIntersection(in float2 f2Pos0, in float fSize0, in float2 f2Pos1, in float fSize1, out float2 R01, out float d01)
{
    f2Pos0.xy /= g_Constants.f2Scale.xy;
    f2Pos1.xy /= g_Constants.f2Scale.xy;
    R01 = f2Pos1.xy - f2Pos0.xy;
    d01 = length(R01);
    R01 /= d01;
    return d01 < fSize0 + fSize1;
}

// https://en.wikipedia.org/wiki/Elastic_collision
bool CollideParticles(in ParticleAttribs P0, in ParticleAttribs P1, inout float2 f2PosDelta)
{
    float2 R01;
    float d01;
    if (CheckIntersection(P0.f2Pos, P0.fSize, P1.f2Pos, P1.fSize, R01, d01))
    {
        // If two particles intersect, update their positions, but do NOT update speed.
        f2PosDelta += -R01 * (P0.fSize + P1.fSize - d01) * g_Constants.f2Scale.xy * 0.51;
        return true;
    }
    else
    {
        return false;
    }
}

void UpdateSpeed(in ParticleAttribs P0, in ParticleAttribs P1, inout float2 f2SpeedDelta)
{
    // If particles previously collided but now do not intersect, update the speed
    float2 R01, New_R01;
    float d01, New_d01;
    if (CheckIntersection(P0.f2Pos, P0.fSize, P1.f2Pos, P1.fSize, R01, d01) &&
        !CheckIntersection(P0.f2NewPos, P0.fSize, P1.f2NewPos, P1.fSize, New_R01, New_d01))
    {
        float v0 = dot(P0.f2Speed.xy, R01);
        float v1 = dot(P1.f2Speed.xy, R01);

        float m0 = P0.fSize * P0.fSize;
        float m1 = P1.fSize * P1.fSize;

        float new_v0 = ((m0 - m1) * v0 + 2.0 * m1 * v1) / (m0 + m1);
        f2SpeedDelta += (new_v0 - v0) * R01;
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
    ParticleAttribs Particle = g_Particles[iParticleIdx];
    
    int2 i2GridPos = GetGridLocation(Particle.f2Pos, g_Constants.i2ParticleGridSize).xy;
    int GridWidth  = g_Constants.i2ParticleGridSize.x;
    int GridHeight = g_Constants.i2ParticleGridSize.y;

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
                    ParticleAttribs AnotherParticle = g_Particles[AnotherParticleIdx];
#if UPDATE_SPEED
                    UpdateSpeed(Particle, AnotherParticle, f2SpeedDelta);
#else
                    if (CollideParticles(Particle, AnotherParticle, f2PosDelta))
                    {
                        Particle.fTemperature = 1.0;
                    }
#endif
                }

                AnotherParticleIdx = g_ParticleLists.Load(AnotherParticleIdx);
            }
        }
    }

#if UPDATE_SPEED
    Particle.f2NewSpeed = Particle.f2Speed + f2SpeedDelta;
#else
    Particle.f2NewPos = Particle.f2Pos + f2PosDelta;
    ClampParticlePosition(Particle.f2NewPos, Particle.f2Speed, Particle.fSize, g_Constants.f2Scale);
#endif

    g_Particles[iParticleIdx] = Particle;
}
