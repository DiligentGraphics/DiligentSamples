#include "BasicStructures.fxh"

cbuffer cbCameraAttribs
{
    CameraAttribs g_CameraAttribs;
};

struct VSInput
{
    float3 Position  : ATTRIB0;
    float3 Normal    : ATTRIB1;
    float2 TexCoord  : ATTRIB2;
};

struct VSOutput
{
    float4 PositionPS 	: SV_Position;
    float3 PositionWS 	: POSITIONWS;
    float3 NormalWS 	: NORMALWS;
    float2 TexCoord 	: TEXCOORD;
    float DepthVS		: DEPTHVS;
};

VSOutput MeshVS(in VSInput input)
{
    VSOutput VSOut;

    // Calc the world-space position
    VSOut.PositionWS = input.Position;// mul(float4(input.PositionOS, 1.0f), g_CameraAttribs.).xyz;

    // Calc the clip-space position
    VSOut.PositionPS = mul(float4(VSOut.PositionWS, 1.0), g_CameraAttribs.mViewProj);

    VSOut.DepthVS = VSOut.PositionPS.w;

    // Rotate the normal into world space
    VSOut.NormalWS = float3(0.0, 0.0, 1.0);//normalize(mul(input.NormalOS, (float3x3)World));

    // Pass along the texture coordinate
    VSOut.TexCoord = input.TexCoord;

    return VSOut;
}
