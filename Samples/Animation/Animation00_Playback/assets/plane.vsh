cbuffer Constants
{
    float4x4 g_WorldViewProj;
}

struct PlanePSInput
{
    float4 Pos : SV_POSITION;
    float2 TexCoord : TEXCOORD;
};

void main(in  uint    VertId : SV_VertexID,
          out PlanePSInput PSIn)
{
    float PlaneExtent = 5.0;
    float PlanePos    = 0.0;
    
    float4 Pos[4];
    Pos[0] = float4(-PlaneExtent, PlanePos, -PlaneExtent, 1.0);
    Pos[1] = float4(-PlaneExtent, PlanePos, +PlaneExtent, 1.0);
    Pos[2] = float4(+PlaneExtent, PlanePos, -PlaneExtent, 1.0);
    Pos[3] = float4(+PlaneExtent, PlanePos, +PlaneExtent, 1.0);
    
    float2 Uv[4];
    Uv[0] = float2(0.0, 0.0);
    Uv[1] = float2(0.0, 1.0);
    Uv[2] = float2(1.0, 0.0);
    Uv[3] = float2(1.0, 1.0);

    PSIn.Pos = mul(Pos[VertId], g_WorldViewProj);
    PSIn.TexCoord = Uv[VertId];
}
