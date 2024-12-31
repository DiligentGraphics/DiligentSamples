
void main(in  uint VertexID : SV_VertexID,
          out float4 Pos : SV_Position)
{
    float4 TriVerts[3];
    TriVerts[0] = float4(-1.0, -1.0, 0.0, 1.0);
    TriVerts[1] = float4(-1.0, +3.0, 0.0, 1.0);
    TriVerts[2] = float4(+3.0, -1.0, 0.0, 1.0);
    Pos = TriVerts[VertexID];
}
