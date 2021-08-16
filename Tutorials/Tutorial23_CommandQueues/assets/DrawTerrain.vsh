#include "Structures.fxh"

cbuffer DrawConstantsCB
{
    DrawConstants g_Constants;
};

cbuffer TerrainConstantsCB
{
    TerrainConstants g_TerrainConstants;
};

Texture2D<float4> g_TerrainHeightMap;

struct VSInput
{
    float2 UV : ATTRIB0;
};

struct PSInput
{
    float4 Pos : SV_POSITION;
    float2 UV  : TEX_COORD;
};

void main(in VSInput  VSIn,
          out PSInput PSIn)
{
    uint2 Dim;
    g_TerrainHeightMap.GetDimensions(Dim.x, Dim.y);
    int3 TexCoord = int3(int2(Dim * VSIn.UV + 0.5), 0);

    PSIn.Pos.xz = VSIn.UV;
    PSIn.Pos.y  = g_TerrainHeightMap.Load(TexCoord).x;
    PSIn.Pos.w  = 1.0;
    PSIn.Pos.xyz *= g_TerrainConstants.Scale;
    PSIn.Pos = mul(PSIn.Pos, g_Constants.ModelViewProj);

    PSIn.UV = VSIn.UV;
}
