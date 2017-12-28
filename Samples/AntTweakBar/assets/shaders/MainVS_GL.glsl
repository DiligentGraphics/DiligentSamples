
uniform Constants
{
    mat4 WorldViewProj;
	mat4 WorldNorm;
    vec3 LightDir;
    float LightCoeff;
}g_Constants;


layout(location = 0) in  vec3 in_Position;
layout(location = 1) in  vec3 in_Normal;
layout(location = 2) in  vec4 in_Color;

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

layout(location = 0) out vec4 out_Color;

vec4 mul( in vec4 v, in mat4 m )
{
    return v*m;
}


void main() 
{
    gl_Position = mul( vec4(in_Position.xyz, 1.f), g_Constants.WorldViewProj);
    vec3 n = normalize(mul(vec4(in_Normal, 0.f), g_Constants.WorldNorm).xyz);
    out_Color.rgb = in_Color.rgb * ((1.f - g_Constants.LightCoeff) + g_Constants.LightCoeff * dot(n, -g_Constants.LightDir));
    out_Color.a  = in_Color.a;
}
