//  ---------------------------------------------------------------------------
//
//  @file       LineRectPS.hlsl
//  @author     Egor Yusov
//	@brief		Source code of the LineRectPS D3D pixel shader
//  @license    This file is based on the TwDirect3D11.hlsl file 
//              from the original AntTweakBar library.
//
//  ---------------------------------------------------------------------------
 
struct LineRectPSInput 
{ 
    float4 Pos : SV_POSITION; 
    float4 Color : COLOR0; 
};

float4 main(LineRectPSInput input) : SV_TARGET
{ 
    return input.Color; 
}
