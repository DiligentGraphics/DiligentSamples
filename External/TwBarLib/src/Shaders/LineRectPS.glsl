//  ---------------------------------------------------------------------------
//
//  @file       LineRectPS.glsl
//  @author     Egor Yusov
//	@brief		Source code of the LineRectPS OpenGL pixel shader
//
//  ---------------------------------------------------------------------------

#if (defined(GL_ES) && __VERSION__ <= 300)
    // GLES3.0 only supports layout location qualifiers for FS outputs
    #define IN_LOCATION(x)
#else
    // Compiling glsl to SPIRV with glslang will fail if locations are not specified
    #define IN_LOCATION(x) layout(location = x)
#endif

IN_LOCATION(0) in vec4 Color;

layout(location = 0) out vec4 out_Color;

void main()
{ 
    out_Color = Color; 
}
