#include "structures.fxh"

#ifndef USE_GPU_FRUSTUM_CULLING
#define USE_GPU_FRUSTUM_CULLING
#endif
// Draw task arguments
StructuredBuffer<DrawTask> DrawTasks;
StructuredBuffer<int> GridIndices;

//StructuredBuffer<GPUOctreeNode> SpatialLookup;

cbuffer cbConstants
{
    Constants g_Constants;
}

// Statistics buffer contains the global counter of visible objects
RWByteAddressBuffer Statistics;

// Payload will be used in the mesh shader.
groupshared Payload s_Payload;

#ifdef USE_GPU_FRUSTUM_CULLING

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

#endif

//bool IsOccludedByNeighbouringVoxel(float3 position, float3 cameraDirection)
//{
//    // Check if the voxel in relative camera direction is occluding this voxel
//    float voxelSize = 0.010f;
//    float3 targetPosition;
    
//    targetPosition.x = position.x + (cameraDirection.x > 0.5f ? voxelSize : 0f);
//    targetPosition.x += (cameraDirection.x < -0.5f ? -voxelSize : 0f);
    
//    targetPosition.y = position.y + (cameraDirection.y > 0.5f ? voxelSize : 0f);
//    targetPosition.y += (cameraDirection.y < -0.5f ? -voxelSize : 0f);
    
//    targetPosition.z = position.z + (cameraDirection.z > 0.5f ? voxelSize : 0f);
//    targetPosition.z += (cameraDirection.z < -0.5f ? -voxelSize : 0f);
    
    
//    // Index targetPosition(s) into Spatial Container and check if there are any voxels resident
//    // If so, return true, if not, false
//    return false;
//}

//bool IsOccluded(float3 cubeCenter, float radius)
//{
//    float3 center = cubeCenter;
//    float3 cameraPosition = g_Constants.ViewMat._11_11_11; // @TODO: Find appropriate members here
//    float3 vecToCam = normalize(cameraPosition - center);
    
//    return IsOccludedByNeighbouringVoxel(center, vecToCam);
//}

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
    float      meshletColorRndValue = task.RandomValue.x;

    
    /* 
        Frustum Culling before occlusion culling ? 
        Both in the same call ? 
        How can I optimize culling computation scaling with the amount of meshlets?
    */
    
    #ifdef USE_GPU_FRUSTUM_CULLING
    // Frustum culling
    if ((g_Constants.FrustumCulling == 0 || IsVisible(pos, 1.73 * scale)))
    {
    #endif
        // Acquire an index that will be used to safely access the payload.
        // Each thread gets a unique index.
        uint index = 0;
        InterlockedAdd(s_TaskCount, 1, index);
        
        s_Payload.PosX[index] = pos.x;
        s_Payload.PosY[index] = pos.y;
        s_Payload.PosZ[index] = pos.z;
        s_Payload.Scale[index] = scale;
        s_Payload.MSRand[index] = meshletColorRndValue;
        
    #ifdef USE_GPU_FRUSTUM_CULLING
    }
    #endif

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
