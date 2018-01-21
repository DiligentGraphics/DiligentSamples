#include "structures.fxh"

cbuffer VSConstants
{
    GlobalConstants g_Constants;
};

TerrainVSOut TerrainVS(uint BlockID : SV_VertexID)
{
    uint BlockHorzOrder = BlockID % g_Constants.NumHorzBlocks;
    uint BlockVertOrder = BlockID / g_Constants.NumHorzBlocks;
    
    float2 BlockOffset = float2( 
        float(BlockHorzOrder) / g_Constants.fNumHorzBlocks,
        float(BlockVertOrder) / g_Constants.fNumVertBlocks
    );

    TerrainVSOut Out;
    Out.BlockOffset = BlockOffset;
    return Out;
}
