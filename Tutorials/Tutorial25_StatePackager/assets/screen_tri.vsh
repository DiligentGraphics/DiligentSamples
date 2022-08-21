struct PSInput 
{ 
    float4 Pos    : SV_POSITION;
    float2 ClipXY : ClipPos;
};

// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be identical.
void main(in  uint    VertID : SV_VertexID,
          out PSInput PSIn) 
{
    float2 ClipXY[3];
    ClipXY[0] = float2(-1.0, -1.0);
    ClipXY[1] = float2(-1.0,  3.0);
    ClipXY[2] = float2( 3.0, -1.0);

    PSIn.Pos    = float4(ClipXY[VertID], 0.0, 1.0);
    PSIn.ClipXY = ClipXY[VertID];
}
