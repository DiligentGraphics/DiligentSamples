#include "structures.fxh"

cbuffer Constants
{
    GlobalConstants g_Constants;
};

RWStructuredBuffer<ParticleAttribs> g_Particles;

[numthreads(64, 1, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID)
{
    uint uiParticleIdx = Gid.x * 64 + GTid.x;
    if (uiParticleIdx >= g_Constants.uiNumParticles)
        return;

    ParticleAttribs Particle = g_Particles[uiParticleIdx];
    if (abs(Particle.f2Pos.x + Particle.f2Speed.x * g_Constants.fDeltaTime) > 1.0)
    {
        Particle.f2Speed.x *= -1.0;
    }
    if (abs(Particle.f2Pos.y + Particle.f2Speed.y * g_Constants.fDeltaTime) > 1.0)
    {
        Particle.f2Speed.y *= -1.0;
    }

    Particle.f2Pos += Particle.f2Speed * g_Constants.fDeltaTime;

    g_Particles[uiParticleIdx] = Particle;
}
