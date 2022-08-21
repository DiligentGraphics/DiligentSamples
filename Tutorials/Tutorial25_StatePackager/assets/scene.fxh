#ifndef _SCENE_FXH_
#define _SCENE_FXH_

float3 ClipToWorld(float4 ClipPos, float4x4 ViewProjInvMat)
{
    // Reconstruct world position by applying inverse view-projection matrix
    float4 WorldPos = mul(ClipPos, ViewProjInvMat);
    return WorldPos.xyz / WorldPos.w;
}

float3 ScreenToWorld(float2 PixelXY, float Depth, float2 ScreenSize, float4x4 ViewProjInvMat)
{
    // Get clip-space position
    float4 ClipPos;
    ClipPos.xy = PixelXY / ScreenSize * float2(2.0, -2.0) + float2(-1.0, 1.0);
    ClipPos.z  = Depth;
    ClipPos.w  = 1.0;
    return ClipToWorld(ClipPos, ViewProjInvMat);
}

struct RayInfo
{
    float3 Origin;
    float3 Dir;
};

struct HitInfo
{
    float3 Color;
    float3 Normal;
    float  Distance;
};

void IntersectPlane(in    RayInfo Ray, 
                    in    float3  PlaneNormal, 
                    in    float   PlaneDistance,
                    inout HitInfo Hit)
{

}

#endif // _SCENE_FXH_
