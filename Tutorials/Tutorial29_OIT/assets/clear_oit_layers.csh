#include "common.fxh"
#include "oit.fxh"

cbuffer cbConstants
{
    Constants g_Constants;
}

RWStructuredBuffer<uint> g_rwOITLayers;

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 16
#endif

[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void main(uint3 ThreadID : SV_DispatchThreadID)
{
    if (ThreadID.x >= g_Constants.ScreenSize.x ||
        ThreadID.y >= g_Constants.ScreenSize.y)
        return;
    
    uint Offset = GetOITLayerDataOffset(ThreadID.xy, g_Constants.ScreenSize);
    for (uint layer = 0; layer < uint(NUM_OIT_LAYERS); ++layer)
    {
        g_rwOITLayers[Offset + layer] = 0xFFFFFFFFu;
    }
}
