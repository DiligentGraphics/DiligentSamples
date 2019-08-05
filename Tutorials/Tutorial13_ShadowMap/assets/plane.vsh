#include "structures.fxh"

cbuffer Constants
{
    float4x4 g_ViewProj;
    float4x4 g_WorldToLightUVDepth;
    float4   g_LightDirection;
};

void main(in  uint    VertId : SV_VertexID,
          out PlanePSInput PSIn)
{
    float PlaneExtent = 10.0;
    float PlaneZ      = -2.0;
    
    float4 Pos[4];
    Pos[0] = float4(-PlaneExtent, -PlaneExtent, PlaneZ, 1.0);
    Pos[1] = float4(-PlaneExtent, +PlaneExtent, PlaneZ, 1.0);
    Pos[2] = float4(+PlaneExtent, -PlaneExtent, PlaneZ, 1.0);
    Pos[3] = float4(+PlaneExtent, +PlaneExtent, PlaneZ, 1.0);
    
    PSIn.Pos          = mul(Pos[VertId], g_ViewProj);
    PSIn.ShadowMapPos = mul(Pos[VertId], g_WorldToLightUVDepth);
    PSIn.NdotL        = dot(float3(0.0, 0.0, 1.0), -g_LightDirection.xyz);
}
