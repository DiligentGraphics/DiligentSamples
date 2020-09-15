#version 430

layout(location=0) out int out_VertexID;

void main()
{
#ifdef VULKAN
	out_VertexID = gl_VertexIndex;
#else
	out_VertexID = gl_VertexID;
#endif
}
