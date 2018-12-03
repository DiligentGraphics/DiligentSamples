# Tutorial03 - Texturing

This tutorial extends Tutorial02 and demonstrates how to apply a texture to a 3D object. It shows how to load a texture 
from file, create shader resource binding object and how to sample a texture in the shader.

![](Screenshot.png)

## Shaders

The vertex shader is very similar to the one we used in Tutorial02. The only difference is that instead of color,
the shader inputs texture UV coordinates and passes them to the pixel shader:

```hlsl
cbuffer Constants
{
    float4x4 g_WorldViewProj;
};

struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float2 uv : TEX_COORD; 
};

PSInput main(float3 pos : ATTRIB0, 
             float2 uv : ATTRIB1) 
{
    PSInput ps; 
    ps.Pos = mul( float4(pos,1.0), g_WorldViewProj);
    ps.uv = uv;
    return ps;
}
```

Pixel shader defines a 2D texture named `g_Texture` and a texture sampler named `g_Texture_sampler`. It samples
the texture using UV coordinates from the vertex shader and writes the resulting color to the render target:

```hlsl
Texture2D g_Texture;
SamplerState g_Texture_sampler;

struct PSInput 
{ 
    float4 Pos : SV_POSITION; 
    float2 uv : TEX_COORD; 
};

float4 main(PSInput ps_in) : SV_TARGET
{
    return g_Texture.Sample(g_Texture_sampler, ps_in.uv); 
}
```

Diligent Engine attaches samplers to shader resource views. In the shader,
**the sampler that is used to sample a texture must be named `<Texture Name>_sampler`**, otherwise
the engine will fail to properly set the sampler. This limitation is necessary to translate HLSL shaders 
to GLSL.

## Initializing the Pipeline State

Pipeline state initialization is in fact the same as in Tutorial02. What is worth discussion is 
creating the pixel shader:

```cpp
RefCntAutoPtr<IShader> pPS;
       
CreationAttribs.Desc.ShaderType = SHADER_TYPE_PIXEL;
CreationAttribs.EntryPoint = "main";
CreationAttribs.Desc.Name = "Cube PS";
CreationAttribs.FilePath = "cube.psh";
// Shader variables should typically be mutable, which means they are expected
// to change on a per-instance basis
ShaderVariableDesc Vars[] = 
{
    {"g_Texture", SHADER_VARIABLE_TYPE_MUTABLE}
};
CreationAttribs.Desc.VariableDesc = Vars;
CreationAttribs.Desc.NumVariables = _countof(Vars);

// Define static sampler for g_Texture. Static samplers should be used whenever possible
SamplerDesc SamLinearClampDesc( FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, 
                                TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP);
StaticSamplerDesc StaticSamplers[] = 
{
    {"g_Texture", SamLinearClampDesc}
};
CreationAttribs.Desc.StaticSamplers = StaticSamplers;
CreationAttribs.Desc.NumStaticSamplers = _countof(StaticSamplers);

pDevice->CreateShader(CreationAttribs, &pPS);
```

This time our pixel shader uses a resource that needs to be set by the application - 
a texture variable `g_Texture`. For this example, we want it to be a mutable variable.
Mutable resources are set through the object called *shader resource binding*, or SRB. SRB
encompasses all resources specific to an object instance. As an example, if your character shader uses 
a shadow map, diffuse and normal maps, then shadow map should typically be static resource as
it is the same for all object instances, while normal and diffuse maps should be mutable as
they are character-specific.

Shader variable type can be specified with `ShaderVariableDesc` structure:

```cpp
ShaderVariableDesc Vars[] = 
{
    {"g_Texture", SHADER_VARIABLE_TYPE_MUTABLE}
};
CreationAttribs.Desc.VariableDesc = Vars;
CreationAttribs.Desc.NumVariables = _countof(Vars);
```

Recall that if shader variable type is not specified, default type defined by `DefaultVariableType`
is used.

As it was mentioned earlier, a sampler can be attached to a texture view at run time. However, in 
most cases samplers do not change dynamically and it is known beforehand what kind of
sampling is required. Diligent Engine uses *static samplers* that can be specified during shader
compilation stage:

```cpp
SamplerDesc SamLinearClampDesc( FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, FILTER_TYPE_LINEAR, 
                                TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP);
StaticSamplerDesc StaticSamplers[] = 
{
    {"g_Texture", SamLinearClampDesc}
};
CreationAttribs.Desc.StaticSamplers = StaticSamplers;
CreationAttribs.Desc.NumStaticSamplers = _countof(StaticSamplers);
```

If static sampler is specified for a texture, the sampler set in the shader resource view is ignored.
Static samplers are preferred from performance point of view and should be used whenever possible.

## Loading a texture

Diligent Engine provides cross-platform texture loading library that supports jpg, png, tiff and dds formats.
Loading a texture with this library requries just few lines of code:

```cpp
TextureLoadInfo loadInfo;
loadInfo.IsSRGB = true;
RefCntAutoPtr<ITexture> Tex;
CreateTextureFromFile("DGLogo.png", loadInfo, m_pDevice, &Tex);
```

To bound a texture to the shader, we need to use the texture's shader resource view. Default SRV
addresses the entire texture:

```cpp
// Get shader resource view from the texture
m_TextureSRV = Tex->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
```

## Shader Resource Binding

Shader resource binding object is created by the pipeline state:

```cpp
m_pPSO->CreateShaderResourceBinding(&m_SRB);
```

Once SRB is created, we can set shader resource view:

```cpp
m_SRB->GetVariable(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);
```

A mutable resource can only be bound once to a SRB object. If multiple variations are required,
a number of SRB objects should be created. For a very frequently changning resource, dynamic
variables can be used. Dynamic resoruces can be bound multiple times to the same SRB object,
but are less efficient from performance point of view. Refer to 
[this post](http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/)
for more details.
Do not confuse shader variable type with resource usage. Shader variable type is all about
binding resources to a shader. It has nothing to do with the ability to change resource contents which
is controlled by the resource usage

## Vertex and Index Buffers

Initializing vertex and index buffers is very similar to that of Tutorial02. The only difference
is that cube vertices cannot be shared between faces as texture UV coordinates need to be different.

# Rendering

Render function is also very similar to that of Tutorial02. This time however we have a SRB
object that encompasses resources to be committed:

```cpp
m_pImmediateContext->SetPipelineState(m_pPSO);
m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
```

Using shader resource binding object allows to efficiently commit all resources required by all shaders
in the pipeline state.
