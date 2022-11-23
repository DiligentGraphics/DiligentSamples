#ifndef _SCENE_FXH_
#define _SCENE_FXH_

#include "structures.fxh"

#define PI      3.1415927
#define INF     1e+30
#define EPSILON 5e-2

float3 ClipToWorld(float4 ClipPos, float4x4 ViewProjInvMat)
{
#if defined(DESKTOP_GL) || defined(GL_ES)
    ClipPos.y *= -1.0;
#endif
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

#define HIT_TYPE_NONE          0
#define HIT_TYPE_LAMBERTIAN    1
#define HIT_TYPE_DIFFUSE_LIGHT 2

struct HitInfo
{
    float3 Albedo;
    float3 Emittance;
    float3 Normal;
    float  Distance;
    int    Type;
};

HitInfo NullHit()
{
    HitInfo Hit;
    Hit.Albedo    = float3(0.0, 0.0, 0.0);
    Hit.Normal    = float3(0.0, 0.0, 0.0);
    Hit.Emittance = float3(0.0, 0.0, 0.0);
    Hit.Distance  = INF;
    Hit.Type      = HIT_TYPE_NONE;
    return Hit;
}

struct BoxInfo
{
    float3 Center;
    float3 Size;
    float3 Albedo;
    float3 Emittance;
    int    Type;
};

bool IntersectAABB(in    RayInfo Ray,
                   in    BoxInfo Box,
                   inout HitInfo Hit)
{
    float3 t1     = (Box.Center - Box.Size - Ray.Origin) / Ray.Dir;
    float3 t2     = (Box.Center + Box.Size - Ray.Origin) / Ray.Dir;
    float3 t_min  = min(t1, t2);
    float3 t_max  = max(t1, t2);
    float  t_near = max(max(t_min.x, t_min.y), t_min.z);
    float  t_far  = min(min(t_max.x, t_max.y), t_max.z);
    if (t_near >= t_far)
        return false;

    // Use far intersection if t_near > EPSILON.
    // This is important to avoid light leaks.
    float t = t_near > EPSILON ? t_near : t_far;
    if (t < EPSILON || t_near > Hit.Distance)
        return false;

    Hit.Albedo    = Box.Albedo;
    Hit.Emittance = Box.Emittance;
    // Compute normal for the entry point. We only use internal intersection for shadows.
    Hit.Normal   = -sign(Ray.Dir) * step(t_min.yzx, t_min.xyz) * step(t_min.zxy, t_min.xyz);
    Hit.Distance = t;
    Hit.Type     = Box.Type;

    return true;
}

bool IntersectRotatedAABB(in    RayInfo Ray,
                          in    BoxInfo Box,
                          in    float   RotationY,
                          inout HitInfo Hit)
{
    RayInfo RotRay = RotateRayY(Ray, -RotationY);
    if (IntersectAABB(RotRay, Box, Hit))
    {
        Hit.Normal = RotateY(Hit.Normal, RotationY);
        return true;
    }

    return false;
}


void IntersectWalls(RayInfo Ray, inout HitInfo Hit)
{
    float RoomSize  = 10.0;
    float WallThick = 0.05;

    BoxInfo Box;
    Box.Type      = HIT_TYPE_LAMBERTIAN;
    Box.Emittance = float3(0.0, 0.0, 0.0);

    float3 Green = float3(0.1, 0.6, 0.1);
    float3 Red   = float3(0.6, 0.1, 0.1);
    float3 Grey  = float3(0.5, 0.5, 0.5);

    // Right wall
    Box.Center = float3(RoomSize * 0.5 + WallThick * 0.5, 0.0, 0.0);
    Box.Size   = float3(WallThick, RoomSize * 0.5, RoomSize * 0.5);
    Box.Albedo = Green;
    IntersectAABB(Ray, Box, Hit);

    // Left wall
    Box.Center = float3(-RoomSize * 0.5 - WallThick * 0.5, 0.0, 0.0);
    Box.Size   = float3(WallThick, RoomSize * 0.5, RoomSize * 0.5);
    Box.Albedo = Red;
    IntersectAABB(Ray, Box, Hit);

    // Ceiling
    Box.Center = float3(0.0, +RoomSize * 0.5 + WallThick * 0.5, 0.0);
    Box.Size   = float3(RoomSize * 0.5, WallThick, RoomSize * 0.5);
    Box.Albedo = Grey;
    IntersectAABB(Ray, Box, Hit);

    // Floor
    Box.Center = float3(0.0, -RoomSize * 0.5 + WallThick * 0.5, 0.0);
    Box.Size   = float3(RoomSize * 0.5, WallThick, RoomSize * 0.5);
    Box.Albedo = Grey;
    IntersectAABB(Ray, Box, Hit);

    // Back wall
    Box.Center = float3(0.0, 0.0, +RoomSize * 0.5 + WallThick * 0.5);
    Box.Size   = float3(RoomSize * 0.5, RoomSize * 0.5, WallThick);
    Box.Albedo = Grey;
    IntersectAABB(Ray, Box, Hit);
}

void IntersectSceneInterior(RayInfo Ray, inout HitInfo Hit)
{
    BoxInfo Box;
    Box.Type      = HIT_TYPE_LAMBERTIAN;
    Box.Emittance = float3(0.0, 0.0, 0.0);

    float Box1Rotation = +PI * 0.1;
    float Box2Rotation = -PI * 0.1;
    // In OpenGL, clip space is flipped vertically. If we rotate objects in the vertex shader,
    // this does not matter as the axis is flipped by the rasterizer.
    // We, however, work in the pixel shader, so we have to negate the
    // rotation directions to compensate for the coordinate system change.
#if defined(DESKTOP_GL) || defined(GL_ES)
    Box1Rotation *= -1.0;
    Box2Rotation *= -1.0;
#endif

    // Tall box
    Box.Center = float3(-2.0, -2.0,  1.5);
    Box.Size   = float3(1.3, 3.0, 1.3);
    Box.Albedo = float3(0.6, 0.6, 0.6);
    IntersectRotatedAABB(Ray, Box, Box1Rotation, Hit);

    // Small box
    Box.Center = float3(+2.5, -3.5, -1.0);
    Box.Size   = float3(1.5, 1.5, 1.5);
    Box.Albedo = float3(0.6, 0.6, 0.6);
    IntersectRotatedAABB(Ray, Box, Box2Rotation, Hit);
}

BoxInfo GetLight(LightAttribs Light)
{
    BoxInfo Box;
    Box.Type      = HIT_TYPE_DIFFUSE_LIGHT;
    Box.Center    = float3(Light.f2PosXZ.x,  4.90, Light.f2PosXZ.y);
    Box.Size      = float3(Light.f2SizeXZ.x, 0.02, Light.f2SizeXZ.y);
    Box.Albedo    = float3(0.75, 0.75, 0.75);
    Box.Emittance = Light.f4Intensity.rgb * Light.f4Intensity.a;
    return Box;
}

void IntersectLight(RayInfo Ray, LightAttribs Light, inout HitInfo Hit)
{
    BoxInfo Box = GetLight(Light);
    if (IntersectAABB(Ray, Box, Hit))
    {
        // Check that the ray hit the light from the emissive side
        if (dot(Hit.Normal, Light.f4Normal.xyz) < 0.99)
        {
            Hit.Emittance = float3(0.0, 0.0, 0.0);
        }
    }
}

HitInfo IntersectScene(RayInfo Ray, LightAttribs Light)
{
    HitInfo Hit = NullHit();

    IntersectSceneInterior(Ray, Hit);
    IntersectWalls(Ray, Hit);
    IntersectLight(Ray, Light, Hit);

    return Hit;
}

float TestShadow(RayInfo Ray)
{
    if (Ray.Dir.y <= 0)
        return 0.0;

    HitInfo Hit = NullHit();

    IntersectSceneInterior(Ray, Hit);

    return Hit.Distance < INF ? 0.0 : 1.0;
}

float3 GetLightSamplePos(LightAttribs Light, float2 uv)
{
    BoxInfo Box = GetLight(Light);
    float3 Corner0 = Box.Center - Box.Size;
    float3 Corner1 = Box.Center + Box.Size;
    float3 Sample;
    Sample.xz = lerp(Corner0.xz, Corner1.xz, uv);
    Sample.y  = Corner0.y;
    return Sample;
}

#endif // _SCENE_FXH_
