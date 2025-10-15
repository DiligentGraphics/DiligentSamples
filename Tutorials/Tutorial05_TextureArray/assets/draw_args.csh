cbuffer Constants
{
    uint g_NumInstances;
    uint g_Padding0;
    uint g_Padding1;
    uint g_Padding2;
};

struct IndirectDrawArgs
{
    uint NumIndices;
    uint NumInstances;
    uint FirstIndexLocation;
    uint BaseVertex;
    uint FirstInstanceLocation;
};

RWStructuredBuffer<IndirectDrawArgs> g_DrawArgsBuffer;

[numthreads(1, 1, 1)]
void main() 
{
    g_DrawArgsBuffer[0].NumInstances = g_NumInstances;
    g_DrawArgsBuffer[0].NumIndices   = 36;
}
