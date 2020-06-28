#include "BasicStructures.fxh"

cbuffer cbCameraAttribs
{
    CameraAttribs g_CameraAttribs;
}

float4 BoundBoxVS(uint id : SV_VertexID) : SV_Position
{
    float3 BoundBoxMinXYZ = float3(0.0, 0.0, 0.0);
    float3 BoundBoxMaxXYZ = float3(1.0, 1.0, 1.0);
    
    float4x4 BBTransform = MatrixFromRows(
        g_CameraAttribs.f4ExtraData[0],
        g_CameraAttribs.f4ExtraData[1],
        g_CameraAttribs.f4ExtraData[2],
        g_CameraAttribs.f4ExtraData[3]);

    BBTransform = mul(BBTransform, g_CameraAttribs.mViewProj);

    float4 BoxCorners[8]=
    {
        float4(BoundBoxMinXYZ.x, BoundBoxMinXYZ.y, BoundBoxMinXYZ.z, 1.0),
        float4(BoundBoxMinXYZ.x, BoundBoxMaxXYZ.y, BoundBoxMinXYZ.z, 1.0),
        float4(BoundBoxMaxXYZ.x, BoundBoxMaxXYZ.y, BoundBoxMinXYZ.z, 1.0),
        float4(BoundBoxMaxXYZ.x, BoundBoxMinXYZ.y, BoundBoxMinXYZ.z, 1.0),

        float4(BoundBoxMinXYZ.x, BoundBoxMinXYZ.y, BoundBoxMaxXYZ.z, 1.0),
        float4(BoundBoxMinXYZ.x, BoundBoxMaxXYZ.y, BoundBoxMaxXYZ.z, 1.0),
        float4(BoundBoxMaxXYZ.x, BoundBoxMaxXYZ.y, BoundBoxMaxXYZ.z, 1.0),
        float4(BoundBoxMaxXYZ.x, BoundBoxMinXYZ.y, BoundBoxMaxXYZ.z, 1.0),
    };

    const int RibIndices[12*2] = {0,1, 1,2, 2,3, 3,0,
                                  4,5, 5,6, 6,7, 7,4,
                                  0,4, 1,5, 2,6, 3,7};
    return mul(BoxCorners[RibIndices[id]], BBTransform);
}
