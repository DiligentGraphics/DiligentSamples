#include "structures.fxh"

cbuffer cbConstants
{
    Constants g_Constants;
}

struct PSInput 
{
    float4 Pos   : SV_POSITION; 
    float4 Color : COLOR;
    float2 UV    : TEXCOORD;
};

static const uint constCubeIndices[12 * 3] =
{
    2, 0, 1,
    2, 3, 0,
    4, 6, 5,
    4, 7, 6,
    8, 10, 9,
    8, 11, 10,
    12, 14, 13,
    12, 15, 14,
    16, 18, 17,
    16, 19, 18,
    20, 21, 22,
    20, 22, 23,
};

static const float3 constCubePos[24] =
{
    float3(-1, -1, -1),
    float3(-1, +1, -1),
    float3(+1, +1, -1),
    float3(+1, -1, -1),
    float3(-1, -1, -1),
    float3(-1, -1, +1),
    float3(+1, -1, +1),
    float3(+1, -1, -1),
    float3(+1, -1, -1),
    float3(+1, -1, +1),
    float3(+1, +1, +1),
    float3(+1, +1, -1),
    float3(+1, +1, -1),
    float3(+1, +1, +1),
    float3(-1, +1, +1),
    float3(-1, +1, -1),
    float3(-1, +1, -1),
    float3(-1, +1, +1),
    float3(-1, -1, +1),
    float3(-1, -1, -1),
    float3(-1, -1, +1),
    float3(+1, -1, +1),
    float3(+1, +1, +1),
    float3(-1, +1, +1),
};

static const float2 constCubeUVs[24] =
{
    float2(0.0f, 0.5f),
    float2(0.0f, 0.0f),
    float2(0.5f, 0.0f),
    float2(0.5f, 0.5f),
    float2(0.0f, 0.5f),
    float2(0.5f, 0.5f),
    float2(0.5f, 1.0f),
    float2(0.0f, 1.0f),
    float2(0.5f, 0.5f),
    float2(0.0f, 0.5f),
    float2(0.0f, 0.0f),
    float2(0.5f, 0.0f),
    float2(0.5f, 0.5f),
    float2(1.0f, 0.5f),
    float2(1.0f, 1.0f),
    float2(0.5f, 1.0f),
    float2(0.0f, 0.0f),
    float2(0.5f, 0.0f),
    float2(0.5f, 0.5f),
    float2(0.0f, 0.5f),
    float2(0.5f, 0.5f),
    float2(0.0f, 0.5f),
    float2(0.0f, 0.0f),
    float2(0.5f, 0.0f),
};

const static float4 primitiveColors[12] =
{
    float4(76.f / 255.f, 74.f / 255.f, 89.f / 255.f, 1.0f),
    float4(27.f / 255.f, 127.f / 255.f, 122.f / 255.f, 1.0f),
    float4(8.f / 255.f, 151.f / 255.f, 180.f / 255.f, 1.0f),
    float4(76.f / 255.f, 171.f / 255.f, 166.f / 255.f, 1.0f),
    float4(242.f / 255.f, 196.f / 255.f, 226.f / 255.f, 1.0f),
    float4(182.f / 255.f, 242.f / 255.f, 242.f / 255.f, 1.0f),
    float4(194.f / 255.f, 242.f / 255.f, 197.f / 255.f, 1.0f),
    float4(242.f / 255.f, 177.f / 255.f, 153.f / 255.f, 1.0f),
    float4(242.f / 255.f, 145.f / 255.f, 145.f / 255.f, 1.0f),
    float4(144.f / 255.f, 222.f / 255.f, 179.f / 255.f, 1.0f),
    float4(255.f / 255.f, 218.f / 255.f, 185.f / 255.f, 1.0f),
    float4(172.f / 255.f, 245.f / 255.f, 184.f / 255.f, 1.0f),
};

float4 getRandomPrimitiveColor(float randMeshletVal)
{    
    uint colorIdx = floor(randMeshletVal * 12);
    return primitiveColors[colorIdx];
}

[numthreads(24, 1, 1)]       // use 24 threads out of the 32 maximum available
[outputtopology("triangle")] // output primitive type is triangle list
void main(in uint I : SV_GroupIndex, // thread index used to access mesh shader output (0 .. 23)
          in uint gid : SV_GroupID,      // work group index used to access amplification shader output (0 .. s_TaskCount-1)
          in  payload  Payload  payload, // entire amplification shader output can be accessed by the mesh shader
          out indices  uint3    tris[12],
          out vertices PSInput  verts[24])
{        
    // The cube contains 24 vertices, 36 indices for 12 triangles.
    // Only the input values from the the first active thread are used.
    SetMeshOutputCounts(24, 12);
    
    // Read the amplification shader output for this group
    float3 pos;
    float scale = payload.Scale[gid];
    float randValue = payload.MSRand[gid];
    pos.x = payload.PosX[gid];
    pos.y = payload.PosY[gid];
    pos.z = payload.PosZ[gid];
    
    // Each thread handles only one vertex
    verts[I].Pos = mul(float4(pos + constCubePos[I].xyz * scale, 1.0), g_Constants.ViewProjMat);
    verts[I].UV = constCubeUVs[I].xy;
    verts[I].Color = g_Constants.MSDebugViz * getRandomPrimitiveColor(randValue);
    
    // Only the first 12 threads write indices. We must not access the array outside of its bounds.
    if (I < 12)
    {
        tris[I] = float3(constCubeIndices[I * 3 + 0], constCubeIndices[I * 3 + 1], constCubeIndices[I * 3 + 2]);
    }
}