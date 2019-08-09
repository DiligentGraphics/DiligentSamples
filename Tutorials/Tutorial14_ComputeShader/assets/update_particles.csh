#include "structures.fxh"

cbuffer Constants
{
    GlobalConstants g_Constants;
};

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 64
#endif

RWStructuredBuffer<ParticleAttribs> g_Particles;

[numthreads(THREAD_GROUP_SIZE, 1, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID)
{
    uint uiParticleIdx = Gid.x * uint(THREAD_GROUP_SIZE) + GTid.x;
    if (uiParticleIdx >= g_Constants.uiNumParticles)
        return;

    float2 f2Scale = saturate(float2(1.0 / g_Constants.fAspectRatio, g_Constants.fAspectRatio));

    ParticleAttribs Particle = g_Particles[uiParticleIdx];
    Particle.f2Pos += Particle.f2Speed * g_Constants.fDeltaTime;
    
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

    g_Particles[uiParticleIdx] = Particle;
}
