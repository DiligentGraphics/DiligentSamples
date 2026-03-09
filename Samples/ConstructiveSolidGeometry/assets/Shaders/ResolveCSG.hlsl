#include "BasicStructures.fxh"
#include "PBR_Structures.fxh"
#include "CSGHelpers.hlsli"

#ifndef MAX_SORT_FRAGMENTS
#   define MAX_SORT_FRAGMENTS 32
#endif

#ifndef MAX_MATERIAL_COUNT
#   define MAX_MATERIAL_COUNT 24
#endif

#ifndef MAX_OBJECT_COUNT
#   define MAX_OBJECT_COUNT 32
#endif

#ifndef THREAD_GROUP_SIZE
#   define THREAD_GROUP_SIZE 8
#endif

cbuffer cbCameraAttribs
{
    CameraAttribs g_Camera;
}

cbuffer cbPBRRendererAttibs
{
    PBRRendererShaderParameters g_PBRRendererAttibs;
}

cbuffer cbABufferConstants
{
    ABufferConstants g_ABufferConstants;
}

cbuffer cbMaterialAttribs
{
    MaterialAttribs g_MaterialAttribs[MAX_MATERIAL_COUNT];
}

Texture2D<uint>                   g_BufferHeadPointers;
StructuredBuffer<ABufferFragment> g_NodeBuffer;
StructuredBuffer<CSGOperationAttribs> g_CSGOperations;

RWTexture2D<float4> g_TextureRadiance;

TextureCube<float3> g_TexturePrefilteredEnvironmentMap;
SamplerState        g_TexturePrefilteredEnvironmentMap_sampler;

Texture2D<float2> g_TextureBRDFIntegrationMap;
SamplerState      g_TextureBRDFIntegrationMap_sampler;


#define THREADS_PER_GROUP (THREAD_GROUP_SIZE * THREAD_GROUP_SIZE)
#define LDS_ARRAY_SIZE    (THREADS_PER_GROUP * MAX_SORT_FRAGMENTS)

groupshared float gs_Depths     [LDS_ARRAY_SIZE];
groupshared uint  gs_PackedData [LDS_ARRAY_SIZE];

// Per-thread LDS indexing helpers
uint LDSIndex(uint ThreadIdx, uint FragIdx)
{
    return ThreadIdx * MAX_SORT_FRAGMENTS + FragIdx;
}

float3 FresnelSchlickRoughness(float CosTheta, float3 F0, float Roughness)
{
    float Alpha = 1.0 - Roughness;
    return F0 + (max(float3(Alpha, Alpha, Alpha), F0) - F0) * pow(clamp(1.0 - CosTheta, 0.0, 1.0), 5.0);
}

float3 CreateViewDir(float2 NormalizedXY)
{
    float4 RayStart = mul(float4(NormalizedXY, DepthToNormalizedDeviceZ(0.0), 1.0f), g_Camera.mViewProjInv);
    float4 RayEnd   = mul(float4(NormalizedXY, DepthToNormalizedDeviceZ(1.0), 1.0f), g_Camera.mViewProjInv);

    RayStart.xyz /= RayStart.w;
    RayEnd.xyz   /= RayEnd.w;
    return normalize(RayStart.xyz - RayEnd.xyz);
}

float3 ComputeCSGLighting(float3 BaseColor, float3 Normal, float Metalness, float Roughness, float3 ViewDir)
{
    // Directional + Ambient lighting
    float3 LightDir = g_ABufferConstants.LightDirection.xyz;
    float3 LightCol = g_ABufferConstants.LightColor.xyz;
    float3 Ambient  = g_ABufferConstants.AmbientColor.xyz;

    float NdotL = max(0.0, dot(Normal, -LightDir));
    float3 Diffuse = BaseColor * (Ambient + LightCol * NdotL);

    float3 F0 = lerp(float3(0.04, 0.04, 0.04), BaseColor, Metalness);

    // Direct specular (Blinn-Phong approximation)
    float3 H = normalize(ViewDir - LightDir);
    float NdotH = max(0.0, dot(Normal, H));
    float SpecPower = max(2.0, 2.0 / max(Roughness * Roughness, 0.001) - 2.0);
    float3 DirectSpecular = F0 * LightCol * pow(NdotH, SpecPower) * NdotL;

    // IBL specular reflections for volume perception
    float NdotV = saturate(dot(Normal, ViewDir));
    float3 R = reflect(-ViewDir, Normal);
    float3 F = FresnelSchlickRoughness(NdotV, F0, Roughness);
    float2 BRDF = g_TextureBRDFIntegrationMap.SampleLevel(g_TextureBRDFIntegrationMap_sampler, float2(NdotV, Roughness), 0.0);
    float3 PrefilteredColor = 
        g_PBRRendererAttibs.IBLScale.xyz *
        g_TexturePrefilteredEnvironmentMap.SampleLevel(g_TexturePrefilteredEnvironmentMap_sampler, float3(+1.0, -1.0, +1.0) * R, Roughness * g_PBRRendererAttibs.PrefilteredCubeLastMip);

    float3 IBLSpecular = PrefilteredColor * (F * BRDF.x + BRDF.yyy);
    return Diffuse + DirectSpecular + IBLSpecular;
}

[numthreads(THREAD_GROUP_SIZE, THREAD_GROUP_SIZE, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID, uint3 GTid : SV_GroupThreadID)
{
    uint2 PixelCoord = DTid.xy;

    // Early out for pixels beyond screen bounds
    if (any(PixelCoord >= g_ABufferConstants.ScreenSize))
        return;

    uint ThreadIdx = GTid.y * THREAD_GROUP_SIZE + GTid.x;

    uint NodeIdx = g_BufferHeadPointers[PixelCoord];

    // Compute normalized device coordinates [-1, 1]
    float2 NormalizedXY = (float2(PixelCoord) + 0.5) * float2(2.0, -2.0) / float2(g_ABufferConstants.ScreenSize) + float2(-1.0, 1.0);

    // If no fragments at this pixel, skip
    if (NodeIdx == 0xFFFFFFFFu)
        return;

    float3 ViewDir = CreateViewDir(NormalizedXY);

    // Collect all fragments from linked list into LDS
    uint FragCount = 0u;

    while (NodeIdx != 0xFFFFFFFFu && FragCount < uint(MAX_SORT_FRAGMENTS))
    {
        ABufferFragment Node = g_NodeBuffer[NodeIdx];

        uint Idx = LDSIndex(ThreadIdx, FragCount);
        gs_Depths[Idx]     = Node.Depth;
        gs_PackedData[Idx] = Node.PackedData;

        FragCount++;
        NodeIdx = Node.Next;
    }

    // Insertion sort by depth (front-to-back, ascending)
    // Each thread sorts its own fragment slice in LDS
    for (uint i = 1u; i < FragCount; ++i)
    {
        float keyDepth     = gs_Depths[LDSIndex(ThreadIdx, i)];
        uint  keyPackedData = gs_PackedData[LDSIndex(ThreadIdx, i)];

        uint j = i;
        while (j > 0u && gs_Depths[LDSIndex(ThreadIdx, j - 1u)] > keyDepth)
        {
            uint Dst = LDSIndex(ThreadIdx, j);
            uint Src = LDSIndex(ThreadIdx, j - 1u);
            gs_Depths[Dst]     = gs_Depths[Src];
            gs_PackedData[Dst] = gs_PackedData[Src];
            j--;
        }
        gs_Depths[LDSIndex(ThreadIdx, j)]     = keyDepth;
        gs_PackedData[LDSIndex(ThreadIdx, j)] = keyPackedData;
    }

    // CSG resolve: walk sorted fragments, track inside/outside state per object,
    // and emit surfaces at CSG boundary transitions.

    // Inside state indexed directly by ObjectID (no linear search needed)
    uint InsideCount[MAX_OBJECT_COUNT];
    for (uint p = 0u; p < uint(MAX_OBJECT_COUNT); ++p)
        InsideCount[p] = 0u;

    // Walk sorted fragments front-to-back.
    // With fully opaque materials, only the first visible CSG surface matters.
    for (uint k = 0u; k < FragCount; ++k)
    {
        float3 Normal;
        uint   MatIdx, ObjID, IsExit;
        UnpackFragmentData(gs_PackedData[LDSIndex(ThreadIdx, k)], Normal, MatIdx, ObjID, IsExit);

        // Find the CSG operation this object participates in
        uint CSGOp          = CSG_OP_UNION;
        bool IsPrimary      = true;
        uint OpIdx          = 0u;
        bool OpFound        = false;
        uint PrimaryObjID   = ObjID;
        uint SecondaryObjID = 0xFFu;
        for (uint oi = 0u; oi < g_ABufferConstants.OperationCount; ++oi)
        {
            CSGOperationAttribs Op = g_CSGOperations[oi];
            if (Op.PrimaryObjectID == ObjID || Op.SecondaryObjectID == ObjID)
            {
                CSGOp          = Op.Operation;
                IsPrimary      = (Op.PrimaryObjectID == ObjID);
                OpIdx          = oi;
                OpFound        = true;
                PrimaryObjID   = Op.PrimaryObjectID;
                SecondaryObjID = Op.SecondaryObjectID;
                break;
            }
        }

        // Evaluate CSG state BEFORE this transition
        bool InsideA_Before = InsideCount[PrimaryObjID] > 0u;
        bool InsideB_Before = (SecondaryObjID < uint(MAX_OBJECT_COUNT)) ? (InsideCount[SecondaryObjID] > 0u) : false;
        bool CSG_Before = EvalCSG(CSGOp, InsideA_Before, InsideB_Before);

        // Update inside state for this fragment's object
        if (IsExit == ABUFFER_FACE_EXIT)
        {
            if (InsideCount[ObjID] > 0u)
                InsideCount[ObjID]--;
        }
        else
        {
            InsideCount[ObjID]++;
        }

        // Evaluate CSG state AFTER this transition
        bool InsideA_After = InsideCount[PrimaryObjID] > 0u;
        bool InsideB_After = (SecondaryObjID < uint(MAX_OBJECT_COUNT)) ? (InsideCount[SecondaryObjID] > 0u) : false;
        bool CSG_After = EvalCSG(CSGOp, InsideA_After, InsideB_After);

        // A visible surface exists where the CSG state transitions
        if (CSG_Before != CSG_After)
        {
            // For subtraction (A-B), when the carved surface belongs to the
            // secondary object (B), show the primary's (A) material instead.
            if (CSGOp == CSG_OP_SUBTRACTION && !IsPrimary && OpFound)
                MatIdx = g_CSGOperations[OpIdx].PrimaryMaterialIdx;

            // For exit fragments, flip the normal (we stored outward-facing normals)
            if (IsExit == ABUFFER_FACE_EXIT)
                Normal = -Normal;

            MaterialAttribs Mat = g_MaterialAttribs[MatIdx];
            float3 LitColor = ComputeCSGLighting(Mat.BaseColor.rgb, Normal, Mat.Metalness, Mat.Roughness, ViewDir);

            g_TextureRadiance[PixelCoord] = float4(LitColor, 1.0);
            return;
        }
    }
}
