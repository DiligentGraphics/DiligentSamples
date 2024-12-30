#include "common.fxh"

cbuffer cbConstants
{
    Constants g_Constants;
}

struct VSInput
{
    // Vertex attributes
    float3 Pos      : ATTRIB0; 
    float3 Normal   : ATTRIB1;

    // Instance attributes
    float4 TranslationAndScale : ATTRIB2;
    float4 Color               : ATTRIB3;
};

void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    float3 Pos = VSIn.Pos * VSIn.TranslationAndScale.w + VSIn.TranslationAndScale.xyz;
    PSIn.Pos    = mul(float4(Pos, 1.0), g_Constants.ViewProj);
    PSIn.Normal = VSIn.Normal;
    PSIn.Color  = VSIn.Color;
}
