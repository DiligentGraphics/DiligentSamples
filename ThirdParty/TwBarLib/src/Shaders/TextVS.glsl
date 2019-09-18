//  ---------------------------------------------------------------------------
//
//  @file       TextVS.glsl
//  @author     Egor Yusov
//	@brief		Source code of the TextVS OpenGL vertex shader
//
//  ---------------------------------------------------------------------------
 
uniform Constants
{
    vec4 g_Offset;
    vec4 g_CstColor;
};

// Shaders for text

layout( location = 0 ) in vec4 in_pos;
layout( location = 1 ) in vec4 in_color;
layout( location = 2 ) in vec2 in_tex;

#if (defined(GL_ES) && __VERSION__ <= 300)
    // GLES3.0 only supports layout location qualifiers for VS inputs
    #define OUT_LOCATION(x)
#else
    // Compiling glsl to SPIRV with glslang will fail if locations are not specified
    #define OUT_LOCATION(x) layout(location = x)
#endif

OUT_LOCATION(0) out vec4 Color;
OUT_LOCATION(1) out vec2 Tex;

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

void main()
{
    gl_Position = vec4(in_pos.xy + g_Offset.xy, 0.5, 1.0);
    Color = in_color; 
    Tex = in_tex; 
}
