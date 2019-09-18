//  ---------------------------------------------------------------------------
//
//  @file       TextPS.hlsl
//  @author     Egor Yusov
//	@brief		Source code of the TextPS D3D pixel shader
//  @license    This file is based on the TwDirect3D11.hlsl file 
//              from the original AntTweakBar library.
//
//  ---------------------------------------------------------------------------


 
Texture2D    g_Font;
SamplerState g_Font_sampler;

struct TextPSInput 
{ 
    float4 Pos   : SV_POSITION; 
    float4 Color : COLOR0; 
    float2 Tex   : TEXCOORD0; 
};
   
float4 main(TextPSInput input) : SV_TARGET
{ 
    return g_Font.Sample(g_Font_sampler, input.Tex) * input.Color; 
}
