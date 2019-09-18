//  ---------------------------------------------------------------------------
//
//  @file       TextVS.hlsl
//  @author     Egor Yusov
//	@brief		Source code of the TextVS D3D vertex shader
//  @license    This file is based on the TextCstColorVS.hlsl file 
//              from the original AntTweakBar library.
//
//  ---------------------------------------------------------------------------
 
cbuffer Constants
{
    float4 g_Offset;
    float4 g_CstColor;
}

// Shaders for text

struct TextPSInput 
{ 
    float4 Pos : SV_POSITION; 
    float4 Color : COLOR0; 
    float2 Tex : TEXCOORD0; 
};

TextPSInput main(float4 pos : ATTRIB0, float4 color : ATTRIB1, float2 tex : ATTRIB2)
{
    TextPSInput ps; 
    ps.Pos = float4(pos.xy + g_Offset.xy, 0.5, 1);
    ps.Color = color; 
    ps.Tex = tex; 
    return ps; 
}

