#include "Structures.fxh"

cbuffer DrawConstantsCB
{
    DrawConstants g_Constants;
};

struct VSInput
{
    float3 Pos  : ATTRIB0;
    float3 Norm : ATTRIB1;
    float3 UVW  : ATTRIB2;
};

struct PSInput
{
    float4 Pos  : SV_POSITION;
    float3 Norm : NORMAL;    // world-space normal
    float3 UVW  : TEX_COORD;
};

void main(in VSInput  VSIn,
          out PSInput PSIn)
{
    PSIn.Pos   = mul(float4(VSIn.Pos, 1.0), g_Constants.ModelViewProj);
    PSIn.Norm  = normalize(mul(VSIn.Norm, (float3x3)(g_Constants.NormalMat)));
    PSIn.UVW   = VSIn.UVW;
}
