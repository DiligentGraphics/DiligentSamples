-- Terrain.lua

-- Externally defined variables:
-- extResourceMapping


PointClampSampler = Sampler.Create{
    Name = "Terrain.lua: point clamp sampler",
    MinFilter = "FILTER_TYPE_POINT", 
    MagFilter = "FILTER_TYPE_POINT", 
    MipFilter = "FILTER_TYPE_POINT", 
    AddressU = "TEXTURE_ADDRESS_CLAMP", 
    AddressV = "TEXTURE_ADDRESS_CLAMP", 
    AddressW = "TEXTURE_ADDRESS_CLAMP"
}

LinearClampSampler = Sampler.Create{
    Name = "Terrain.lua: linear clamp sampler",
    MinFilter = "FILTER_TYPE_LINEAR", 
    MagFilter = "FILTER_TYPE_LINEAR", 
    MipFilter = "FILTER_TYPE_LINEAR", 
    AddressU = "TEXTURE_ADDRESS_CLAMP", 
    AddressV = "TEXTURE_ADDRESS_CLAMP", 
    AddressW = "TEXTURE_ADDRESS_CLAMP"
}

LinearWrapSampler = Sampler.Create{
    Name = "Terrain.lua: linear wrap sampler",
    MinFilter = "FILTER_TYPE_LINEAR", 
    MagFilter = "FILTER_TYPE_LINEAR", 
    MipFilter = "FILTER_TYPE_LINEAR", 
    AddressU = "TEXTURE_ADDRESS_WRAP", 
    AddressV = "TEXTURE_ADDRESS_WRAP", 
    AddressW = "TEXTURE_ADDRESS_WRAP"
}

ComparisonSampler = Sampler.Create{
    Name = "Terrain.lua: comparison sampler",
    MinFilter = "FILTER_TYPE_COMPARISON_LINEAR", 
    MagFilter = "FILTER_TYPE_COMPARISON_LINEAR", 
    MipFilter = "FILTER_TYPE_COMPARISON_LINEAR", 
    AddressU = "TEXTURE_ADDRESS_CLAMP", 
    AddressV = "TEXTURE_ADDRESS_CLAMP", 
    AddressW = "TEXTURE_ADDRESS_CLAMP",
	ComparisonFunc = "COMPARISON_FUNC_LESS",
}

-- Shaders

function CreateHemisphereShaders()

	HemisphereVS = Shader.Create{
		FilePath =  "HemisphereVS.fx",
		EntryPoint = "HemisphereVS",
		SearchDirectories = "shaders;shaders\\terrain",
		SourceLanguage = "SHADER_SOURCE_LANGUAGE_HLSL",
        UseCombinedTextureSamplers = true,
		Desc = {
			ShaderType = "SHADER_TYPE_VERTEX",
			Name = "HemisphereVS",
			DefaultVariableType = "SHADER_VARIABLE_TYPE_STATIC",
			VariableDesc = 
			{ 
				{Name = "cbCameraAttribs", Type = "SHADER_VARIABLE_TYPE_MUTABLE"}, 
				{Name = "cbLightAttribs", Type = "SHADER_VARIABLE_TYPE_MUTABLE"}, 
				{Name = "cbTerrainAttribs", Type = "SHADER_VARIABLE_TYPE_MUTABLE"},
				{Name = "cbParticipatingMediaScatteringParams", Type = "SHADER_VARIABLE_TYPE_STATIC"},
				{Name = "g_tex2DOccludedNetDensityToAtmTop", Type = "SHADER_VARIABLE_TYPE_DYNAMIC"},
				{Name = "g_tex2DAmbientSkylight", Type = "SHADER_VARIABLE_TYPE_DYNAMIC"} 
			}
		}
	}

	assert(HemisphereVS.Desc.VariableDesc[1].Name == "cbCameraAttribs")
	assert(HemisphereVS.Desc.VariableDesc[1].Type == "SHADER_VARIABLE_TYPE_MUTABLE")

	assert(HemisphereVS.Desc.VariableDesc[4].Name == "cbParticipatingMediaScatteringParams")
	assert(HemisphereVS.Desc.VariableDesc[4].Type == "SHADER_VARIABLE_TYPE_STATIC")

	assert(HemisphereVS.Desc.VariableDesc[6].Name == "g_tex2DAmbientSkylight")
	assert(HemisphereVS.Desc.VariableDesc[6].Type == "SHADER_VARIABLE_TYPE_DYNAMIC")

	HemisphereZOnlyVS = Shader.Create{
		FilePath =  "HemisphereZOnlyVS.fx",
		EntryPoint = "HemisphereZOnlyVS",
		SourceLanguage = "SHADER_SOURCE_LANGUAGE_HLSL",
        UseCombinedTextureSamplers = true,
		SearchDirectories = "shaders\\;shaders\\terrain",
		Desc = {
			ShaderType = "SHADER_TYPE_VERTEX",
			Name = "HemisphereZOnlyVS",
			DefaultVariableType = "SHADER_VARIABLE_TYPE_STATIC",
			VariableDesc = 
			{ 
				{Name = "cbCameraAttribs", Type = "SHADER_VARIABLE_TYPE_STATIC"}, 
			}
		}
	}

	-- extResourceMapping is set by the app
	HemisphereVS:BindResources(extResourceMapping)
	HemisphereZOnlyVS:BindResources(extResourceMapping)
end

function SetHemispherePS(in_HemispherePS, RTVFormat)
	HemispherePS = in_HemispherePS
	HemispherePS:BindResources(extResourceMapping)
	
	InputLayoutElements = 
	{
			{ InputIndex = 0, BufferSlot = 0, NumComponents = 3, ValueType = "VT_FLOAT32"},
			{ InputIndex = 1, BufferSlot = 0, NumComponents = 2, ValueType = "VT_FLOAT32"}
	}

	RenderHemispherePSO = PipelineState.Create
	{
		Name = "RenderHemisphere",
		GraphicsPipeline = 
		{
			DepthStencilDesc = 
			{
				DepthEnable = true,
				DepthWriteEnable = true,
				DepthFunc = "COMPARISON_FUNC_LESS"
			},
			RasterizerDesc = 
			{
				FillMode = "FILL_MODE_SOLID",
				--FillMode = "FILL_MODE_WIREFRAME",
				CullMode = "CULL_MODE_BACK",
				FrontCounterClockwise = true
			},
			InputLayout = InputLayoutElements,
			pVS = HemisphereVS,
			pPS = HemispherePS,
			RTVFormats = RTVFormat,
			DSVFormat = "TEX_FORMAT_D32_FLOAT",
            PrimitiveTopology = "PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP"
		}
	}
	RenderHemisphereSRB = RenderHemispherePSO:CreateShaderResourceBinding()
	RenderHemisphereSRB:BindResources("SHADER_TYPE_VERTEX", extResourceMapping, "BIND_SHADER_RESOURCES_KEEP_EXISTING")

	RenderHemisphereZOnlyPSO = PipelineState.Create
	{
		Name = "Render Hemisphere Z Only",
		GraphicsPipeline = 
		{
			DepthStencilDesc = 
			{
				DepthEnable = true,
				DepthWriteEnable = true,
				DepthFunc = "COMPARISON_FUNC_LESS"
			},
			RasterizerDesc = 
			{
				FillMode = "FILL_MODE_SOLID",
				CullMode = "CULL_MODE_BACK",
				DepthClipEnable = false,
				FrontCounterClockwise = false
				-- Do not use slope-scaled depth bias because this results in light leaking
				-- through terrain!			
			},
			InputLayout = InputLayoutElements,
			pVS = HemisphereZOnlyVS,
			DSVFormat = "TEX_FORMAT_D32_FLOAT",
            PrimitiveTopology = "PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP"
		}
	}
end


function CreateRenderNormalMapShaders()

	local ScreenSizeQuadVS = Shader.Create{
		FilePath =  "ScreenSizeQuadVS.fx",
		EntryPoint = "GenerateScreenSizeQuadVS",
		SearchDirectories = "shaders\\;shaders\\terrain",
		SourceLanguage = "SHADER_SOURCE_LANGUAGE_HLSL",
        UseCombinedTextureSamplers = true,
		Desc = {
			ShaderType = "SHADER_TYPE_VERTEX",
			Name = "GenerateScreenSizeQuadVS",
			DefaultVariableType = "SHADER_VARIABLE_TYPE_STATIC"
		}
	}

	local GenerateNormalMapPS = Shader.Create{
		FilePath =  "GenerateNormalMapPS.fx",
		EntryPoint = "GenerateNormalMapPS",
		SearchDirectories = "shaders\\;shaders\\terrain",
		SourceLanguage = "SHADER_SOURCE_LANGUAGE_HLSL",
        UseCombinedTextureSamplers = true,
		Desc = {
			ShaderType = "SHADER_TYPE_PIXEL",
			Name = "GenerateNormalMapPS",
			DefaultVariableType = "SHADER_VARIABLE_TYPE_STATIC",
			VariableDesc = 
			{ 
				{Name = "g_tex2DElevationMap", Type = "SHADER_VARIABLE_TYPE_STATIC"}, 
				{Name = "cbNMGenerationAttribs", Type = "SHADER_VARIABLE_TYPE_STATIC"}, 
			}
		}
	}

	extResourceMapping["g_tex2DElevationMap"]:SetSampler(PointClampSampler)
	ScreenSizeQuadVS:BindResources(extResourceMapping)
	GenerateNormalMapPS:BindResources(extResourceMapping)


	RenderNormalMapPSO = PipelineState.Create
	{
		GraphicsPipeline = 
		{
			DepthStencilDesc = 
			{
				DepthEnable = false,
				DepthWriteEnable = false
			},
			RasterizerDesc = 
			{
				FillMode = "FILL_MODE_SOLID",
				CullMode = "CULL_MODE_NONE",
				FrontCounterClockwise = true
			},
			pVS = ScreenSizeQuadVS,
			pPS = GenerateNormalMapPS,
			RTVFormats = "TEX_FORMAT_RG8_UNORM",
            PrimitiveTopology = "PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP"
		}
	}
	
end


function SetRenderNormalMapShadersAndStates()
	Context.SetPipelineState(RenderNormalMapPSO)
	Context.CommitShaderResources("COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES")
end

function RenderHemisphere(PrecomputedNetDensitySRV, AmbientSkylightSRV, ShadowMapSRV)
	Context.SetPipelineState(RenderHemispherePSO)
	
	RenderHemisphereSRB:GetVariable("SHADER_TYPE_VERTEX", "g_tex2DOccludedNetDensityToAtmTop"):Set(PrecomputedNetDensitySRV)
	RenderHemisphereSRB:GetVariable("SHADER_TYPE_VERTEX", "g_tex2DAmbientSkylight"):Set(AmbientSkylightSRV)
	RenderHemisphereSRB:GetVariable("SHADER_TYPE_PIXEL", "g_tex2DShadowMap"):Set(ShadowMapSRV)
	
	Context.CommitShaderResources(RenderHemisphereSRB, "COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES")
end

function RenderHemisphereShadow()
	Context.SetPipelineState(RenderHemisphereZOnlyPSO)
	Context.CommitShaderResources("COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES")
end
