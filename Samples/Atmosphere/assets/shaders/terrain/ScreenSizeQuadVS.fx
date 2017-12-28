
#include "TerrainShadersCommon.fxh"

void GenerateScreenSizeQuadVS(in uint VertexId : SV_VertexID,
                              out float4 f4Pos : SV_Position)
{
    float4 MinMaxUV = float4(-1.0, -1.0, 1.0, 1.0);
    
    float2 Verts[4] = 
    {
        MinMaxUV.xy, 
        MinMaxUV.xw,
        MinMaxUV.zy,
        MinMaxUV.zw
    };

    f4Pos = float4(Verts[VertexId], 1.0, 1.0);
}
