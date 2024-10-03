
StructuredBuffer<bool> VisibilityBuffer;

[numthreads(1, 1, 1)]
void main( uint3 DTid : SV_DispatchThreadID )
{
    // first pass and second pass of two-pass occlusion culling   
    
}
