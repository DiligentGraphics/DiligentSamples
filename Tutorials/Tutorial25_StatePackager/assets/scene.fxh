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

// Plane: dot(P, PlaneNormal) = PlaneDistance
void IntersectPlane(in    RayInfo Ray,
                    in    float3  PlaneNormal,
                    in    float   PlaneDistance,
                    in    float3  PlaneAlbedo,
                    inout HitInfo Hit)
{
    float Denom = dot(Ray.Dir, PlaneNormal);
    if (abs(Denom) < 0.0001)
        return;


    float HitDist = (PlaneDistance - dot(Ray.Origin, PlaneNormal)) / Denom;
    if (HitDist < 0.0)
        return;

    if (HitDist < Hit.Distance)
    {
        Hit.Albedo   = PlaneAlbedo;
        Hit.Normal   = PlaneNormal;
        Hit.Distance = HitDist;
    }
}

HitInfo IntersectScene(RayInfo Ray)
{
    HitInfo Hit;
    Hit.Albedo   = float3(0.0, 0.0, 0.0);
    Hit.Normal   = float3(0.0, 0.0, 0.0);
    Hit.Distance =  1e+30;

    float BoxSize = 5;
    // Right wall
    IntersectPlane(Ray, float3(+1.0, 0.0, 0.0), -BoxSize, float3(0.6, 0.1, 0.1), Hit);
    // Left wall
    IntersectPlane(Ray, float3(-1.0, 0.0, 0.0), -BoxSize, float3(0.1, 0.6, 0.1), Hit);
    // Top wall
    IntersectPlane(Ray, float3(0.0, -1.0, 0.0), -BoxSize, float3(0.5, 0.5, 0.5), Hit);
    // Bottom wall
    IntersectPlane(Ray, float3(0.0, +1.0, 0.0), -BoxSize, float3(0.5, 0.5, 0.5), Hit);
    // Back wall
    IntersectPlane(Ray, float3(0.0, 0.0, -1.0), -BoxSize, float3(0.5, 0.5, 0.5), Hit);

    return Hit;   
}


#endif // _SCENE_FXH_
