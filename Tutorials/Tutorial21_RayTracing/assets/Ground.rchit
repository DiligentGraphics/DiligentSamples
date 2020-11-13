
#include "structures.fxh"
#include "RayUtils.fxh"

ConstantBuffer<CubeAttribs>  g_CubeAttribsCB;

Texture2D     g_GroundTexture;
SamplerState  g_GroundTexture_sampler; // By convention, texture samplers must use the '_sampler' suffix

[shader("closesthit")]
void main(inout PrimaryRayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    float3 barycentrics = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    uint4  face         = g_CubeAttribsCB.Primitives[PrimitiveIndex()];
    float2 uv           = g_CubeAttribsCB.UVs[face.x].xy * barycentrics.x +
                          g_CubeAttribsCB.UVs[face.y].xy * barycentrics.y +
                          g_CubeAttribsCB.UVs[face.z].xy * barycentrics.z;
    uv *= 32.0; // tiling
    float3  origin      = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3  normal      = float3(0.0, -1.0, 0.0);

    payload.Color = g_GroundTexture.SampleLevel(g_GroundTexture_sampler, uv, 0).rgb;
    payload.Depth = RayTCurrent();

    LightingPass(payload.Color, origin, normal, payload.Recursion + 1);
}
