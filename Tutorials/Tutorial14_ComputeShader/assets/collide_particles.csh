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

StructuredBuffer<ParticleAttribs>   g_InParticles;
RWStructuredBuffer<ParticleAttribs> g_OutParticles;
Buffer<int>                         g_ParticleListHead;
Buffer<int>                         g_ParticleLists;

bool CheckIntersection(in ParticleAttribs P0, in ParticleAttribs P1, out float2 R01, out float d01)
{
    P0.f2Pos.xy /= g_Constants.f2Scale.xy;
    P1.f2Pos.xy /= g_Constants.f2Scale.xy;
    R01 = P1.f2Pos.xy - P0.f2Pos.xy;
    d01 = length(R01);
    R01 /= d01;
    return d01 < P0.fSize + P1.fSize;
}

// https://en.wikipedia.org/wiki/Elastic_collision
bool CollideParticles(in ParticleAttribs P0, in ParticleAttribs P1, inout float2 f2PosDelta)
{
    float2 R01;
    float d01;
    if (CheckIntersection(P0, P1, R01, d01))
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

void UpdateSpeed(in ParticleAttribs OriginalP0, in ParticleAttribs OriginalP1, 
                 in ParticleAttribs UpdatedP0, in ParticleAttribs UpdatedP1,
                 inout float2 f2SpeedDelta)
{
    // If particles previously collided but now do not intersect, update the speed
    float2 Orig_R01, Updt_R01;
    float Orig_d01, Updt_d01;
    if (CheckIntersection(OriginalP0, OriginalP1, Orig_R01, Orig_d01) &&
        !CheckIntersection(UpdatedP0, UpdatedP1, Updt_R01, Updt_d01))
    {
        float v0 = dot(OriginalP0.f2Speed.xy, Orig_R01);
        float v1 = dot(OriginalP1.f2Speed.xy, Orig_R01);

        float m0 = OriginalP0.fSize * OriginalP0.fSize;
        float m1 = OriginalP1.fSize * OriginalP1.fSize;

        float new_v0 = ((m0 - m1) * v0 + 2.0 * m1 * v1) / (m0 + m1);
        f2SpeedDelta += (new_v0 - v0) * Orig_R01;
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
#if UPDATE_SPEED
                    UpdateSpeed(Particle, AnotherParticle, g_OutParticles[iParticleIdx], g_OutParticles[AnotherParticleIdx], f2SpeedDelta);
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
    g_OutParticles[iParticleIdx].f2Speed += f2SpeedDelta;
#else
    Particle.f2Pos += f2PosDelta;
    ClampParticlePosition(Particle, g_Constants.f2Scale);
    g_OutParticles[iParticleIdx] = Particle;
#endif
}
