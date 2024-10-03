#include "structures.fxh"

#ifndef SHOW_STATISTICS
#define SHOW_STATISTICS 1
#endif
// Draw task arguments
StructuredBuffer<DrawTask> DrawTasks;

// Inidces into the drawtask buffer which group adjacent voxels together
StructuredBuffer<int> GridIndices;

// Description of the spatial layout of the nodes, containing adjacent voxels
StructuredBuffer<GPUOctreeNode> OctreeNodes;

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
bool IsVisible(float4 min, float4 max)
{
    float4 center = float4(((max + min).xyz * 0.5f), 1.0f);
    float radius = length(max - center); // Add some padding for conservative culling
    
    for (int i = 0; i < 6; ++i)
    {
        if (dot(g_Constants.Frustum[i], center) < -radius)
            return false;
    }
    return true;
}


// The number of cubes that are visible by the camera,
// computed by every thread group
groupshared uint s_TaskCount;
groupshared uint s_OctreeNodeCount;

[numthreads(GROUP_SIZE, 1, 1)]
void main(in uint I  : SV_GroupIndex,
          in uint wg : SV_GroupID)
{
    // Reset the counter from the first thread in the group
    if (I == 0)
    {
        s_TaskCount = 0;
#if SHOW_STATISTICS
        s_OctreeNodeCount = 0;
#endif
    }

    // Flush the cache and synchronize
    GroupMemoryBarrierWithGroupSync();

    // Read the first task arguments in order to get some constant data
    const uint gid = wg * GROUP_SIZE + I;   
    DrawTask firstTask = DrawTasks[wg];
    float meshletColorRndValue = firstTask.RandomValue.x;
    int taskCount = (int) firstTask.RandomValue.y;
    int padding = (int) firstTask.RandomValue.z;
    
    // Get the node for this thread group
    GPUOctreeNode node = OctreeNodes[wg];
    
    // Access node indices for each thread 
    if ((g_Constants.FrustumCulling == 0 || IsVisible(node.minAndIsFull, node.max))
        && I < node.numChildren)
    {
        DrawTask task = DrawTasks[GridIndices[node.childrenStartIndex + I]];
        float3 pos = task.BasePosAndScale.xyz;
        float scale = task.BasePosAndScale.w;
    
        // Atomically increase task count
        uint index = 0;
        InterlockedAdd(s_TaskCount, 1, index);

        // Add mesh data to payload
        s_Payload.PosX[index] = pos.x;
        s_Payload.PosY[index] = pos.y;
        s_Payload.PosZ[index] = pos.z;
        s_Payload.Scale[index] = scale;
        s_Payload.MSRand[index] = meshletColorRndValue;
        
#if SHOW_STATISTICS
        
        if (I == 0)
        {
            uint temp = 0;
            InterlockedAdd(s_OctreeNodeCount, 1, temp);
        }
#endif
    }
    
    // All threads must complete their work so that we can read s_TaskCount
    GroupMemoryBarrierWithGroupSync();

    if (I == 0)
    {
        // Update statistics from the first thread
        uint orig_value_task_count;
        Statistics.InterlockedAdd(0, s_TaskCount, orig_value_task_count);
#if SHOW_STATISTICS
        uint orig_value_ocn_count;
        Statistics.InterlockedAdd(4, s_OctreeNodeCount, orig_value_ocn_count);
#endif
    }
    
    // This function must be called exactly once per amplification shader.
    // The DispatchMesh call implies a GroupMemoryBarrierWithGroupSync(), and ends the amplification shader group's execution.
    DispatchMesh(s_TaskCount, 1, 1, s_Payload);
}
