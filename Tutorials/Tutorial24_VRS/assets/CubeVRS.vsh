#include "Structures.fxh"

ConstantBuffer<Constants> g_Constants;

struct VSInput
{
    float3 Pos : ATTRIB0;
    float2 UV  : ATTRIB1;
};

struct PSInput
{
    float4 Pos : SV_POSITION; 
    float2 UV  : TEX_COORD; 

#ifndef METAL
    nointerpolation uint Rate : SV_ShadingRate;
#endif
};

void main(in  VSInput VSIn,
          in  uint    vid : SV_VertexID,
          out PSInput PSIn)
{
    PSIn.Pos = mul(float4(VSIn.Pos, 1.0), g_Constants.WorldViewProj);
    PSIn.UV  = VSIn.UV;
    
#ifndef METAL
    PSIn.Rate = g_Constants.PrimitiveShadingRate;
#endif
}
