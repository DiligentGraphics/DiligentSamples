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

// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be identical.
void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    float3 Pos = VSIn.Pos * VSIn.TranslationAndScale.w + VSIn.TranslationAndScale.xyz;
    PSIn.Pos    = mul(float4(Pos, 1.0), g_Constants.ViewProj);
    PSIn.Normal = VSIn.Normal;
    PSIn.Color  = VSIn.Color;
}
