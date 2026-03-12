#include "BasicStructures.fxh"
#include "CSGHelpers.hlsli"

struct VSInput
{
    float3 Position : ATTRIB0;
    float3 Normal   : ATTRIB1;
};

struct PSInput
{
    float4 PixelPosition : SV_POSITION;
    float3 WorldPosition : WORLD_POS;
    float3 WorldNormal   : WORLD_NORMAL;
};

cbuffer cbCameraAttribs
{
    CameraAttribs g_CurrCamera;
}

cbuffer cbObjectAttribs
{
    ObjectAttribs g_ObjectAttribs;
}

cbuffer cbABufferConstants
{
    ABufferConstants g_ABufferConstants;
}

RWByteAddressBuffer                 g_BufferHeadPointers;
RWStructuredBuffer<ABufferFragment> g_BufferNodes;
RWStructuredBuffer<uint>            g_BufferCounter;

void VSMain(VSInput VSIn, out PSInput VSOut)
{
    float4 WorldPos = mul(float4(VSIn.Position, 1.0), g_ObjectAttribs.CurrWorldMatrix);
    VSOut.PixelPosition = mul(float4(VSIn.Position, 1.0), g_ObjectAttribs.CurrWorldViewProjectMatrix);
    VSOut.WorldPosition = WorldPos.xyz;
    VSOut.WorldNormal   = normalize(mul(float4(VSIn.Normal, 0.0), g_ObjectAttribs.CurrNormalMatrix).xyz);
}

void PSMain(PSInput VSOut, bool IsFrontFace : SV_IsFrontFace)
{
    // Determine entry/exit from face orientation
    uint FaceType = IsFrontFace ? ABUFFER_FACE_ENTRY : ABUFFER_FACE_EXIT;

    uint MaterialIdx = g_ObjectAttribs.MaterialIdx;

    // Store outward-facing normal (vertex normal always points outward from the mesh surface).
    // ResolveCSG will flip exit normals to face the camera for lighting.
    float3 Normal = normalize(VSOut.WorldNormal);

    // Allocate one A-Buffer fragment
    uint NodeIndex;
    InterlockedAdd(g_BufferCounter[0], 1u, NodeIndex);

    if (NodeIndex >= g_ABufferConstants.MaxNodeCount)
        discard;

    uint2 PixelCoord = uint2(VSOut.PixelPosition.xy);
    uint  HeadOffset  = (PixelCoord.y * g_ABufferConstants.ScreenSize.x + PixelCoord.x) * 4u;

    ABufferFragment Node;
    Node.PackedData = PackFragmentData(Normal, MaterialIdx, g_ObjectAttribs.ObjectID, FaceType);
    Node.Depth      = VSOut.PixelPosition.z;

    uint OldHead;
    g_BufferHeadPointers.InterlockedExchange(HeadOffset, NodeIndex, OldHead);
    Node.Next = OldHead;

    g_BufferNodes[NodeIndex] = Node;
}
