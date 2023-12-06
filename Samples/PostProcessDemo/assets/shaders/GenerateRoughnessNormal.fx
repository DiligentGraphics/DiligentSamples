#include "BasicStructures.fxh"

cbuffer cbCameraAttribs
{
    CameraAttribs g_CameraAttibs;
}

Texture2D<float> g_InputDepth;

struct PSOutput
{
    float4 MaterialParameters : SV_Target0;
    float3 Normal             : SV_Target1;
};

float2 ScreenSpaceToNDC(float2 Location, float2 InvDimension)
{
    float2 NDC = 2.0f * Location.xy * InvDimension - 1.0f;
    return float2(NDC.x, -NDC.y);
}

float3 GetPositionWS(float2 Location, float Depth)
{
    float2 NDC = ScreenSpaceToNDC(Location, g_CameraAttibs.f4ViewportSize.zw);
    float4 Position = mul(float4(NDC, Depth, 1.0), g_CameraAttibs.mViewProjInv);
    return Position.xyz / Position.w;
}

PSOutput GenerateRoughnessNormalPS(in float4 Position : SV_Position)
{
    float3 WorldPosition = GetPositionWS(Position.xy, g_InputDepth.Load(int3(Position.xy, 0)));
    float3 DepthNormal = normalize(cross(ddx(WorldPosition), ddy(WorldPosition)));

    PSOutput Output;
    Output.Normal = DepthNormal; 
    Output.MaterialParameters = float4(g_CameraAttibs.f4ExtraData[0].x, 0.0, 0.0, 0.0);
    return Output;
}
