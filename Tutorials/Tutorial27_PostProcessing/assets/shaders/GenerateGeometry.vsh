#include "BasicStructures.fxh"
#include "GeometryStructures.fxh"

struct VSInput
{
    float3 Position : ATTRIB0;
};

struct PSInput
{
    float4 PixelPosition : SV_POSITION;
};

cbuffer cbObjectAttribs
{
    ObjectAttribs g_ObjectAttribs;
}

void GenerateGeometryVS(VSInput VSIn, out PSInput VSOut)
{
    VSOut.PixelPosition = mul(float4(VSIn.Position, 1.0), g_ObjectAttribs.CurrWorldViewProjectMatrix);
}
