
struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float2 uv : TEX_COORD;
    float TexIndex : TEX_ARRAY_INDEX;
};

void main(in float2 PolygonXY            : ATTRIB0,
          in float4 QuadRotationAndScale : ATTRIB1,
          in float2 QuadCenter           : ATTRIB2,
          in float TexArrInd             : ATTRIB3,
          out PSInput PSIn)
{
    float2 pos = PolygonXY.xy;
    float2x2 mat = MatrixFromRows(QuadRotationAndScale.xy, QuadRotationAndScale.zw);
    pos = mul(pos, mat);
    pos += QuadCenter.xy;
    PSIn.Pos = float4(pos, 0.0, 1.0);
    const float sqrt2 = 1.414213562373095;
    PSIn.uv = PolygonXY * sqrt2 * 0.5 + 0.5;
    PSIn.TexIndex = TexArrInd;
}
