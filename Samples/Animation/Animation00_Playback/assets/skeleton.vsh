cbuffer Constants
{
    float4x4 g_WorldViewProj;
};

// Vertex shader takes two inputs: vertex position and color.
// By convention, Diligent Engine expects vertex shader inputs to be 
// labeled 'ATTRIBn', where n is the attribute number.
struct VSInput
{
    // Vertex attributes
    float3 Pos   :  ATTRIB0;
    float3 Normal : ATTRIB1;
    float4 Color :  ATTRIB2;
    
    // Instance attributes
    float4 MtrxRow0 : ATTRIB3;
    float4 MtrxRow1 : ATTRIB4;
    float4 MtrxRow2 : ATTRIB5;
    float4 MtrxRow3 : ATTRIB6;
};

struct PSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float3 Normal : NORMAL; 
    float4 Color : COLOR0; 
};

// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be indentical.
void main(in  VSInput VSIn,
          out PSInput PSIn) 
{
    // HLSL matrices are row-major while GLSL matrices are column-major. We will
    // use convenience function MatrixFromRows() appropriately defined by the engine
    float4x4 InstanceMatr = MatrixFromRows(VSIn.MtrxRow0, VSIn.MtrxRow1, VSIn.MtrxRow2, VSIn.MtrxRow3);
    // Apply instance-specific transformation
    float4 TransformedPos = mul(float4(VSIn.Pos, 1.0), InstanceMatr);
    // Apply view-projection matrix
    PSIn.Pos = mul(TransformedPos, g_WorldViewProj);

    float3x3 CrossMatr = float3x3(
        cross(InstanceMatr[1].xyz, InstanceMatr[2].xyz),
        cross(InstanceMatr[2].xyz, InstanceMatr[0].xyz),
        cross(InstanceMatr[0].xyz, InstanceMatr[1].xyz)
    );
    float    Invdet     = 1.0 / dot(CrossMatr[2], InstanceMatr[2].xyz);
    float3x3 NormalMatr = CrossMatr * Invdet;
    PSIn.Normal         = mul(VSIn.Normal, NormalMatr);
    PSIn.Color = VSIn.Color;
}
