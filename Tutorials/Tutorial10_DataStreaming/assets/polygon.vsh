
cbuffer PolygonAttribs
{
    float4 g_QuadRotationAndScale; // float2x2 doesn't work
    float4 g_QuadCenter;
};

struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float2 uv : TEX_COORD;
};

PSInput main(in float2 PolygonXY : ATTRIB0) 
{
    float2 pos = PolygonXY.xy;
    float2x2 mat = MatrixFromRows(g_QuadRotationAndScale.xy, g_QuadRotationAndScale.zw);
    pos = mul(pos, mat);
    pos += g_QuadCenter.xy;
    PSInput ps;
    ps.Pos = float4(pos, 0.0, 1.0);
    const float sqrt2 = 1.414213562373095;
    ps.uv = PolygonXY * sqrt2 * 0.5 + 0.5;
    return ps;
}
