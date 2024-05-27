#include "FullScreenTriangleVSOutput.fxh"
#include "SRGBUtilities.fxh"

Texture2D<float3> g_TextureColor;
SamplerState      g_TextureColor_sampler;

float4 GammaCorrectionPS(FullScreenTriangleVSOutput VSOut) : SV_Target0
{
    float2 Texcoord = NormalizedDeviceXYToTexUV(VSOut.f2NormalizedXY.xy);
    float3 GammaColor = LinearToSRGB(g_TextureColor.SampleLevel(g_TextureColor_sampler, Texcoord, 0.0));
    return float4(GammaColor, 1.0);
}
