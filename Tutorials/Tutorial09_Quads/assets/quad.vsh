
cbuffer QuadAttribs
{
    float4 g_QuadRotationAndScale; // float2x2 doesn't work
    float4 g_QuadCenter;
};

struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float2 uv : TEX_COORD;
};

void main(in uint VertID : SV_VertexID,
          out PSInput PSIn) 
{
    float4 pos_uv[4];
    pos_uv[0] = float4(-1.0,+1.0, 0.0,0.0);
    pos_uv[1] = float4(-1.0,-1.0, 0.0,1.0);
    pos_uv[2] = float4(+1.0,+1.0, 1.0,0.0);
    pos_uv[3] = float4(+1.0,-1.0, 1.0,1.0);

    float2 pos = pos_uv[VertID].xy;
    float2x2 mat = MatrixFromRows(g_QuadRotationAndScale.xy, g_QuadRotationAndScale.zw);
    pos = mul(pos, mat);
    pos += g_QuadCenter.xy;
    PSIn.Pos = float4(pos, 0.0, 1.0);
    PSIn.uv = pos_uv[VertID].zw;
}
