
struct ParticleAttribs
{
    float2 f2Pos;
    float2 f2Speed;

    float  fSize;
    float  fTemperature;
    float  fDummy0;
    float  fDummy1;
};

struct GlobalConstants
{
    uint  uiNumParticles;
    float fAspectRatio;
    float fDeltaTime;
};
