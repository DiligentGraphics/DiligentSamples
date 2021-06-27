struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float2 UV  : TEX_COORD; 
};

void main(in uint vid : SV_VertexID,
          out PSInput PSIn) 
{
    // fullscreen triangle
    PSIn.UV  = float2(vid >> 1, vid & 1) * 2.0;
    PSIn.Pos = float4(PSIn.UV * 2.0 - 1.0, 0.0, 1.0);
}
