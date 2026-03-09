#ifndef _RAY_INTERSECTION_HLSLI_
#define _RAY_INTERSECTION_HLSLI_


#define M_EPSILON   1e-3
#define FLT_MAX     3.402823466e+38

struct Ray
{
    float3 Origin;
    float3 Direction;
    float  Min;
    float  Max;
};

struct IntersectionInterval
{
    float  TNear;
    float  TFar;
    float3 NormalNear;  // outward-facing normal at entry point
    float3 NormalFar;   // outward-facing normal at exit point
    bool   Hit;
};

Ray CreateCameraRay(float2 NormalizedXY)
{
    float4 RayStart = mul(float4(NormalizedXY, DepthToNormalizedDeviceZ(0.0), 1.0f), g_CurrCamera.mViewProjInv);
    float4 RayEnd   = mul(float4(NormalizedXY, DepthToNormalizedDeviceZ(1.0), 1.0f), g_CurrCamera.mViewProjInv);

    RayStart.xyz /= RayStart.w;
    RayEnd.xyz   /= RayEnd.w;

    Ray ray;
    ray.Direction = normalize(RayEnd.xyz - RayStart.xyz);
    ray.Origin = RayStart.xyz;
    ray.Min = 0.0;
    ray.Max = distance(RayStart.xyz, RayEnd.xyz);
    return ray;
}

float ComputeDepth(float3 Position)
{
    float4 NDC = mul(float4(Position, 1.0), g_CurrCamera.mViewProj);
    NDC.xyz /= NDC.w;
    return NormalizedDeviceZToDepth(NDC.z);
}

Ray TransformRayToLocalSpace(Ray InputRay)
{
    float4 Position  = mul(float4(InputRay.Origin, 1.0), g_ObjectAttribs.CurrInvWorldMatrix);
    float3 Direction = mul(float4(InputRay.Direction, 0.0), g_ObjectAttribs.CurrInvWorldMatrix).xyz;

    Ray Result;
    Result.Origin = Position.xyz;
    Result.Direction = Direction;
    Result.Min = InputRay.Min;
    Result.Max = InputRay.Max;
    return Result;
}

float3 ComputeNormal(float3 Normal)
{
    return normalize(mul(float4(Normal, 0.0), g_ObjectAttribs.CurrNormalMatrix).xyz);
}

IntersectionInterval IntersectSphereInterval(Ray InputRay, float3 Center, float Radius)
{
    IntersectionInterval Result;
    Result.Hit = false;
    Result.TNear = FLT_MAX;
    Result.TFar  = -FLT_MAX;
    Result.NormalNear = float3(0.0, 0.0, 0.0);
    Result.NormalFar  = float3(0.0, 0.0, 0.0);

    float3 Orig = InputRay.Origin - Center;
    float A = dot(InputRay.Direction, InputRay.Direction);
    float B = 2.0 * dot(Orig, InputRay.Direction);
    float C = dot(Orig, Orig) - Radius * Radius;
    float D = B * B - 4.0 * A * C;

    if (D > 0.0)
    {
        D = sqrt(D);
        float TNear = (-B - D) / (2.0 * A);
        float TFar  = (-B + D) / (2.0 * A);

        if (TFar > M_EPSILON)
        {
            Result.Hit   = true;
            Result.TNear = TNear;
            Result.TFar  = TFar;

            float3 HitNear = InputRay.Origin + TNear * InputRay.Direction;
            float3 HitFar  = InputRay.Origin + TFar  * InputRay.Direction;
            Result.NormalNear = normalize(HitNear - Center);
            Result.NormalFar  = normalize(HitFar  - Center);
        }
    }
    return Result;
}

IntersectionInterval IntersectAABBInterval(Ray InputRay, float3 Center, float3 Size)
{
    IntersectionInterval Result;
    Result.Hit = false;
    Result.TNear = FLT_MAX;
    Result.TFar  = -FLT_MAX;
    Result.NormalNear = float3(0.0, 0.0, 0.0);
    Result.NormalFar  = float3(0.0, 0.0, 0.0);

    float3 T1 = (Center - Size - InputRay.Origin) / (InputRay.Direction + M_EPSILON);
    float3 T2 = (Center + Size - InputRay.Origin) / (InputRay.Direction + M_EPSILON);

    float3 TMin = min(T1, T2);
    float3 TMax = max(T1, T2);
    float TNear = max(max(TMin.x, TMin.y), TMin.z);
    float TFar  = min(min(TMax.x, TMax.y), TMax.z);

    if (TNear < TFar && TFar > M_EPSILON)
    {
        Result.Hit   = true;
        Result.TNear = TNear;
        Result.TFar  = TFar;

        // Entry normal: face that was hit for entry
        Result.NormalNear = -sign(InputRay.Direction) * step(TMin.yzx, TMin.xyz) * step(TMin.zxy, TMin.xyz);
        // Exit normal: face that was hit for exit
        Result.NormalFar  = sign(InputRay.Direction) * step(TMax.xyz, TMax.yzx) * step(TMax.xyz, TMax.zxy);
    }
    return Result;
}

IntersectionInterval IntersectGeometryInterval(Ray RayLS, uint ObjectType)
{
    IntersectionInterval Result;
    Result.Hit = false;
    Result.TNear = FLT_MAX;
    Result.TFar  = -FLT_MAX;
    Result.NormalNear = float3(0.0, 0.0, 0.0);
    Result.NormalFar  = float3(0.0, 0.0, 0.0);

    switch (ObjectType)
    {
        case GEOMETRY_TYPE_SPHERE:
            Result = IntersectSphereInterval(RayLS, float3(0.0, 0.0, 0.0), 1.0);
            break;
        case GEOMETRY_TYPE_AABB:
            Result = IntersectAABBInterval(RayLS, float3(0.0, 0.0, 0.0), float3(1.0, 1.0, 1.0));
            break;
        default:
            break;
    }
    return Result;
}

#endif // _RAY_INTERSECTION_HLSLI_
