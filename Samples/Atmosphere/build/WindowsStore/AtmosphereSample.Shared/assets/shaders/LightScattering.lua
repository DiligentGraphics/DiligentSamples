-- LightScattering.lua

-- Externally defined variables:
--[[

extResourceMapping

]]-- 


---------------------------------  Depth Stencil States ---------------------------------------

EnableDepthDSS = DepthStencilState.Create{    
	Name = "LightScattering.lua: comparison less, writes enabled DSS",
	DepthEnable = true,
    DepthWriteEnable = true,
	DepthFunc = "COMPARISON_FUNC_LESS"
}

CmpEqNoWritesDSS = DepthStencilState.Create{    
	Name = "LightScattering.lua: comparison equal, no writes DSS",
	DepthEnable = true,
    DepthWriteEnable = false,
	DepthFunc = "COMPARISON_FUNC_EQUAL"
}

DisableDepthDSS = DepthStencilState.Create{    
	Name = "LightScattering.lua: disable depth DSS",
	DepthEnable = false,
    DepthWriteEnable = false
}

-- Disable depth testing and always increment stencil value
-- This depth stencil state is used to mark samples which will undergo further processing
-- Pixel shader discards pixels that should not be further processed, thus keeping the
-- stencil value untouched
-- For instance, pixel shader performing epipolar coordinates generation discards all 
-- sampes, whoose coordinates are outside the screen [-1,1]x[-1,1] area
DSSDesc = {
	Name = "LightScattering.lua: disable depth, inc stencil DSS",
    DepthEnable = false,
    DepthWriteEnable = false,
    StencilEnable = true,
    FrontFace = {
		StencilFunc = "COMPARISON_FUNC_ALWAYS",
		StencilPassOp = "STENCIL_OP_INCR_SAT", 
		StencilFailOp = "STENCIL_OP_KEEP",  
		StencilDepthFailOp = "STENCIL_OP_KEEP"
	},
    BackFace  = {
		StencilFunc = "COMPARISON_FUNC_ALWAYS",
		StencilPassOp = "STENCIL_OP_INCR_SAT",
		StencilFailOp = "STENCIL_OP_KEEP",  
		StencilDepthFailOp = "STENCIL_OP_KEEP"
	},
	StencilReadMask = 0xFF,
	StencilWriteMask= 0xFF
}
IncStencilAlwaysDSS = DepthStencilState.Create(DSSDesc)

-- Disable depth testing, stencil testing function equal, increment stencil
-- This state is used to process only those pixels that were marked at the previous pass
-- All pixels whith different stencil value are discarded from further processing as well
-- as some pixels can also be discarded during the draw call
-- For instance, pixel shader marking ray marching samples processes only those pixels which are inside
-- the screen. It also discards all but those samples that are interpolated from themselves
DSSDesc.FrontFace.StencilFunc = "COMPARISON_FUNC_EQUAL"
DSSDesc.BackFace.StencilFunc = "COMPARISON_FUNC_EQUAL"
DSSDesc.Name = "LightScattering.lua: disable depth, stencil equal, inc stencil DSS"
StencilEqIncStencilDSS = DepthStencilState.Create(DSSDesc)


-- Disable depth testing, stencil testing function equal, keep stencil
DSSDesc.FrontFace.StencilPassOp = "STENCIL_OP_KEEP"
DSSDesc.BackFace.StencilPassOp = "STENCIL_OP_KEEP"
DSSDesc.Name = "LightScattering.lua: disable depth, stencil equal, keep stencil DSS"
StencilEqKeepStencilDSS = DepthStencilState.Create(DSSDesc)



---------------------------------  Rasterizer States ---------------------------------------

SolidFillNoCullRS = RasterizerState.Create{
	Name = "LightScattering.lua: solid fill, no cull RS",
	FillMode = "FILL_MODE_SOLID",
	CullMode = "CULL_MODE_NONE",
	FrontCounterClockwise = true
}


------------------------------------  Blend States ------------------------------------------
DefaultBS = BlendState.Create{Name = "LightScattering.lua: default BS"}

BSDesc = {
	Name = "LightScattering.lua: additive blend BS",
	IndependentBlendEnable = false,
	RenderTargets = {
		{
			BlendEnable = true,
			BlendOp     ="BLEND_OPERATION_ADD",
			BlendOpAlpha= "BLEND_OPERATION_ADD",
			DestBlend   = "BLEND_FACTOR_ONE",
			DestBlendAlpha= "BLEND_FACTOR_ONE",
			SrcBlend     = "BLEND_FACTOR_ONE",
			SrcBlendAlpha= "BLEND_FACTOR_ONE"
		}
	}
}
AdditiveBlendBS = BlendState.Create(BSDesc)

BSDesc.Name = "LightScattering.lua: Alpha blend BS"
BSDesc.RenderTargets[1].SrcBlend      = "BLEND_FACTOR_SRC_ALPHA"
BSDesc.RenderTargets[1].SrcBlendAlpha = "BLEND_FACTOR_SRC_ALPHA"
BSDesc.RenderTargets[1].DestBlend      = "BLEND_FACTOR_INV_SRC_ALPHA"
BSDesc.RenderTargets[1].DestBlendAlpha = "BLEND_FACTOR_INV_SRC_ALPHA"
AlphaBlendBS = BlendState.Create(BSDesc)


------------------------------------  Samplers ------------------------------------------

LinearClampSampler = Sampler.Create{
    Name = "LightScattering.lua: linear clamp sampler",
    MinFilter = "FILTER_TYPE_LINEAR", 
    MagFilter = "FILTER_TYPE_LINEAR", 
    MipFilter = "FILTER_TYPE_LINEAR", 
    AddressU = "TEXTURE_ADDRESS_CLAMP", 
    AddressV = "TEXTURE_ADDRESS_CLAMP", 
    AddressW = "TEXTURE_ADDRESS_CLAMP"
}

PointClampSampler = Sampler.Create{
    Name = "LightScattering.lua: point clamp sampler",
    MinFilter = "FILTER_TYPE_POINT", 
    MagFilter = "FILTER_TYPE_POINT", 
    MipFilter = "FILTER_TYPE_POINT", 
    AddressU = "TEXTURE_ADDRESS_CLAMP", 
    AddressV = "TEXTURE_ADDRESS_CLAMP", 
    AddressW = "TEXTURE_ADDRESS_CLAMP"
}

function CreateAuxTextures(NumSlices, MaxSamplesInSlice)
	local TexDesc = {
		Type = "TEXTURE_TYPE_2D", 
		Width = MaxSamplesInSlice, Height = NumSlices,
		Format = "TEX_FORMAT_RG32_FLOAT",
		MipLevels = 1,
		Usage = "USAGE_DEFAULT",
		BindFlags = {"BIND_RENDER_TARGET", "BIND_SHADER_RESOURCE"}
	}
	-- MaxSamplesInSlice x NumSlices RG32F texture to store screen-space coordinates
	-- for every epipolar sample
	tex2DCoordinateTexture = Texture.Create(TexDesc)
	tex2DCoordinateTextureSRV = tex2DCoordinateTexture:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DCoordinateTextureRTV = tex2DCoordinateTexture:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	tex2DCoordinateTextureSRV:SetSampler(LinearClampSampler)
	extResourceMapping["g_tex2DCoordinates"] = tex2DCoordinateTextureSRV

	-- NumSlices x 1 RGBA32F texture to store end point coordinates
	-- for every epipolar slice
	TexDesc.Width = NumSlices
	TexDesc.Height = 1
	TexDesc.Format = "TEX_FORMAT_RGBA32_FLOAT"
	tex2DSliceEndpoints = Texture.Create(TexDesc)
	tex2DSliceEndpointsSRV = tex2DSliceEndpoints:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DSliceEndpointsRTV = tex2DSliceEndpoints:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	tex2DSliceEndpointsSRV:SetSampler(LinearClampSampler)
	extResourceMapping["g_tex2DSliceEndPoints"] = tex2DSliceEndpointsSRV

	-- MaxSamplesInSlice x NumSlices RG16U texture to store two indices from which
	-- the sample should be interpolated, for every epipolar sample
	TexDesc.Width = MaxSamplesInSlice
	TexDesc.Height = NumSlices

	-- In fact we only need RG16U texture to store
	-- interpolation source indices. However, NVidia GLES does
	-- not supported imge load/store operations on this format, 
	-- so we have to resort to RGBA32U.
	TexDesc.Format = "TEX_FORMAT_RGBA32_UINT"
	TexDesc.BindFlags = {"BIND_UNORDERED_ACCESS", "BIND_SHADER_RESOURCE"}
	tex2DInterpolationSource = Texture.Create(TexDesc)
	tex2DInterpolationSourceSRV = tex2DInterpolationSource:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DInterpolationSourceUAV = tex2DInterpolationSource:GetDefaultView("TEXTURE_VIEW_UNORDERED_ACCESS")
	tex2DInterpolationSourceSRV:SetSampler(PointClampSampler)
	extResourceMapping["g_tex2DInterpolationSource"] = tex2DInterpolationSourceSRV
	extResourceMapping["g_rwtex2DInterpolationSource"] = tex2DInterpolationSourceUAV

	-- MaxSamplesInSlice x NumSlices R32F texture to store camera-space Z coordinate,
	-- for every epipolar sample
	TexDesc.Format = "TEX_FORMAT_R32_FLOAT"
	TexDesc.BindFlags = {"BIND_RENDER_TARGET", "BIND_SHADER_RESOURCE"}
	tex2DEpipolarCamSpaceZ = Texture.Create(TexDesc)
	tex2DEpipolarCamSpaceZSRV = tex2DEpipolarCamSpaceZ:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DEpipolarCamSpaceZRTV = tex2DEpipolarCamSpaceZ:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	tex2DEpipolarCamSpaceZSRV:SetSampler(LinearClampSampler)
	extResourceMapping["g_tex2DEpipolarCamSpaceZ"] = tex2DEpipolarCamSpaceZSRV

	-- MaxSamplesInSlice x NumSlices RGBA16F texture to store interpolated inscattered light,
	-- for every epipolar sample
	TexDesc.Format = "TEX_FORMAT_RGBA16_FLOAT"
	tex2DEpipolarInscattering = Texture.Create(TexDesc)
	tex2DEpipolarInscatteringSRV = tex2DEpipolarInscattering:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DEpipolarInscatteringRTV = tex2DEpipolarInscattering:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	tex2DEpipolarInscatteringSRV:SetSampler(LinearClampSampler)
	extResourceMapping["g_tex2DScatteredColor"] = tex2DEpipolarInscatteringSRV

	-- MaxSamplesInSlice x NumSlices RGBA16F texture to store initial inscattered light,
	-- for every epipolar sample
	tex2DInitialScatteredLight = Texture.Create(TexDesc)
	tex2DInitialScatteredLightSRV = tex2DInitialScatteredLight:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DInitialScatteredLightRTV = tex2DInitialScatteredLight:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	tex2DInitialScatteredLightSRV:SetSampler(LinearClampSampler)
	extResourceMapping["g_tex2DInitialInsctrIrradiance"] = tex2DInitialScatteredLightSRV

	-- MaxSamplesInSlice x NumSlices depth stencil texture to mark samples for processing,
	-- for every epipolar sample
	TexDesc.Format = "TEX_FORMAT_D24_UNORM_S8_UINT"
	TexDesc.BindFlags = "BIND_DEPTH_STENCIL"
	tex2DEpipolarImageDepth = Texture.Create(TexDesc)
	tex2DEpipolarImageDSV = tex2DEpipolarImageDepth:GetDefaultView("TEXTURE_VIEW_DEPTH_STENCIL")

	-- Slice UV dir and origin texture must be recreated
	tex2DSliceUVDirAndOriginSRV = nil
	tex2DSliceUVDirAndOriginRTV = nil
	tex2DSliceUVDirAndOrigin = nil

	-- Extinction texture must be recreated
	tex2DEpipolarExtinction = nil
	tex2DEpipolarExtinctionSRV = nil
	tex2DEpipolarExtinctionRTV = nil

	-- Force garbage collection to make sure all graphics resources are released
	collectgarbage()
end

function CreateExtinctionTexture(NumSlices, MaxSamplesInSlice)
	local TexDesc = {
		Type = "TEXTURE_TYPE_2D", 
		Width = MaxSamplesInSlice, Height = NumSlices,
		Format = "TEX_FORMAT_RGBA8_UNORM",
		MipLevels = 1,
		Usage = "USAGE_DEFAULT",
		BindFlags = {"BIND_RENDER_TARGET", "BIND_SHADER_RESOURCE"}
	}
	-- MaxSamplesInSlice x NumSlices RGBA8_UNORM texture to store extinction
	-- for every epipolar sample
	tex2DEpipolarExtinction = Texture.Create(TexDesc)
	tex2DEpipolarExtinctionSRV = tex2DEpipolarExtinction:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DEpipolarExtinctionRTV = tex2DEpipolarExtinction:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	tex2DEpipolarExtinctionSRV:SetSampler(LinearClampSampler)
	extResourceMapping["g_tex2DEpipolarExtinction"] = tex2DEpipolarExtinctionSRV

	-- Force garbage collection to make sure all graphics resources are released
	collectgarbage()
end

function CreateLowResLuminanceTexture(LowResLuminanceMips)
	-- Create low-resolution texture to store image luminance
	local TexDesc = {
		Type = "TEXTURE_TYPE_2D", 
		Width = 2^(LowResLuminanceMips-1), Height = 2^(LowResLuminanceMips-1),
		Format = "TEX_FORMAT_R16_FLOAT",
		MipLevels = LowResLuminanceMips,
		Usage = "USAGE_DEFAULT",
		BindFlags = {"BIND_RENDER_TARGET", "BIND_SHADER_RESOURCE"},
		MiscFlags = "MISC_TEXTURE_FLAG_GENERATE_MIPS"
	}
	tex2DLowResLuminance = Texture.Create(TexDesc)
	tex2DLowResLuminanceSRV = tex2DLowResLuminance:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DLowResLuminanceRTV = tex2DLowResLuminance:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	tex2DLowResLuminanceSRV:SetSampler(LinearClampSampler)
	extResourceMapping["g_tex2DLowResLuminance"] = tex2DLowResLuminanceSRV


	TexDesc.Width = 1
	TexDesc.Height = 1
	TexDesc.MipLevels = 1
	TexDesc.MiscFlags = 0
	tex2DAverageLuminance = Texture.Create(TexDesc)
	tex2DAverageLuminanceSRV = tex2DAverageLuminance:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DAverageLuminanceRTV = tex2DAverageLuminance:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	-- Set intial luminance to 1
	Context.SetRenderTargets(tex2DAverageLuminanceRTV)
	Context.ClearRenderTarget(tex2DAverageLuminanceRTV, 0.1,0.1,0.1,0.1)
	tex2DAverageLuminanceSRV:SetSampler(LinearClampSampler)
	extResourceMapping["g_tex2DAverageLuminance"] = tex2DAverageLuminanceSRV
end

function CreateSliceUVDirAndOriginTexture(NumEpipolarSlices, NumCascades)
	tex2DSliceUVDirAndOrigin = Texture.Create{
		Type = "TEXTURE_TYPE_2D", 
		Width = NumEpipolarSlices, 
		Height = NumCascades,
		Format = "TEX_FORMAT_RGBA32_FLOAT",
		MipLevels = 1,
		Usage = "USAGE_DEFAULT",
		BindFlags = {"BIND_RENDER_TARGET", "BIND_SHADER_RESOURCE"}
	}
	
	tex2DSliceUVDirAndOriginSRV = tex2DSliceUVDirAndOrigin:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DSliceUVDirAndOriginRTV = tex2DSliceUVDirAndOrigin:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	tex2DSliceUVDirAndOriginSRV:SetSampler(LinearClampSampler)
	extResourceMapping["g_tex2DSliceUVDirAndOrigin"] = tex2DSliceUVDirAndOriginSRV

	-- Force garbage collection to make sure all graphics resources are released
	collectgarbage()
end

function CreateAmbientSkyLightTexture(AmbientSkyLightTexDim)
	tex2DAmbientSkyLight = Texture.Create{
		Type = "TEXTURE_TYPE_2D", 
		Width = AmbientSkyLightTexDim, 
		Height = 1,
		Format = "TEX_FORMAT_RGBA16_FLOAT",
		MipLevels = 1,
		Usage = "USAGE_DEFAULT",
		BindFlags = {"BIND_RENDER_TARGET", "BIND_SHADER_RESOURCE"}
	}
	
	tex2DAmbientSkyLightSRV = tex2DAmbientSkyLight:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DAmbientSkyLightRTV = tex2DAmbientSkyLight:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	tex2DAmbientSkyLightSRV:SetSampler(LinearClampSampler)
end

-----------------------------------[ Screen Size Quad Rendering ]-----------------------------------
function CreateShader(File, Entry, ShaderType)
	return Shader.Create{
				FilePath = File,
				EntryPoint = Entry,
				SearchDirectories = "shaders;shaders\\atmosphere",
				SourceLanguage = "SHADER_SOURCE_LANGUAGE_HLSL",
				Desc = {
					ShaderType = ShaderType,
					Name = "LightScattering.lua:" .. Entry
				}
			}
end
function CreateVertexShader(File, Entry, ShaderType) 
	return CreateShader(File, Entry, "SHADER_TYPE_VERTEX") 
end
function CreatePixelShader(File, Entry, ShaderType) 
	return CreateShader(File, Entry, "SHADER_TYPE_PIXEL") 
end

ScreenSizeQuadVS = CreateVertexShader("ScreenSizeQuadVS.fx", "ScreenSizeQuadVS")

ScreenSizeQuadDrawAttrs = DrawAttribs.Create{
    NumVertices = 4,
    Topology = "PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP"
}

function RenderScreenSizeQuad(PixelShader, DSS, StencilRef, BS, NumQuads)
	
	Context.SetRasterizerState(SolidFillNoCullRS)
	Context.SetDepthStencilState(DSS, StencilRef)
	Context.SetBlendState(BS)

	--Context.SetInputLayout(nil)

	Context.SetShaders(ScreenSizeQuadVS, PixelShader)

	ScreenSizeQuadDrawAttrs.NumInstances = NumQuads or 1
	Context.Draw(ScreenSizeQuadDrawAttrs)
end


-----------------------------------[ Precomputing Optical Depth ]-----------------------------------
PrecomputeNetDensityToAtmTopPS = CreatePixelShader("Precomputation.fx", "PrecomputeNetDensityToAtmTopPS")

function PrecomputeNetDensityToAtmTop(NumPrecomputedHeights, NumPrecomputedAngles)
	
	tex2DOccludedNetDensityToAtmTop = Texture.Create{
		Type = "TEXTURE_TYPE_2D", 
		Width = NumPrecomputedHeights, Height = NumPrecomputedAngles,
		Format = "TEX_FORMAT_RG32_FLOAT",
		MipLevels = 1,
		Usage = "USAGE_DEFAULT",
		BindFlags = {"BIND_SHADER_RESOURCE", "BIND_RENDER_TARGET"}
	}

	Context.SetRenderTargets( tex2DOccludedNetDensityToAtmTop:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET") )
	-- Bind required resources
	PrecomputeNetDensityToAtmTopPS:BindResources(extResourceMapping, "BIND_SHADER_RESOURCES_ALL_RESOLVED")
	-- Render quad
	RenderScreenSizeQuad(PrecomputeNetDensityToAtmTopPS, DisableDepthDSS, 0, DefaultBS)
	-- Get SRV	
	tex2DOccludedNetDensityToAtmTopSRV = tex2DOccludedNetDensityToAtmTop:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	-- Set linear sampler
	tex2DOccludedNetDensityToAtmTopSRV:SetSampler(LinearClampSampler)
	-- Add texture to resource mapping
	extResourceMapping["g_tex2DOccludedNetDensityToAtmTop"] = tex2DOccludedNetDensityToAtmTopSRV

	-- Force garbage collection to make sure all graphics resources are released
	collectgarbage()
end




function WindowResize(BackBufferWidth, BackBufferHeight)
	ptex2DCamSpaceZ = Texture.Create{
		Type = "TEXTURE_TYPE_2D", Width = BackBufferWidth, Height = BackBufferHeight,
		Format = "TEX_FORMAT_R32_FLOAT",
		MipLevels = 1,
		Usage = "USAGE_DEFAULT",
		BindFlags = {"BIND_RENDER_TARGET", "BIND_SHADER_RESOURCE"}
	}

	ptex2DCamSpaceZRTV = ptex2DCamSpaceZ:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	ptex2DCamSpaceZSRV = ptex2DCamSpaceZ:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")

	ptex2DCamSpaceZSRV:SetSampler(LinearClampSampler)
	-- Add texture to resource mapping
	extResourceMapping["g_tex2DCamSpaceZ"] = ptex2DCamSpaceZSRV

	-- Force garbage collection to make sure all graphics resources are released
	collectgarbage()
end



function ReconstructCameraSpaceZ(ReconstructCameraSpaceZPS)
	Context.SetRenderTargets(ptex2DCamSpaceZRTV)
	RenderScreenSizeQuad(ReconstructCameraSpaceZPS, DisableDepthDSS, 0, DefaultBS)
end

function RenderSliceEndPoints(RenderSliceEndPointsPS)
	Context.SetRenderTargets(tex2DSliceEndpointsRTV)
	RenderScreenSizeQuad(RenderSliceEndPointsPS, DisableDepthDSS, 0, DefaultBS)
end

function RenderCoordinateTexture(RenderCoordinateTexturePS)
	Context.SetRenderTargets(tex2DCoordinateTextureRTV, tex2DEpipolarCamSpaceZRTV, tex2DEpipolarImageDSV)
	-- Clear both render targets with values that can't be correct projection space coordinates and camera space Z:
	Context.ClearRenderTarget(tex2DCoordinateTextureRTV, -1e+30, -1e+30, -1e+30, -1e+30)
	Context.ClearRenderTarget(tex2DEpipolarCamSpaceZRTV, -1e+30)
	Context.ClearDepthStencil(tex2DEpipolarImageDSV, 1.0, 0)
    -- Depth stencil state is configured to always increment stencil value. If coordinates are outside the screen,
    -- the pixel shader discards the pixel and stencil value is left untouched. All such pixels will be skipped from
    -- further processing
	RenderScreenSizeQuad(RenderCoordinateTexturePS, IncStencilAlwaysDSS, 0, DefaultBS)
end

function RenderCoarseUnshadowedInctr(RenderCoarseUnshadowedInctrPS)
	RenderScreenSizeQuad(RenderCoarseUnshadowedInctrPS, StencilEqKeepStencilDSS, 1, DefaultBS)
end

function MarkRayMarchingSamples(MarkRayMarchingSamplesPS)
	Context.SetRenderTargets(tex2DEpipolarImageDSV)
	RenderScreenSizeQuad(MarkRayMarchingSamplesPS, StencilEqIncStencilDSS, 1, DefaultBS)
end

function RenderSliceUVDirAndOrigin(RenderSliceUVDirAndOriginPS)
	Context.SetRenderTargets(tex2DSliceUVDirAndOriginRTV)
	RenderScreenSizeQuad(RenderSliceUVDirAndOriginPS, DisableDepthDSS, 0, DefaultBS)
end

function InitMinMaxShadowMap(InitMinMaxShadowMapPS)
	RenderScreenSizeQuad(InitMinMaxShadowMapPS, DisableDepthDSS, 0, DefaultBS)
end

function ComputeMinMaxShadowMapLevel(ComputeMinMaxShadowMapLevelPS)
	RenderScreenSizeQuad(ComputeMinMaxShadowMapLevelPS, DisableDepthDSS, 0, DefaultBS)
end

function ClearInitialScatteredLight()
	-- On GL, we need to bind render target to pipeline to clear it
	Context.SetRenderTargets(tex2DInitialScatteredLightRTV)
	Context.ClearRenderTarget(tex2DInitialScatteredLightRTV, 0,0,0,0)
end
function RayMarch(RayMarchPS, NumQuads)
	Context.SetRenderTargets(tex2DInitialScatteredLightRTV, tex2DEpipolarImageDSV)
	RenderScreenSizeQuad(RayMarchPS, StencilEqKeepStencilDSS, 2, AdditiveBlendBS, NumQuads)
end


function InterpolateIrradiance(InterpolateIrradiancePS)
	Context.SetRenderTargets(tex2DEpipolarInscatteringRTV)
	RenderScreenSizeQuad(InterpolateIrradiancePS, DisableDepthDSS, 0, DefaultBS)
end


function UnwarpAndRenderLuminance(UnwarpAndRenderLuminancePS)
	-- Disable depth testing - we need to render the entire image in low resolution
	RenderScreenSizeQuad(UnwarpAndRenderLuminancePS, DisableDepthDSS, 0, DefaultBS)
end

function UnwarpEpipolarScattering(UnwarpEpipolarScatteringPS)
	-- Enable depth testing to write 0.0 to the depth buffer. All pixel that require 
	-- inscattering correction (if enabled) will be discarded, so that 1.0 will be retained
	-- This 1.0 will then be used to perform inscattering correction
	RenderScreenSizeQuad(UnwarpEpipolarScatteringPS, EnableDepthDSS, 0, DefaultBS)
end


function FixInscatteringAtDepthBreaks(FixInsctrAtDepthBreaksPS, Mode)
	if Mode == 0 then -- Luminance Only
		-- Disable depth and stencil tests to render all pixels
		-- Use default blend state to overwrite old luminance values
		RenderScreenSizeQuad(FixInsctrAtDepthBreaksPS, DisableDepthDSS, 0, DefaultBS)
	elseif Mode == 1 then -- Fix Inscattering
		-- Depth breaks are marked with 1.0 in depth, so we enable depth test
		-- to render only pixels that require correction
		-- Use default blend state - the rendering is always done in single pass
		RenderScreenSizeQuad(FixInsctrAtDepthBreaksPS, EnableDepthDSS, 0, DefaultBS)
	elseif Mode == 2 then -- Full Screen Ray Marching
		-- Disable depth and stencil tests since we are performing 
		-- full screen ray marching
		-- Use default blend state - the rendering is always done in single pass
		RenderScreenSizeQuad(FixInsctrAtDepthBreaksPS, DisableDepthDSS, 0, DefaultBS)
	end
end

function UpdateAverageLuminance(UpdateAverageLuminancePS)
	Context.SetRenderTargets(tex2DAverageLuminanceRTV)
	RenderScreenSizeQuad(UpdateAverageLuminancePS, DisableDepthDSS, 0, AlphaBlendBS)
end

SampleLocationsDrawAttrs = DrawAttribs.Create{
    NumVertices = 4,
    Topology = "PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP"
}

function RenderSampleLocations(RenderSampleLocationsVS, RenderSampleLocationsPS, TotalSamples)
	Context.SetRasterizerState(SolidFillNoCullRS)
	Context.SetDepthStencilState(DisableDepthDSS)
	Context.SetBlendState(AlphaBlendBS)

	--Context.SetInputLayout(nil)

	Context.SetShaders(RenderSampleLocationsVS, RenderSampleLocationsPS)

	SampleLocationsDrawAttrs.NumInstances = TotalSamples
	Context.Draw(SampleLocationsDrawAttrs)
end

SunVS = CreateVertexShader("Sun.fx", "SunVS")
SunPS = CreatePixelShader("Sun.fx", "SunPS")

function RenderSun()
	Context.SetRasterizerState(SolidFillNoCullRS)
	Context.SetDepthStencilState(CmpEqNoWritesDSS)
	Context.SetBlendState(DefaultBS)

	--Context.SetInputLayout(nil)

	Context.SetShaders(SunVS, SunPS)
	SunVS:BindResources(extResourceMapping, "BIND_SHADER_RESOURCES_ALL_RESOLVED")
	SunPS:BindResources(extResourceMapping, "BIND_SHADER_RESOURCES_ALL_RESOLVED")

	ScreenSizeQuadDrawAttrs.NumInstances = 1
	Context.Draw(ScreenSizeQuadDrawAttrs)
end

function PrecomputeAmbientSkyLight(PrecomputeAmbientSkyLightPS)
	RenderScreenSizeQuad(PrecomputeAmbientSkyLightPS, DisableDepthDSS, 0, DefaultBS)
end
