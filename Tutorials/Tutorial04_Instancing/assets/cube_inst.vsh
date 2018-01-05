cbuffer Constants
{
    float4x4 g_ViewProj;
    float4x4 g_Rotation;
};

struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float2 uv : TEX_COORD; 
};

// Vertex shader takes two inputs: vertex position and color.
// By convention, Diligent Engine expects vertex shader inputs to 
// be labeled as ATTRIBn, where n is the attribute number
PSInput main(float3 pos : ATTRIB0, 
             float2 uv : ATTRIB1,
             // Instance-specific attribute are updated once per instance
             float4 matr_row0 : ATTRIB2,
             float4 matr_row1 : ATTRIB3,
             float4 matr_row2 : ATTRIB4,
             float4 matr_row3 : ATTRIB5) 
{
    PSInput ps; 
    // HLSL matrices are row-major while GLSL matrices are column-major. We will
    // use convenience function MatrixFromRows() appropriately defined by the engine
    float4x4 InstanceMatr = MatrixFromRows(matr_row0, matr_row1, matr_row2, matr_row3);
    // Apply rotation
    float4 TransformedPos = mul( float4(pos,1.0),g_Rotation);
    // Apply instance-specific transformation
    TransformedPos = mul(TransformedPos, InstanceMatr);
    // Apply view-projection matrix
    ps.Pos = mul( TransformedPos, g_ViewProj);
    ps.uv = uv;
    return ps;
}
