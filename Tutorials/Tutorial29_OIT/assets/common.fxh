struct Constants
{
    float4x4 ViewProj;
    float4   LightDir;
};

#ifndef __cplusplus
struct PSInput 
{ 
    float4 Pos    : SV_POSITION; 
    float3 Normal : TEX_COORD; 
    float4 Color  : COLOR;
};

float3 ComputeLighting(float3 Color, float3 Normal, float3 LightDir)
{
    float3 LightColor   = float3(0.75, 0.75, 0.75);
    float3 AmbientColor = float3(0.25, 0.25, 0.25);
    float DiffuseIntensity = max(0, dot(Normal, -LightDir));
    return Color * (AmbientColor + LightColor * DiffuseIntensity);
}
#endif
