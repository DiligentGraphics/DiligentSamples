-- Terrain.lua

-- Externally defined variables:
-- extResourceMapping

DisableDepthDSS = DepthStencilState.Create{    
	Name = "Terrain.lua: disable depth DSS",
	DepthEnable = false,
    DepthWriteEnable = false
}

EnableDepthDSS = DepthStencilState.Create{    
	Name = "Terrain.lua: enable depth DSS",
	DepthEnable = true,
    DepthWriteEnable = true,
	DepthFunc = "COMPARISON_FUNC_LESS"
}

DefaultBS = BlendState.Create{}

SolidFillCullBackRS = RasterizerState.Create{
	Name = "Terrain.lua: solid fill, cull back RS",
	FillMode = "FILL_MODE_SOLID",
	CullMode = "CULL_MODE_BACK",
	FrontCounterClockwise = true
}

SolidFillNoCullRS = RasterizerState.Create{
	Name = "Terrain.lua: solid fill, no cull RS",
	FillMode = "FILL_MODE_SOLID",
	CullMode = "CULL_MODE_NONE",
	FrontCounterClockwise = true
}

WireFillCullBackRS = RasterizerState.Create{
	Name = "Terrain.lua: wire fill, cull back RS",
	FillMode = "FILL_MODE_WIREFRAME",
	CullMode = "CULL_MODE_BACK",
	FrontCounterClockwise = true
}

ZOnlyPassRS = RasterizerState.Create{
	Name = "Terrain.lua: Z-only pass RS",
	FillMode = "FILL_MODE_SOLID",
	CullMode = "CULL_MODE_BACK",
	DepthClipEnable = false,
	FrontCounterClockwise = false
	-- Do not use slope-scaled depth bias because this results in light leaking
    -- through terrain!
}

LinearMirrorSampler = Sampler.Create{
    Name = "Terrain.lua: linear mirror sampler",
    MinFilter = "FILTER_TYPE_LINEAR", 
    MagFilter = "FILTER_TYPE_LINEAR", 
    MipFilter = "FILTER_TYPE_LINEAR", 
    AddressU = "TEXTURE_ADDRESS_MIRROR", 
    AddressV = "TEXTURE_ADDRESS_MIRROR", 
    AddressW = "TEXTURE_ADDRESS_MIRROR"
}

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

extResourceMapping["g_tex2DNormalMap"]:SetSampler(LinearMirrorSampler)
extResourceMapping["g_tex2DMtrlMap"]:SetSampler(LinearMirrorSampler)
extResourceMapping["g_tex2DTileDiffuse0"]:SetSampler(LinearWrapSampler)
extResourceMapping["g_tex2DTileDiffuse1"]:SetSampler(LinearWrapSampler)
extResourceMapping["g_tex2DTileDiffuse2"]:SetSampler(LinearWrapSampler)
extResourceMapping["g_tex2DTileDiffuse3"]:SetSampler(LinearWrapSampler)
extResourceMapping["g_tex2DTileDiffuse4"]:SetSampler(LinearWrapSampler)
extResourceMapping["g_tex2DTileNM0"]:SetSampler(LinearWrapSampler)
extResourceMapping["g_tex2DTileNM1"]:SetSampler(LinearWrapSampler)
extResourceMapping["g_tex2DTileNM2"]:SetSampler(LinearWrapSampler)
extResourceMapping["g_tex2DTileNM3"]:SetSampler(LinearWrapSampler)
extResourceMapping["g_tex2DTileNM4"]:SetSampler(LinearWrapSampler)

-- Shaders

function CreateHemisphereShaders()

	HemisphereVS = Shader.Create{
		FilePath =  "HemisphereVS.fx",
		EntryPoint = "HemisphereVS",
		SearchDirectories = "shaders;shaders\\terrain",
		SourceLanguage = "SHADER_SOURCE_LANGUAGE_HLSL",
		Desc = {
			ShaderType = "SHADER_TYPE_VERTEX",
			Name = "HemisphereVS"
		}
	}

	HemisphereZOnlyVS = Shader.Create{
		FilePath =  "HemisphereZOnlyVS.fx",
		EntryPoint = "HemisphereZOnlyVS",
		SourceLanguage = "SHADER_SOURCE_LANGUAGE_HLSL",
		SearchDirectories = "shaders\\;shaders\\terrain",
		Desc = {
			ShaderType = "SHADER_TYPE_VERTEX",
			Name = "HemisphereZOnlyVS"
		}
	}

	VertexLayout = VertexLayout or LayoutDesc.Create({
		Name = "Hemisphere Vertex Layout",
		LayoutElements = {
			{ InputIndex = 0, BufferSlot = 0, NumComponents = 3, ValueType = "VT_FLOAT32"},
			{ InputIndex = 1, BufferSlot = 0, NumComponents = 2, ValueType = "VT_FLOAT32"}
		}
	}, HemisphereVS)
	
	-- extResourceMapping is set by the app
	HemisphereVS:BindResources(extResourceMapping)
	HemisphereZOnlyVS:BindResources(extResourceMapping)

end

function SetHemispherePS(in_HemispherePS)
	HemispherePS = in_HemispherePS
	HemispherePS:BindResources(extResourceMapping)
end


function CreateRenderNormalMapShaders()

	ScreenSizeQuadVS = Shader.Create{
		FilePath =  "ScreenSizeQuadVS.fx",
		EntryPoint = "GenerateScreenSizeQuadVS",
		SearchDirectories = "shaders\\;shaders\\terrain",
		SourceLanguage = "SHADER_SOURCE_LANGUAGE_HLSL",
		Desc = {
			ShaderType = "SHADER_TYPE_VERTEX",
			Name = "GenerateScreenSizeQuadVS"
		}
	}

	GenerateNormalMapPS = Shader.Create{
		FilePath =  "GenerateNormalMapPS.fx",
		EntryPoint = "GenerateNormalMapPS",
		SearchDirectories = "shaders\\;shaders\\terrain",
		SourceLanguage = "SHADER_SOURCE_LANGUAGE_HLSL",
		Desc = {
			ShaderType = "SHADER_TYPE_PIXEL",
			Name = "GenerateNormalMapPS"
		}
	}

	extResourceMapping["g_tex2DElevationMap"]:SetSampler(PointClampSampler)
	ScreenSizeQuadVS:BindResources(extResourceMapping)
	GenerateNormalMapPS:BindResources(extResourceMapping)

end


function SetRenderNormalMapShadersAndStates()
	Context.SetDepthStencilState(DisableDepthDSS)
	Context.SetRasterizerState(SolidFillNoCullRS)
	Context.SetBlendState(DefaultBS)
	
	Context.SetShaders(ScreenSizeQuadVS, GenerateNormalMapPS)
end

function RenderHemisphere(PrecomputedNetDensitySRV, AmbientSkylightSRV)
	Context.SetDepthStencilState(EnableDepthDSS)
	Context.SetRasterizerState(SolidFillCullBackRS)
	--Context.SetRasterizerState(WireFillCullBackRS)
	Context.SetBlendState(DefaultBS)
	
	HemisphereVS:GetShaderVariable("g_tex2DOccludedNetDensityToAtmTop"):Set(PrecomputedNetDensitySRV)
	HemisphereVS:GetShaderVariable("g_tex2DAmbientSkylight"):Set(AmbientSkylightSRV)
	Context.SetInputLayout(VertexLayout)
	Context.SetShaders(HemisphereVS, HemispherePS)
end

function RenderHemisphereShadow()
	Context.SetDepthStencilState(EnableDepthDSS)
	Context.SetRasterizerState(ZOnlyPassRS)
	Context.SetBlendState(DefaultBS)
	
	Context.SetInputLayout(VertexLayout)
	Context.SetShaders(HemisphereZOnlyVS)
end
