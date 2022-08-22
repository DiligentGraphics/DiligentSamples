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
    float3 Albedo;
    float3 Normal;
    float  Distance;
};

void IntersectAABB(in    RayInfo Ray,
                   in    float3  BoxCenter,
                   in    float3  BoxSize,
                   in    float3  BoxAlbedo,
                   inout HitInfo Hit)
{
    float3 t1     = (BoxCenter - BoxSize - Ray.Origin) / Ray.Dir;
    float3 t2     = (BoxCenter + BoxSize - Ray.Origin) / Ray.Dir;
    float3 t_min  = min(t1, t2);
    float3 t_max  = max(t1, t2);
    float  t_near = max(max(t_min.x, t_min.y), t_min.z);
    float  t_far  = min(min(t_max.x, t_max.y), t_max.z);

    if (t_near < t_far && t_near > 0.0 && t_near < Hit.Distance)
    {
        Hit.Albedo   = BoxAlbedo;
        Hit.Normal   = -sign(Ray.Dir) * step(t_min.yzx, t_min.xyz) * step(t_min.zxy, t_min.xyz);
        Hit.Distance = t_near;
    }
}

HitInfo IntersectScene(RayInfo Ray)
{
    HitInfo Hit;
    Hit.Albedo   = float3(0.0, 0.0, 0.0);
    Hit.Normal   = float3(0.0, 0.0, 0.0);
    Hit.Distance =  1e+30;

    float RoomSize = 10;
    float WallThick = 0.05;
    // Right wall
    IntersectAABB(Ray, float3(+RoomSize * 0.5 + WallThick * 0.5, 0.0, 0.0), float3(WallThick, RoomSize * 0.5, RoomSize * 0.5), float3(0.1, 0.6, 0.1), Hit);
    // Left wall
    IntersectAABB(Ray, float3(-RoomSize * 0.5 - WallThick * 0.5, 0.0, 0.0), float3(WallThick, RoomSize * 0.5, RoomSize * 0.5), float3(0.6, 0.1, 0.1), Hit);
    // Top wall
    IntersectAABB(Ray, float3(0.0, +RoomSize * 0.5 + WallThick * 0.5, 0.0), float3(RoomSize * 0.5, WallThick, RoomSize * 0.5), float3(0.5, 0.5, 0.5), Hit);
    // Bottom wall
    IntersectAABB(Ray, float3(0.0, -RoomSize * 0.5 + WallThick * 0.5, 0.0), float3(RoomSize * 0.5, WallThick, RoomSize * 0.5), float3(0.5, 0.5, 0.5), Hit);
    // Back wall
    IntersectAABB(Ray, float3(0.0, 0.0, +RoomSize * 0.5 + WallThick * 0.5), float3(RoomSize * 0.5, RoomSize * 0.5, WallThick), float3(0.5, 0.5, 0.5), Hit);

    // Tall box
    IntersectAABB(Ray,  float3(-2.0, -2.0,  1.0), float3(1.5, 3.0, 1.5), float3(0.6, 0.6, 0.6), Hit);
    // Small box
    IntersectAABB(Ray,  float3(+1.5, -3.5, -2.5), float3(1.5, 1.5, 1.5), float3(0.6, 0.6, 0.6), Hit);

    return Hit;
}

#endif // _SCENE_FXH_
