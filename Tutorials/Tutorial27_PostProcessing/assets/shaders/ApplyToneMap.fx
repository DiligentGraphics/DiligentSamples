#include "FullScreenTriangleVSOutput.fxh"
#include "PBR_Structures.fxh"
#include "ToneMapping.fxh"
#include "SRGBUtilities.fxh"

cbuffer cbPBRRendererAttibs
{
    PBRRendererShaderParameters g_PBRRendererAttibs;
}

Texture2D<float3> g_TextureHDR;

float4 ApplyToneMapPS(FullScreenTriangleVSOutput VSOut) : SV_Target0
{
    ToneMappingAttribs TMAttribs;
    TMAttribs.iToneMappingMode = TONE_MAPPING_MODE;
    TMAttribs.bAutoExposure = false;
    TMAttribs.fMiddleGray = g_PBRRendererAttibs.MiddleGray;
    TMAttribs.bLightAdaptation = false;
    TMAttribs.fWhitePoint = g_PBRRendererAttibs.WhitePoint;
    TMAttribs.fLuminanceSaturation = 1.0;

    float3 HDRColor = g_TextureHDR.Load(int3(VSOut.f4PixelPos.xy, 0));
    float3 SDRColor = ToneMap(HDRColor, TMAttribs, g_PBRRendererAttibs.AverageLogLum);

#if CONVERT_OUTPUT_TO_SRGB
    SDRColor = LinearToSRGB(SDRColor);
#endif
    return float4(SDRColor, 1.0);
}
