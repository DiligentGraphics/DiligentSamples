#ifndef _SCENE_FXH_
#define _SCENE_FXH_

#include "structures.fxh"

#define PI      3.1415927
#define INF     1e+30
#define EPSILON 1e-2

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

struct HitInfo
{
    Material Mat;
    float3   Normal;
    float    Distance;
};


Material NullMaterial()
{
    Material Mat;
    Mat.BaseColor = float4(0.0, 0.0, 0.0, 0.0);
    Mat.Emittance = float4(0.0, 0.0, 0.0, 0.0);
    Mat.Type      = MAT_TYPE_NONE;
    Mat.Metallic  = 0.0;
    Mat.Roughness = 0.0;
    Mat.IOR       = 0.0;
    return Mat;
}

HitInfo NullHit()
{
    HitInfo Hit;
    Hit.Mat      = NullMaterial();
    Hit.Normal   = float3(0.0, 0.0, 0.0);
    Hit.Distance = INF;
    return Hit;
}

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

    Hit.Mat = Box.Mat;
    // Compute normal for the entry point. We only use internal intersection for shadows.
    Hit.Normal   = -sign(Ray.Dir) * step(t_min.yzx, t_min.xyz) * step(t_min.zxy, t_min.xyz);
    Hit.Distance = t;

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

bool IntersectSphere(in    RayInfo    Ray,
                     in    SphereInfo Sphere,
                     inout HitInfo    Hit)
{
    // http://wiki.cgsociety.org/index.php/Ray_Sphere_Intersection
    float3 Orig = Ray.Origin - Sphere.Center;
    float A = dot(Ray.Dir, Ray.Dir);
    float B = 2.0 * dot(Orig, Ray.Dir);
    float C = dot(Orig, Orig) - Sphere.Radius * Sphere.Radius;
    float D = B * B - 4.0 * A * C;
    // If discriminant is negative, there are no real roots hence the ray misses the
    // sphere
    if (D < 0.0)
        return false;

    D = sqrt(D);
    float t_near = (-B - D) / (2.0 * A);
    float t_far  = (-B + D) / (2.0 * A);
    
    float t = t_near > EPSILON ? t_near : t_far;
    if (t < EPSILON || t > Hit.Distance)
        return false;

    float3 HitPos = Ray.Origin + t * Ray.Dir;

    Hit.Mat      = Sphere.Mat;
    Hit.Normal   = normalize(HitPos - Sphere.Center);
    Hit.Distance = t;

    return true;
}

void IntersectWalls(RayInfo Ray, inout HitInfo Hit)
{
    float RoomSize  = 10.0;
    float WallThick = 0.05;

    BoxInfo Box;
    Box.Mat.Type      = MAT_TYPE_SMITH_GGX;
    Box.Mat.Emittance = float4(0.0, 0.0, 0.0, 0.0);
    Box.Mat.Metallic  = 0.0;
    Box.Mat.Roughness = 1.0;
    Box.Mat.IOR       = 1.0;

    float4 Green = float4(0.1, 0.6, 0.1, 0.0);
    float4 Red   = float4(0.6, 0.1, 0.1, 0.0);
    float4 Grey  = float4(0.5, 0.5, 0.5, 0.0);

    // Right wall
    Box.Center        = float3(RoomSize * 0.5 + WallThick * 0.5, 0.0, 0.0);
    Box.Size          = float3(WallThick, RoomSize * 0.5, RoomSize * 0.5);
    Box.Mat.BaseColor = Green;
    IntersectAABB(Ray, Box, Hit);

    // Left wall
    Box.Center        = float3(-RoomSize * 0.5 - WallThick * 0.5, 0.0, 0.0);
    Box.Size          = float3(WallThick, RoomSize * 0.5, RoomSize * 0.5);
    Box.Mat.BaseColor = Red;
    IntersectAABB(Ray, Box, Hit);

    // Ceiling
    Box.Center        = float3(0.0, +RoomSize * 0.5 + WallThick * 0.5, 0.0);
    Box.Size          = float3(RoomSize * 0.5, WallThick, RoomSize * 0.5);
    Box.Mat.BaseColor = Grey;
    IntersectAABB(Ray, Box, Hit);

    // Floor
    Box.Center        = float3(0.0, -RoomSize * 0.5 + WallThick * 0.5, 0.0);
    Box.Size          = float3(RoomSize * 0.5, WallThick, RoomSize * 0.5);
    Box.Mat.BaseColor = Grey;
    IntersectAABB(Ray, Box, Hit);

    // Back wall
    Box.Center        = float3(0.0, 0.0, +RoomSize * 0.5 + WallThick * 0.5);
    Box.Size          = float3(RoomSize * 0.5, RoomSize * 0.5, WallThick);
    Box.Mat.BaseColor = Grey;
    IntersectAABB(Ray, Box, Hit);
}

void IntersectSceneInterior(SceneAttribs Scene, RayInfo Ray, inout HitInfo Hit)
{
    IntersectSphere(Ray, Scene.Balls[0], Hit);
    IntersectSphere(Ray, Scene.Balls[1], Hit);
    IntersectSphere(Ray, Scene.Balls[2], Hit);
    IntersectSphere(Ray, Scene.Balls[3], Hit);
    IntersectSphere(Ray, Scene.Balls[4], Hit);
    IntersectSphere(Ray, Scene.Balls[5], Hit);
}

BoxInfo GetLight(LightAttribs Light)
{
    BoxInfo Box;
    Box.Center = float3(Light.f2PosXZ.x,  4.90, Light.f2PosXZ.y);
    Box.Size   = float3(Light.f2SizeXZ.x, 0.02, Light.f2SizeXZ.y);

    Box.Mat.Type      = MAT_TYPE_DIFFUSE_LIGHT;
    Box.Mat.BaseColor = float4(0.75, 0.75, 0.75, 0.0);
    Box.Mat.Emittance = float4(Light.f4Intensity.rgb * Light.f4Intensity.a, 0.0);
    Box.Mat.Metallic  = 0.0;
    Box.Mat.Roughness = 1.0;
    Box.Mat.IOR       = 1.5;
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
            Hit.Mat.Emittance = float4(0.0, 0.0, 0.0, 0.0);
        }
    }
}

HitInfo IntersectScene(RayInfo Ray, SceneAttribs Scene)
{
    HitInfo Hit = NullHit();

    IntersectSceneInterior(Scene, Ray, Hit);
    IntersectWalls(Ray, Hit);
    IntersectLight(Ray, Scene.Light, Hit);

    return Hit;
}

float TestShadow(SceneAttribs Scene, RayInfo Ray)
{
    if (Ray.Dir.y <= 0)
        return 0.0;

    HitInfo Hit = NullHit();

    IntersectSceneInterior(Scene, Ray, Hit);

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
