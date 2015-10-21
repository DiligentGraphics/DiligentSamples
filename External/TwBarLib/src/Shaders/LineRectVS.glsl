//  ---------------------------------------------------------------------------
//
//  @file       LineRectVS.glsl
//  @author     Egor Yusov
//	@brief		Source code of the LineRectVS OpenGL vertex shader
//
//  ---------------------------------------------------------------------------

 
uniform Constants
{
    vec4 g_Offset;
    vec4 g_CstColor;
};

// Shaders for lines and rectangles

layout( location = 0 ) in vec4 in_pos;
layout( location = 1 ) in vec4 in_color;

//To use any built-in input or output in the gl_PerVertex and
//gl_PerFragment blocks in separable program objects, shader code must
//redeclare those blocks prior to use. 
//
// Declaring this block causes compilation error on NVidia GLES
#ifndef GL_ES
out gl_PerVertex
{
	vec4 gl_Position;
};
#endif

layout(location = 1) out vec4 Color;

void main() 
{
    gl_Position = vec4(in_pos.xy + g_Offset.xy, 0.5, 1.0);
    Color = in_color; 
}
