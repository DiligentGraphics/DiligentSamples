cbuffer Constants
{
    float4x4 g_ViewProj;
    float4x4 g_Rotation;
};

cbuffer InstanceData
{
    float4x4 g_InstanceMatr;
};

struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float2 uv : TEX_COORD; 
};

// Vertex shader takes two inputs: vertex position and uv coordinates.
// By convention, Diligent Engine expects vertex shader inputs to be labeled as ATTRIBn, where n is the attribute number.
// Note that if separate shader objects are not supported (this is only the case for old GLES3.0 devices), vertex
// shader output variable name must match exactly the name of the pixel shader input variable.
// If the variable has structure type (like in this example), the structure declarations must also be indentical.
void main(float3 pos : ATTRIB0, 
          float2 uv  : ATTRIB1,
          out PSInput PSIn) 
{
    // Apply rotation
    float4 TransformedPos = mul( float4(pos,1.0),g_Rotation);
    // Apply instance-specific transformation
    TransformedPos = mul(TransformedPos, g_InstanceMatr);
    // Apply view-projection matrix
    PSIn.Pos = mul( TransformedPos, g_ViewProj);
    PSIn.uv = uv;
}
