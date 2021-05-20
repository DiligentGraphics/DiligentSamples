
#include "structures.fxh"
#include "RayUtils.fxh"

ConstantBuffer<CubeAttribs>  g_CubeAttribsCB;

Texture2D     g_GroundTexture;
SamplerState  g_SamLinearWrap;

[shader("closesthit")]
void main(inout PrimaryRayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // Calculate triangle barycentrics.
    float3 barycentrics = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);
    
    // Get vertex indices for primitive.
    uint3 primitive = g_CubeAttribsCB.Primitives[PrimitiveIndex()].xyz;
    
    // Calculate texture coordinates.
    float2 uv = g_CubeAttribsCB.UVs[primitive.x].xy * barycentrics.x +
                g_CubeAttribsCB.UVs[primitive.y].xy * barycentrics.y +
                g_CubeAttribsCB.UVs[primitive.z].xy * barycentrics.z;
    uv *= 32.0; // tiling

    payload.Color = g_GroundTexture.SampleLevel(g_SamLinearWrap, uv, 0).rgb;
    payload.Depth = RayTCurrent();

    // Setup ray origing and direction for shadow casting.
    float3  origin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    float3  normal = float3(0.0, 1.0, 0.0);

    LightingPass(payload.Color, origin, normal, payload.Recursion + 1);
}
