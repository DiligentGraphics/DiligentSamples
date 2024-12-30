#include "common.fxh"
#include "oit.fxh"

cbuffer cbConstants
{
    Constants g_Constants;
}

RWTexture2D<uint4> g_rwOITLayers;

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 16
#endif

[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void main(uint3 Gid  : SV_GroupID,
          uint3 GTid : SV_GroupThreadID)
{
    uint2 uiGlobalThreadXY = Gid.xy * uint(THREAD_GROUP_SIZE) + GTid.xy;
    if (uiGlobalThreadXY.x >= g_Constants.ScreenSize.x ||
        uiGlobalThreadXY.y >= g_Constants.ScreenSize.y)
        return;

    g_rwOITLayers[uiGlobalThreadXY] = GetDefaultOITLayers().Data;
}
