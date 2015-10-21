
function GetShaderPath( ShaderName )
	
	local ProcessedShaderPath = ""
	if Constants.DeviceType == "DirectX" then
		ProcessedShaderPath = "shaders\\" .. ShaderName .. "_DX.hlsl"
	else
		ProcessedShaderPath = "shaders\\" .. ShaderName .. "_GL.glsl"
	end

	return ProcessedShaderPath
end

MainVS = Shader.Create{
	FilePath =  GetShaderPath("MainVS" ),
	Desc = { ShaderType = "SHADER_TYPE_VERTEX" }
}

MainPS = Shader.Create{
	FilePath =  GetShaderPath("MainPS" ),
	Desc = { ShaderType = "SHADER_TYPE_PIXEL" }
}

VertexLayout = LayoutDesc.Create({
    LayoutElements = {
        { InputIndex = 0, NumComponents = 3, ValueType = "VT_FLOAT32"},
        { InputIndex = 1, NumComponents = 3, ValueType = "VT_FLOAT32"},
		{ InputIndex = 2, NumComponents = 4, ValueType = "VT_UINT8", IsNormalized = true}
    }
}, MainVS)


EnableDepth = DepthStencilState.Create
{
	DepthEnable = true
}

SolidFillNoCull = RasterizerState.Create
{
	CullMode = "CULL_MODE_NONE"
}

DefaultBlend = BlendState.Create
{
	IndependentBlendEnable = false,
	RenderTargets = { {BlendEnable = false} }
}

ResMapping = ResourceMapping.Create{
	{Name = "Constants", pObject = extConstantBuffer}
}

MainVS:BindResources(ResMapping)
MainPS:BindResources(ResMapping)

DrawAttrs = DrawAttribs.Create{
    Topology = "PRIMITIVE_TOPOLOGY_TRIANGLE_LIST",
	IsIndexed = true,
	IndexType = "VT_UINT32"
}

function SetNumIndices(NumIndices)
	DrawAttrs.NumIndices = NumIndices
end

function Draw()
	Context.SetDepthStencilState(EnableDepth)
	Context.SetRasterizerState(SolidFillNoCull)
	Context.SetBlendState(DefaultBlend)

	Context.SetShaders(MainVS, MainPS)
	Context.BindShaderResources(ResMapping, {"BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED",  "BIND_SHADER_RESOURCES_ALL_RESOLVED"})
	Context.SetVertexBuffers(extSpongeVB, "SET_VERTEX_BUFFERS_FLAG_RESET")
	Context.SetIndexBuffer(extSpongeIB)
	Context.SetInputLayout(VertexLayout)
	Context.Draw(DrawAttrs)
end
