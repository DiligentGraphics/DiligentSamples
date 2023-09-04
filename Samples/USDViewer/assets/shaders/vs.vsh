cbuffer Constants
{
    float4x4 g_WorldViewProj;
};

struct VSInput
{
    float3 Pos    : ATTRIB0;
    float3 Normal : ATTRIB1;
};

struct PSInput
{
    float4 Pos   : SV_POSITION;
    float4 Color : COLOR0;
    float3 Normal: NORMAL;
};

void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    PSIn.Pos    = mul( float4(VSIn.Pos,1.0), g_WorldViewProj);
    PSIn.Color  = float4(1.0, 1.0, 1.0, 1.0);
    PSIn.Normal = VSIn.Normal;
}
