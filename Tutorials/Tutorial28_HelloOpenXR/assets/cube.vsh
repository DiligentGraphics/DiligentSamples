#include "shader_constants.h"

cbuffer Constants
{
    float4x4 g_WorldViewProj;
    float4x4 g_NormalTransform;
    float4   g_Color;
};

struct VSInput
{
    float3 Pos    : ATTRIB0;
    float3 Normal : ATTRIB1;
};

void main(in  VSInput VSIn,
          out PSInput PSIn)
{
    PSIn.Pos    = mul(float4(VSIn.Pos, 1.0), g_WorldViewProj);
    PSIn.Normal = mul(float4(VSIn.Normal, 0.0), g_NormalTransform);
    PSIn.Color  = g_Color;
}
