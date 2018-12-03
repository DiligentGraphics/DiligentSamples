
function GetShaderPath( ShaderName )
	
	local ProcessedShaderPath = ""
	if Constants.DeviceType == "D3D11" or Constants.DeviceType == "D3D12" then
		ProcessedShaderPath = "shaders\\" .. ShaderName .. "_DX.hlsl"
	else
		ProcessedShaderPath = "shaders\\" .. ShaderName .. "_GL.glsl"
	end

	return ProcessedShaderPath
end

MainVS = Shader.Create{
	FilePath =  GetShaderPath("MainVS" ),
    UseCombinedTextureSamplers = true,
	Desc = { ShaderType = "SHADER_TYPE_VERTEX" }
}

MainPS = Shader.Create{
	FilePath =  GetShaderPath("MainPS" ),
    UseCombinedTextureSamplers = true,
	Desc = { ShaderType = "SHADER_TYPE_PIXEL" }
}

PSO = PipelineState.Create
{
	Name = "Menger Sponge PSO",
	GraphicsPipeline = 
	{
		DepthStencilDesc = 
		{
			DepthEnable = true
		},
		RasterizerDesc = 
		{
			CullMode = "CULL_MODE_NONE"
		},
		BlendDesc = 
		{
			IndependentBlendEnable = false,
			RenderTargets = { {BlendEnable = false} }
		},
		InputLayout = 
		{
			{ InputIndex = 0, NumComponents = 3, ValueType = "VT_FLOAT32"},
			{ InputIndex = 1, NumComponents = 3, ValueType = "VT_FLOAT32"},
			{ InputIndex = 2, NumComponents = 4, ValueType = "VT_UINT8", IsNormalized = true}
		},
		PrimitiveTopology = "PRIMITIVE_TOPOLOGY_TRIANGLE_LIST",
		pVS = MainVS,
		pPS = MainPS,
		RTVFormats = extBackBufferFormat,
		DSVFormat = extDepthBufferFormat
	}
}

ResMapping = ResourceMapping.Create{
	{Name = "Constants", pObject = extConstantBuffer}
}

MainVS:BindResources(ResMapping)
MainPS:BindResources(ResMapping)

SRB = PSO:CreateShaderResourceBinding(true)

DrawAttrs = DrawAttribs.Create{
	IsIndexed = true,
	IndexType = "VT_UINT32",
    Flags = {"DRAW_FLAG_TRANSITION_VERTEX_BUFFERS", "DRAW_FLAG_TRANSITION_INDEX_BUFFER"}
}

function SetNumIndices(NumIndices)
	DrawAttrs.NumIndices = NumIndices
end

function Draw()
	Context.SetPipelineState(PSO)
	SRB:BindResources({"SHADER_TYPE_VERTEX", "SHADER_TYPE_PIXEL"}, ResMapping, {"BIND_SHADER_RESOURCES_KEEP_EXISTING", "BIND_SHADER_RESOURCES_VERIFY_ALL_RESOLVED"})
	Context.CommitShaderResources(SRB, "RESOURCE_STATE_TRANSITION_MODE_TRANSITION")
	Context.SetVertexBuffers(extSpongeVB, "SET_VERTEX_BUFFERS_FLAG_RESET")
	Context.SetIndexBuffer(extSpongeIB)
	
	Context.Draw(DrawAttrs)
end
