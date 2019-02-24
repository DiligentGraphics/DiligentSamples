cbuffer Constants
{
    float4x4 g_WorldViewProj;
};

struct VSInput
{
    uint VertexID : SV_VertexID;
};

struct PSInput
{
    float4 Pos    : SV_POSITION;
    float4 Color  : COLOR0;
};

// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be indentical.
void main(in  VSInput VSIn,
          out PSInput PSIn)
{
    float3 Pos[] =
    {
        float3(-1, -1, -1),
        float3(-1, +1, -1),
        float3(+1, +1, -1),
        float3(+1, -1, -1),
        float3(-1, -1, +1),
        float3(-1, +1, +1),
        float3(+1, +1, +1),
        float3(+1, -1, +1)
    };

    float3 Color[] =
    {
        float3(0, 0, 0),
        float3(0, 1, 0),
        float3(1, 1, 0),
        float3(1, 0, 0),
        float3(0, 0, 1),
        float3(0, 1, 1),
        float3(1, 1, 1),
        float3(1, 0, 1)
    };

    uint Indices[] =
    {
        2,0,1, 2,3,0,
        4,6,5, 4,7,6,
        0,7,4, 0,3,7,
        1,0,4, 1,4,5,
        1,5,2, 5,6,2,
        3,6,7, 3,2,6
    };

    uint VertexID = Indices[VSIn.VertexID];
    PSIn.Pos = mul(float4(Pos[VertexID], 1.0), g_WorldViewProj);
    PSIn.Color = float4(Color[VertexID], 1.0f);
}
