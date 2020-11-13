
#include "structures.fxh"
#include "RayUtils.fxh"

ConstantBuffer<CubeAttribs>  g_CubeAttribsCB;

Texture2D     g_Texture[NUM_TEXTURES];
SamplerState  g_Texture_sampler; // By convention, texture samplers must use the '_sampler' suffix

[shader("closesthit")]
void main(inout PrimaryRayPayload payload, in BuiltInTriangleIntersectionAttributes attr)
{
    // Calculate triangle barycentrics.
    float3 barycentrics = float3(1.0 - attr.barycentrics.x - attr.barycentrics.y, attr.barycentrics.x, attr.barycentrics.y);

    // Get vertex indices for primitive.
    uint3  face = g_CubeAttribsCB.Primitives[PrimitiveIndex()].xyz;

    // Calculate texture coordinates.
    float2 uv = g_CubeAttribsCB.UVs[face.x].xy * barycentrics.x +
                g_CubeAttribsCB.UVs[face.y].xy * barycentrics.y +
                g_CubeAttribsCB.UVs[face.z].xy * barycentrics.z;

    // Calculate and transform triangle normal.
    float3 normal = g_CubeAttribsCB.Normals[face.x].xyz * barycentrics.x +
                    g_CubeAttribsCB.Normals[face.y].xyz * barycentrics.y +
                    g_CubeAttribsCB.Normals[face.z].xyz * barycentrics.z;
    normal        = normalize(mul((float3x3) ObjectToWorld3x4(), normal));

    // Sample texture. Ray tracing shader doesn't support LOD calculation, so you must specify LOD and apply filtering.
    payload.Color = g_Texture[InstanceID()].SampleLevel(g_Texture_sampler, uv, 0).rgb;
    payload.Depth = RayTCurrent();
    
    // Apply lighting.
    float3 rayOrigin = WorldRayOrigin() + WorldRayDirection() * RayTCurrent();
    LightingPass(payload.Color, rayOrigin, normal, payload.Recursion + 1);
}
