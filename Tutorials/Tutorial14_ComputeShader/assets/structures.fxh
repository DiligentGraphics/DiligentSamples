
struct ParticleAttribs
{
    float2 f2Pos;
    float2 f2Speed;

    float  fSize;
    float  fDummy0;
    float  fDummy1;
    float  fDummy2;
};

struct GlobalConstants
{
    uint  uiNumParticles;
    float fAspectRatio;
    float fDeltaTime;
};
