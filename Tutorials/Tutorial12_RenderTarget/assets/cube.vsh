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
    float3 Pos[8];
    Pos[0] = float3(-1.0, -1.0, -1.0);
    Pos[1] = float3(-1.0, +1.0, -1.0);
    Pos[2] = float3(+1.0, +1.0, -1.0);
    Pos[3] = float3(+1.0, -1.0, -1.0);
    Pos[4] = float3(-1.0, -1.0, +1.0);
    Pos[5] = float3(-1.0, +1.0, +1.0);
    Pos[6] = float3(+1.0, +1.0, +1.0);
    Pos[7] = float3(+1.0, -1.0, +1.0);

    float3 Color[8];
    Color[0] = float3(0.0, 0.0, 0.0);
    Color[1] = float3(0.0, 1.0, 0.0);
    Color[2] = float3(1.0, 1.0, 0.0);
    Color[3] = float3(1.0, 0.0, 0.0);
    Color[4] = float3(0.0, 0.0, 1.0);
    Color[5] = float3(0.0, 1.0, 1.0);
    Color[6] = float3(1.0, 1.0, 1.0);
    Color[7] = float3(1.0, 0.0, 1.0);

    // OpenGLES does not allow array initialization of the
    // form float Indices[] = {1,2, ... }
    int Indices[36];
    Indices[0]  = 2;  Indices[1]  = 0;  Indices[2]  = 1;
    Indices[3]  = 2;  Indices[4]  = 3;  Indices[5]  = 0;
    Indices[6]  = 4;  Indices[7]  = 6;  Indices[8]  = 5;
    Indices[9]  = 4;  Indices[10] = 7;  Indices[11] = 6;
    Indices[12] = 0;  Indices[13] = 7;  Indices[14] = 4;
    Indices[15] = 0;  Indices[16] = 3;  Indices[17] = 7;
    Indices[18] = 1;  Indices[19] = 0;  Indices[20] = 4;
    Indices[21] = 1;  Indices[22] = 4;  Indices[23] = 5;
    Indices[24] = 1;  Indices[25] = 5;  Indices[26] = 2;
    Indices[27] = 5;  Indices[28] = 6;  Indices[29] = 2;
    Indices[30] = 3;  Indices[31] = 6;  Indices[32] = 7;
    Indices[33] = 3;  Indices[34] = 2;  Indices[35] = 6;

    int VertexID = Indices[VSIn.VertexID];
    PSIn.Pos = mul(float4(Pos[VertexID], 1.0), g_WorldViewProj);
    PSIn.Color = float4(Color[VertexID], 1.0);
}
