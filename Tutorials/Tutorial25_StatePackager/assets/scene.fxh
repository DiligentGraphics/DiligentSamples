#ifndef _SCENE_FXH_
#define _SCENE_FXH_

#define PI  3.1415927
#define INF 1e+30

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


float3 RotateY(float3 V, float a) {
    float s, c;
    sincos(a, s, c);
    float2x2 mat;
    mat[0] = float2( c, s);
    mat[1] = float2(-s, c);
    float2 xz = mul(V.xz, mat);
    return float3(xz.x, V.y, xz.y);
}

struct RayInfo
{
    float3 Origin;
    float3 Dir;
};

RayInfo RotateRayY(RayInfo Ray, float a)
{
    RayInfo RotatedRay;
    RotatedRay.Origin = RotateY(Ray.Origin, a);
    RotatedRay.Dir    = RotateY(Ray.Dir,    a);
    return RotatedRay;
}

struct HitInfo
{
    float3 Albedo;
    float3 Normal;
    float  Distance;
};

bool IntersectAABB(in    RayInfo Ray,
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

    if (t_near >= t_far || t_near < 0.0 || t_near > Hit.Distance)
        return false;

    Hit.Albedo   = BoxAlbedo;
    Hit.Normal   = -sign(Ray.Dir) * step(t_min.yzx, t_min.xyz) * step(t_min.zxy, t_min.xyz);
    Hit.Distance = t_near;
    return true;
}

bool IntersectRotatedAABB(in    RayInfo Ray,
                          in    float3  BoxCenter,
                          in    float3  BoxSize,
                          in    float3  BoxAlbedo,
                          in    float   RotationY,
                          inout HitInfo Hit)
{
    RayInfo RotRay = RotateRayY(Ray, -RotationY);
    if (IntersectAABB(RotRay, BoxCenter, BoxSize, BoxAlbedo, Hit))
    {
        Hit.Normal = RotateY(Hit.Normal, RotationY);
        return true;
    }

    return false;
}


void IntersectWalls(RayInfo Ray, inout HitInfo Hit)
{
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
}

void IntersectSceneInterior(RayInfo Ray, inout HitInfo Hit)
{
    // Tall box
    IntersectRotatedAABB(Ray, float3(-2.0, -2.0,  1.5), float3(1.3, 3.0, 1.3), float3(0.6, 0.6, 0.6), +PI * 0.1, Hit);
    // Small box
    IntersectRotatedAABB(Ray, float3(+2.5, -3.5, -1.0), float3(1.5, 1.5, 1.5), float3(0.6, 0.6, 0.6), -PI * 0.1, Hit);
}

HitInfo IntersectScene(RayInfo Ray)
{
    HitInfo Hit;
    Hit.Albedo   = float3(0.0, 0.0, 0.0);
    Hit.Normal   = float3(0.0, 0.0, 0.0);
    Hit.Distance = INF;

    IntersectSceneInterior(Ray, Hit);
    IntersectWalls(Ray, Hit);

    return Hit;
}

float TestShadow(RayInfo Ray)
{
    HitInfo Hit;
    Hit.Albedo   = float3(0.0, 0.0, 0.0);
    Hit.Normal   = float3(0.0, 0.0, 0.0);
    Hit.Distance = INF;

    IntersectSceneInterior(Ray, Hit);

    return Hit.Distance < INF ? 0.0 : 1.0;
}

#endif // _SCENE_FXH_
