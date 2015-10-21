//  ---------------------------------------------------------------------------
//
//  @file       LineRectVS.hlsl
//  @author     Egor Yusov
//	@brief		Source code of the LineRectVS D3D vertex shader
//  @license    This file is based on the TwDirect3D11.hlsl file 
//              from the original AntTweakBar library.
//
//  ---------------------------------------------------------------------------

 
cbuffer Constants
{
    float4 g_Offset;
    float4 g_CstColor;
}

// Shaders for lines and rectangles

struct LineRectPSInput 
{ 
    float4 Pos : SV_POSITION; 
    float4 Color : COLOR0; 
};

LineRectPSInput main(float4 pos : ATTRIB0, float4 color : ATTRIB1) 
{
    LineRectPSInput ps; 
    ps.Pos = float4(pos.xy + g_Offset.xy, 0.5, 1.0);
    ps.Color = color; 
    return ps; 
}
