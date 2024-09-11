#include "structures.fxh"

// Draw task arguments
StructuredBuffer<DrawTask> DrawTasks;

cbuffer cbConstants
{
    Constants g_Constants;
}

// Statistics buffer contains the global counter of visible objects
RWByteAddressBuffer Statistics;

// Payload will be used in the mesh shader.
groupshared Payload s_Payload;

// The sphere is visible when the distance from each plane is greater than or
// equal to the radius of the sphere.
bool IsVisible(float3 cubeCenter, float radius)
{
    float4 center = float4(cubeCenter, 1.0);

    for (int i = 0; i < 6; ++i)
    {
        if (dot(g_Constants.Frustum[i], center) < -radius)
            return false;
    }
    return true;
}

bool IsOccluded(uint taskGID) //,float3 cameraPos)
{
    // Voxels are inherently AABBs
    
    
    // Iterate over all meshlets (all DrawTasks) and calculate occlusion relative to 
    // this meshlet in relation to the camera position
    for (uint i = 0; i < 1; ++i)
    {
        if (i == taskGID) // Both meshlets to be tested against are the same
            continue;   
            
        DrawTask task = DrawTasks[i];
        float3 otherMeshletPos = task.BasePosAndScale.xzy;
        float3 thisMeshletPos = DrawTasks[taskGID].BasePosAndScale.xyz;
    }
    
    // If meshlet is occluded by at least one other meshlet (which is not this meshlet)
    // return true, 
    // GPU Resident Drawer -> Unity
    // @TODO: Find out, which optimization techniques to use for meshlet culling
    
    
    // If none is occluded, return false
        return false;
}

float CalcDetailLevel(float3 cubeCenter, float radius)
{
    // cubeCenter - the center of the sphere 
    // radius     - the radius of circumscribed sphere
    
    // Get the position in the view space
    float3 pos = mul(float4(cubeCenter, 1.0), g_Constants.ViewMat).xyz;
    
    // Square of distance from camera to circumscribed sphere
    float dist2 = dot(pos, pos);
    
    // Calculate the sphere size in screen space
    float size = g_Constants.CoTanHalfFov * radius / sqrt(dist2 - radius * radius);
    
    // Calculate detail level
    float level = clamp(1.0 - size, 0.0, 1.0);
    return level;
}

// The number of cubes that are visible by the camera,
// computed by every thread group
groupshared uint s_TaskCount;

[numthreads(GROUP_SIZE, 1, 1)]
void main(in uint I  : SV_GroupIndex,
          in uint wg : SV_GroupID)
{
    // Reset the counter from the first thread in the group
    if (I == 0)
    {
        s_TaskCount = 0;
    }

    // Flush the cache and synchronize
    GroupMemoryBarrierWithGroupSync();

    // Read the task arguments
    const uint gid   = wg * GROUP_SIZE + I;
    DrawTask   task  = DrawTasks[gid];
    float3     pos   = task.BasePosAndScale.xyz;
    float      scale = task.BasePosAndScale.w;
    float      meshletColorRndValue = task.randomValue.x;

    
    /* 
        Fustum Culling before occlusion culling ? 
        Both in the same call ? 
        How can I optimize culling computation scaling with the amount of meshlets?
    */
    
    
    // Frustum culling
    if ((g_Constants.FrustumCulling == 0 || IsVisible(pos, 1.73 * scale)) &&
        (g_Constants.OcclusionCulling == 0 || !IsOccluded(gid)))
    {
        // Acquire an index that will be used to safely access the payload.
        // Each thread gets a unique index.
        uint index = 0;
        InterlockedAdd(s_TaskCount, 1, index);
        
        s_Payload.PosX[index]  = pos.x;
        s_Payload.PosY[index]  = pos.y;
        s_Payload.PosZ[index]  = pos.z;
        s_Payload.Scale[index] = scale;
        s_Payload.MSRand[index] = meshletColorRndValue;
    }
    
    // All threads must complete their work so that we can read s_TaskCount
    GroupMemoryBarrierWithGroupSync();

    if (I == 0)
    {
        // Update statistics from the first thread
        uint orig_value;
        Statistics.InterlockedAdd(0, s_TaskCount, orig_value);
    }
    
    // This function must be called exactly once per amplification shader.
    // The DispatchMesh call implies a GroupMemoryBarrierWithGroupSync(), and ends the amplification shader group's execution.
    DispatchMesh(s_TaskCount, 1, 1, s_Payload);
}
