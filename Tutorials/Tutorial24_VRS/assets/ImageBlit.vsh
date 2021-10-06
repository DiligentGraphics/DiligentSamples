struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float2 UV  : TEX_COORD; 
};

void VSmain(in uint vid : SV_VertexID,
            out PSInput PSIn) 
{
    // fullscreen triangle
    float2 uv = float2(vid >> 1, vid & 1) * 2.0;
    PSIn.UV   = float2(uv.x, 1.0 - uv.y); 
    PSIn.Pos  = float4(uv * 2.0 - 1.0, 0.0, 1.0);
}
