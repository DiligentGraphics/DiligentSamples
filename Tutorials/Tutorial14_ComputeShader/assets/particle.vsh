#include "structures.fxh"

StructuredBuffer<ParticleAttribs> g_Particles;

struct VSInput
{
    uint VertID : SV_VertexID;
    uint InstID : SV_InstanceID;
};

struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float2 uv  : TEX_COORD;
};

void main(in  VSInput VSIn,
          out PSInput PSIn)
{
    float4 pos_uv[4];
    pos_uv[0] = float4(-1.0,+1.0, 0.0,0.0);
    pos_uv[1] = float4(-1.0,-1.0, 0.0,1.0);
    pos_uv[2] = float4(+1.0,+1.0, 1.0,0.0);
    pos_uv[3] = float4(+1.0,-1.0, 1.0,1.0);

    ParticleAttribs Attribs = g_Particles[VSIn.InstID];

    float2 pos = pos_uv[VSIn.VertID].xy;
    pos = pos * 0.015 + Attribs.f2Pos;
    PSIn.Pos = float4(pos, 0.0, 1.0);
    PSIn.uv = pos_uv[VSIn.VertID].zw;
}
