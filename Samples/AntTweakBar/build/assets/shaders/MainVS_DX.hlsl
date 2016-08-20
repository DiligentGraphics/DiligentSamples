
cbuffer Constants
{
    float4x4 g_WorldViewProj;
	float4x4 g_WorldNorm;
    float3 g_LightDir;
    float g_LightCoeff;
};

struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float4 Color : COLOR0; 
};

PSInput main(float3 pos : ATTRIB0, float3 norm : ATTRIB1, float4 color : ATTRIB2) 
{
    PSInput ps; 
    ps.Pos = mul( float4(pos,1), g_WorldViewProj);
    float3 n = normalize(mul(float4(norm, 0), g_WorldNorm).xyz);
    ps.Color.rgb = color.rgb * ((1 - g_LightCoeff) + g_LightCoeff * dot(n, -g_LightDir));
    ps.Color.a  = color.a;
    return ps;
}
