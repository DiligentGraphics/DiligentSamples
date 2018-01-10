#include "structures.h"

cbuffer VSConstants
{
    Constants g_Constants;
};

// Vertex shader takes two inputs: vertex position and uv coordinates.
// By convention, Diligent Engine expects vertex shader inputs to 
// be labeled as ATTRIBn, where n is the attribute number
VSOutput main(float3 pos : ATTRIB0, 
              float2 uv : ATTRIB1) 
{
    VSOutput vs_out; 
    vs_out.Pos = mul( float4(pos,1.0), g_Constants.WorldViewProj);
    vs_out.uv = uv;
    return vs_out;
}
