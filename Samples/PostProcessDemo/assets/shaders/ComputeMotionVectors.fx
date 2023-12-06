#include "BasicStructures.fxh"

cbuffer cbCameraAttribs
{
    CameraAttribs g_CameraAttibs;
}

Texture2D<float> g_InputDepth;

float2 ScreenSpaceToNDC(float2 Location, float2 InvDimension)
{
    float2 NDC = 2.0f * Location.xy * InvDimension - 1.0f;
    return float2(NDC.x, -NDC.y);
}

bool IsBackground(float depth)
{
#if SSR_OPTION_INVERTED_DEPTH
    return depth < 1.e-6f;
#else
    return depth >= (1.0f - 1.e-6f);
#endif
}

float2 ComputeMotionVectorsPS(in float4 Position : SV_Position) : SV_Target
{
    float Depth = g_InputDepth.Load(int3(Position.xy, 0.0));
        
    float2 CurrNDC = ScreenSpaceToNDC(Position.xy, g_CameraAttibs.f4ViewportSize.zw);
    float4 PositionWS = mul(float4(CurrNDC, Depth, 1.0f), g_CameraAttibs.mViewProjInv);
    PositionWS /= PositionWS.w;
      
    float4 PrevNDC = mul(PositionWS, g_CameraAttibs.mPrevViewProj);
    PrevNDC = PrevNDC / PrevNDC.w;
             
    float2 Motion = -0.5 * (CurrNDC.xy - PrevNDC.xy);
#if !defined(DESKTOP_GL) && !defined(GL_ES)
    Motion.y = -Motion.y;
#endif
    return !IsBackground(Depth) ? Motion : float2(0.0, 0.0);
}
