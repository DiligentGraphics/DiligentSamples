//  ---------------------------------------------------------------------------
//
//  @file       TextPS.glsl
//  @author     Egor Yusov
//	@brief		Source code of the TextPS OpenGL pixel shader
//
//  ---------------------------------------------------------------------------

uniform sampler2D g_Font;

layout(location = 1) in vec4 Color;
layout(location = 2) in vec2 Tex;

out vec4 out_Color;

void main()
{ 
    out_Color = texture(g_Font, Tex) * Color; 
}
