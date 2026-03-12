#include "BasicStructures.fxh"
#include "CSGHelpers.hlsli"

struct VSInput
{
    float3 Position : ATTRIB0;
};

struct PSInput
{
    float4 PixelPosition : SV_POSITION;
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

#include "RayIntersection.hlsli"

void VSMain(VSInput VSIn, out PSInput VSOut)
{
    VSOut.PixelPosition = mul(float4(VSIn.Position, 1.0), g_ObjectAttribs.CurrWorldViewProjectMatrix);
}

void PSMain(PSInput VSOut)
{
    float2 NormalizedXY = TexUVToNormalizedDeviceXY(VSOut.PixelPosition.xy * g_CurrCamera.f4ViewportSize.zw);
    Ray RayWS = CreateCameraRay(NormalizedXY);
    Ray RayLS = TransformRayToLocalSpace(RayWS);

    IntersectionInterval Interval = IntersectGeometryInterval(RayLS, g_ObjectAttribs.ObjectType);

    if (!Interval.Hit)
        discard;

    // Compute world-space positions and depths for entry and exit
    float3 EntryPosWS = RayWS.Origin + Interval.TNear * RayWS.Direction;
    float3 ExitPosWS  = RayWS.Origin + Interval.TFar  * RayWS.Direction;

    float EntryDepth = ComputeDepth(EntryPosWS);
    float ExitDepth  = ComputeDepth(ExitPosWS);

    bool EntryVisible = (Interval.TNear > M_EPSILON);
    bool ExitVisible  = (Interval.TFar  > M_EPSILON);

    if (!any(bool2(EntryVisible, ExitVisible)))
        discard;

    uint MaterialIdx = g_ObjectAttribs.MaterialIdx;

    // Count how many nodes we need
    uint NodeCount = (EntryVisible ? 1u : 0u) + (ExitVisible ? 1u : 0u);

    // Allocate nodes atomically
    uint FirstNodeIndex;
    InterlockedAdd(g_BufferCounter[0], NodeCount, FirstNodeIndex);

    if (FirstNodeIndex + NodeCount > g_ABufferConstants.MaxNodeCount)
        discard;

    uint2 PixelCoord = uint2(VSOut.PixelPosition.xy);
    uint  HeadOffset  = (PixelCoord.y * g_ABufferConstants.ScreenSize.x + PixelCoord.x) * 4u;
    uint  CurrNodeIdx = FirstNodeIndex;

    // Insert entry node (front face)
    if (EntryVisible)
    {
        float3 NormalWS = ComputeNormal(Interval.NormalNear);

        ABufferFragment EntryNode;
        EntryNode.PackedData = PackFragmentData(NormalWS, MaterialIdx, g_ObjectAttribs.ObjectID, ABUFFER_FACE_ENTRY);
        EntryNode.Depth      = EntryDepth;

        uint OldHead;
        g_BufferHeadPointers.InterlockedExchange(HeadOffset, CurrNodeIdx, OldHead);
        EntryNode.Next = OldHead;

        g_BufferNodes[CurrNodeIdx] = EntryNode;
        CurrNodeIdx++;
    }

    // Insert exit node (back face)
    if (ExitVisible)
    {
        float3 NormalWS = ComputeNormal(Interval.NormalFar);

        ABufferFragment ExitNode;
        ExitNode.PackedData = PackFragmentData(NormalWS, MaterialIdx, g_ObjectAttribs.ObjectID, ABUFFER_FACE_EXIT);
        ExitNode.Depth      = ExitDepth;

        uint OldHead;
        g_BufferHeadPointers.InterlockedExchange(HeadOffset, CurrNodeIdx, OldHead);
        ExitNode.Next = OldHead;

        g_BufferNodes[CurrNodeIdx] = ExitNode;
        CurrNodeIdx++;
    }
}
