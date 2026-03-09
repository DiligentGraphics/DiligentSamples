#include "ConstantBufferDeclarations.hlsli"

cbuffer cbABufferConstants
{
    ABufferConstants g_ABufferConstants;
}

RWTexture2D<uint>        g_BufferHeadPointers;
RWStructuredBuffer<uint> g_BufferCounter;

[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void CSMain(uint3 ThreadID : SV_DispatchThreadID)
{
    if (any(ThreadID.xy >= g_ABufferConstants.ScreenSize))
        return;

    g_BufferHeadPointers[ThreadID.xy] = 0xFFFFFFFFu;

    // Reset counter once from thread (0,0)
    if (all(ThreadID.xy == 0u))
        g_BufferCounter[0] = 0u;
}
