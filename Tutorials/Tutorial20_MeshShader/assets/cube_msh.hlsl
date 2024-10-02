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

const static float4 primitiveColors[20] =
{
    float4(76.f / 255.f, 74.f / 255.f, 89.f / 255.f, 1.0f), // Dunkelviolett
    float4(255.f / 255.f, 107.f / 255.f, 107.f / 255.f, 1.0f), // Korallenrot
    float4(50.f / 255.f, 168.f / 255.f, 82.f / 255.f, 1.0f), // Smaragdgrün
    float4(255.f / 255.f, 195.f / 255.f, 0.f / 255.f, 1.0f), // Goldgelb
    float4(0.f / 255.f, 153.f / 255.f, 255.f / 255.f, 1.0f), // Hellblau
    float4(255.f / 255.f, 128.f / 255.f, 0.f / 255.f, 1.0f), // Orange
    float4(138.f / 255.f, 43.f / 255.f, 226.f / 255.f, 1.0f), // Blauviolett
    float4(0.f / 255.f, 204.f / 255.f, 102.f / 255.f, 1.0f), // Mintgrün
    float4(255.f / 255.f, 0.f / 255.f, 127.f / 255.f, 1.0f), // Magenta
    float4(204.f / 255.f, 204.f / 255.f, 0.f / 255.f, 1.0f), // Olivgrün
    float4(102.f / 255.f, 204.f / 255.f, 255.f / 255.f, 1.0f), // Himmelblau
    float4(255.f / 255.f, 153.f / 255.f, 204.f / 255.f, 1.0f), // Rosa
    float4(153.f / 255.f, 51.f / 255.f, 0.f / 255.f, 1.0f), // Rotbraun
    float4(0.f / 255.f, 255.f / 255.f, 204.f / 255.f, 1.0f), // Türkis
    float4(204.f / 255.f, 0.f / 255.f, 204.f / 255.f, 1.0f), // Lila
    float4(255.f / 255.f, 204.f / 255.f, 153.f / 255.f, 1.0f), // Pfirsich
    float4(51.f / 255.f, 102.f / 255.f, 0.f / 255.f, 1.0f), // Dunkelgrün
    float4(102.f / 255.f, 0.f / 255.f, 102.f / 255.f, 1.0f), // Dunkelviolett
    float4(255.f / 255.f, 255.f / 255.f, 153.f / 255.f, 1.0f), // Hellgelb
    float4(0.f / 255.f, 102.f / 255.f, 204.f / 255.f, 1.0f), // Königsblau
};

float4 getRandomPrimitiveColor(float randMeshletVal)
{    
    uint colorIdx = floor(randMeshletVal * 20);
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
    verts[I].Color = g_Constants.MSDebugViz * getRandomPrimitiveColor((randValue + ((1 - g_Constants.OctreeDebugViz) * 0.1f * gid)) % 1.0f);
    
    // Only the first 12 threads write indices. We must not access the array outside of its bounds.
    if (I < 12)
    {
        tris[I] = float3(constCubeIndices[I * 3 + 0], constCubeIndices[I * 3 + 1], constCubeIndices[I * 3 + 2]);
    }
}