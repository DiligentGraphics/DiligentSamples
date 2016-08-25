-- LightScattering.lua

--[[
Externally defined variables:

	extResourceMapping
	MainBackBufferFmt
	MainDepthBufferFmt
	OffscreenBackBufferFmt
]]-- 


---------------------------------  Depth Stencil States ---------------------------------------
CoordinateTexFmt = "TEX_FORMAT_RG32_FLOAT"
SliceEndpointsFmt = "TEX_FORMAT_RGBA32_FLOAT"
InterpolationSourceTexFmt = "TEX_FORMAT_RGBA32_UINT"
EpipolarCamSpaceZFmt = "TEX_FORMAT_R32_FLOAT"
EpipolarInsctrTexFmt = "TEX_FORMAT_RGBA16_FLOAT"
EpipolarImageDepthFmt = "TEX_FORMAT_D24_UNORM_S8_UINT"
EpipolarExtinctionFmt = "TEX_FORMAT_RGBA8_UNORM"
AmbientSkyLightTexFmt = "TEX_FORMAT_RGBA16_FLOAT"
LuminanceTexFmt = "TEX_FORMAT_R16_FLOAT"
SliceUVDirAndOriginTexFmt = "TEX_FORMAT_RGBA32_FLOAT"
CamSpaceZFmt = "TEX_FORMAT_R32_FLOAT"

EnableDepthDesc = 
{
	DepthEnable = true,
    DepthWriteEnable = true,
	DepthFunc = "COMPARISON_FUNC_LESS"
}

DisableDepthDesc = 
{
	DepthEnable = false,
    DepthWriteEnable = false
}


CmpEqNoWritesDSSDesc =
{    
	DepthEnable = true,
    DepthWriteEnable = false,
	DepthFunc = "COMPARISON_FUNC_EQUAL"
}


-- Disable depth testing and always increment stencil value
-- This depth stencil state is used to mark samples which will undergo further processing
-- Pixel shader discards pixels that should not be further processed, thus keeping the
-- stencil value untouched
-- For instance, pixel shader performing epipolar coordinates generation discards all 
-- sampes, whoose coordinates are outside the screen [-1,1]x[-1,1] area
IncStencilAlwaysDSSDesc = 
{
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

-- Disable depth testing, stencil testing function equal, increment stencil
-- This state is used to process only those pixels that were marked at the previous pass
-- All pixels whith different stencil value are discarded from further processing as well
-- as some pixels can also be discarded during the draw call
-- For instance, pixel shader marking ray marching samples processes only those pixels which are inside
-- the screen. It also discards all but those samples that are interpolated from themselves
StencilEqIncStencilDSSDesc = 
{
    DepthEnable = false,
    DepthWriteEnable = false,
    StencilEnable = true,
    FrontFace = {
		StencilFunc = "COMPARISON_FUNC_EQUAL",
		StencilPassOp = "STENCIL_OP_INCR_SAT", 
		StencilFailOp = "STENCIL_OP_KEEP",  
		StencilDepthFailOp = "STENCIL_OP_KEEP"
	},
    BackFace  = {
		StencilFunc = "COMPARISON_FUNC_EQUAL",
		StencilPassOp = "STENCIL_OP_INCR_SAT",
		StencilFailOp = "STENCIL_OP_KEEP",  
		StencilDepthFailOp = "STENCIL_OP_KEEP"
	},
	StencilReadMask = 0xFF,
	StencilWriteMask= 0xFF
}


-- Disable depth testing, stencil testing function equal, keep stencil
StencilEqKeepStencilDSSDesc = 
{
    DepthEnable = false,
    DepthWriteEnable = false,
    StencilEnable = true,
    FrontFace = {
		StencilFunc = "COMPARISON_FUNC_EQUAL",
		StencilPassOp = "STENCIL_OP_KEEP", 
		StencilFailOp = "STENCIL_OP_KEEP",  
		StencilDepthFailOp = "STENCIL_OP_KEEP"
	},
    BackFace  = {
		StencilFunc = "COMPARISON_FUNC_EQUAL",
		StencilPassOp = "STENCIL_OP_KEEP",
		StencilFailOp = "STENCIL_OP_KEEP",  
		StencilDepthFailOp = "STENCIL_OP_KEEP"
	},
	StencilReadMask = 0xFF,
	StencilWriteMask= 0xFF
}




------------------------------------  Blend States ------------------------------------------
DefaultBlendDesc = {}

AdditiveBlendBSDesc = 
{
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

AlphaBlendBSDesc = 
{
	IndependentBlendEnable = false,
	RenderTargets = {
		{
			BlendEnable = true,
			BlendOp     ="BLEND_OPERATION_ADD",
			BlendOpAlpha= "BLEND_OPERATION_ADD",
			DestBlend   = "BLEND_FACTOR_INV_SRC_ALPHA",
			DestBlendAlpha= "BLEND_FACTOR_INV_SRC_ALPHA",
			SrcBlend     = "BLEND_FACTOR_SRC_ALPHA",
			SrcBlendAlpha= "BLEND_FACTOR_SRC_ALPHA"
		}
	}
}


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


function ResetShaderResourceBindings()
	RenderCoarseUnshadowedInctrSRB = nil
	RenderSliceUVDirAndOriginSRB = nil
	InterpolateIrradianceSRB = nil
	MarkRayMarchingSamplesSRB = nil
	InitMinMaxShadowMapSRB = nil
	RayMarchSRB = nil
	RenderCoordinateTextureSRB = nil
	RenderSampleLocationsSRB = nil
	FixInscatteringAtDepthBreaksSRB = nil
	UnwarpAndRenderLuminanceSRB = nil
	UnwarpEpipolarScatteringSRB = nil
	UpdateAverageLuminanceSRB = nil
end

function CreateAuxTextures(NumSlices, MaxSamplesInSlice)
	local TexDesc = {
		Type = "RESOURCE_DIM_TEX_2D", 
		Width = MaxSamplesInSlice, Height = NumSlices,
		Format = CoordinateTexFmt,
		MipLevels = 1,
		Usage = "USAGE_DEFAULT",
		BindFlags = {"BIND_RENDER_TARGET", "BIND_SHADER_RESOURCE"},
		ClearValue = {Format = CoordinateTexFmt, Color = {r=-1e+30, g=-1e+30, b=-1e+30, a=-1e+30}}
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
	TexDesc.Format = SliceEndpointsFmt
	TexDesc.ClearValue = {Color = {r=-1e+30}}
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
	TexDesc.Format = InterpolationSourceTexFmt
	TexDesc.BindFlags = {"BIND_UNORDERED_ACCESS", "BIND_SHADER_RESOURCE"}
	tex2DInterpolationSource = Texture.Create(TexDesc)
	tex2DInterpolationSourceSRV = tex2DInterpolationSource:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DInterpolationSourceUAV = tex2DInterpolationSource:GetDefaultView("TEXTURE_VIEW_UNORDERED_ACCESS")
	tex2DInterpolationSourceSRV:SetSampler(PointClampSampler)
	extResourceMapping["g_tex2DInterpolationSource"] = tex2DInterpolationSourceSRV
	extResourceMapping["g_rwtex2DInterpolationSource"] = tex2DInterpolationSourceUAV

	-- MaxSamplesInSlice x NumSlices R32F texture to store camera-space Z coordinate,
	-- for every epipolar sample
	TexDesc.Format = EpipolarCamSpaceZFmt
	TexDesc.BindFlags = {"BIND_RENDER_TARGET", "BIND_SHADER_RESOURCE"}
	tex2DEpipolarCamSpaceZ = Texture.Create(TexDesc)
	tex2DEpipolarCamSpaceZSRV = tex2DEpipolarCamSpaceZ:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DEpipolarCamSpaceZRTV = tex2DEpipolarCamSpaceZ:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	tex2DEpipolarCamSpaceZSRV:SetSampler(LinearClampSampler)
	extResourceMapping["g_tex2DEpipolarCamSpaceZ"] = tex2DEpipolarCamSpaceZSRV

	-- MaxSamplesInSlice x NumSlices RGBA16F texture to store interpolated inscattered light,
	-- for every epipolar sample
	TexDesc.Format = EpipolarInsctrTexFmt
	flt16max = 65504
	TexDesc.ClearValue = {Format = TexDesc.Format, Color = {r=-flt16max, g=-flt16max, b=-flt16max, a=-flt16max}}
	tex2DEpipolarInscattering = Texture.Create(TexDesc)
	tex2DEpipolarInscatteringSRV = tex2DEpipolarInscattering:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DEpipolarInscatteringRTV = tex2DEpipolarInscattering:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	tex2DEpipolarInscatteringSRV:SetSampler(LinearClampSampler)
	extResourceMapping["g_tex2DScatteredColor"] = tex2DEpipolarInscatteringSRV

	-- MaxSamplesInSlice x NumSlices RGBA16F texture to store initial inscattered light,
	-- for every epipolar sample
	TexDesc.ClearValue = {Color = {r=0, g=0, b=0, a=0}}
	tex2DInitialScatteredLight = Texture.Create(TexDesc)
	tex2DInitialScatteredLightSRV = tex2DInitialScatteredLight:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DInitialScatteredLightRTV = tex2DInitialScatteredLight:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	tex2DInitialScatteredLightSRV:SetSampler(LinearClampSampler)
	extResourceMapping["g_tex2DInitialInsctrIrradiance"] = tex2DInitialScatteredLightSRV

	-- MaxSamplesInSlice x NumSlices depth stencil texture to mark samples for processing,
	-- for every epipolar sample
	TexDesc.Format = EpipolarImageDepthFmt
	TexDesc.BindFlags = "BIND_DEPTH_STENCIL"
	TexDesc.ClearValue = {Format = EpipolarImageDepthFmt, DepthStencil = {Depth=1, Stencil=0}}
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

	ResetShaderResourceBindings()

	-- Force garbage collection to make sure all graphics resources are released
	collectgarbage()

end

function CreateExtinctionTexture(NumSlices, MaxSamplesInSlice)
	local TexDesc = {
		Type = "RESOURCE_DIM_TEX_2D", 
		Width = MaxSamplesInSlice, Height = NumSlices,
		Format = EpipolarExtinctionFmt,
		MipLevels = 1,
		Usage = "USAGE_DEFAULT",
		BindFlags = {"BIND_RENDER_TARGET", "BIND_SHADER_RESOURCE"},
		ClearValue = {Color = {r=1, g=1, b=1, a=1}}
	}
	-- MaxSamplesInSlice x NumSlices RGBA8_UNORM texture to store extinction
	-- for every epipolar sample
	tex2DEpipolarExtinction = Texture.Create(TexDesc)
	tex2DEpipolarExtinctionSRV = tex2DEpipolarExtinction:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DEpipolarExtinctionRTV = tex2DEpipolarExtinction:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	tex2DEpipolarExtinctionSRV:SetSampler(LinearClampSampler)
	extResourceMapping["g_tex2DEpipolarExtinction"] = tex2DEpipolarExtinctionSRV

	ResetShaderResourceBindings()

	-- Force garbage collection to make sure all graphics resources are released
	collectgarbage()
end

function CreateLowResLuminanceTexture(LowResLuminanceMips)
	-- Create low-resolution texture to store image luminance
	local TexDesc = {
		Type = "RESOURCE_DIM_TEX_2D", 
		Width = 2^(LowResLuminanceMips-1), Height = 2^(LowResLuminanceMips-1),
		Format = LuminanceTexFmt,
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
	TexDesc.ClearValue = {Color = {r=0.1, g=0.1, b=0.1, a=0.1}}
	tex2DAverageLuminance = Texture.Create(TexDesc)
	tex2DAverageLuminanceSRV = tex2DAverageLuminance:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DAverageLuminanceRTV = tex2DAverageLuminance:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	-- Set intial luminance to 1
	Context.SetRenderTargets(tex2DAverageLuminanceRTV)
	Context.ClearRenderTarget(tex2DAverageLuminanceRTV, 0.1,0.1,0.1,0.1)
	tex2DAverageLuminanceSRV:SetSampler(LinearClampSampler)
	extResourceMapping["g_tex2DAverageLuminance"] = tex2DAverageLuminanceSRV

	ResetShaderResourceBindings()
end

function CreateSliceUVDirAndOriginTexture(NumEpipolarSlices, NumCascades)
	tex2DSliceUVDirAndOrigin = Texture.Create{
		Type = "RESOURCE_DIM_TEX_2D", 
		Width = NumEpipolarSlices, 
		Height = NumCascades,
		Format = SliceUVDirAndOriginTexFmt,
		MipLevels = 1,
		Usage = "USAGE_DEFAULT",
		BindFlags = {"BIND_RENDER_TARGET", "BIND_SHADER_RESOURCE"}
	}
	
	tex2DSliceUVDirAndOriginSRV = tex2DSliceUVDirAndOrigin:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DSliceUVDirAndOriginRTV = tex2DSliceUVDirAndOrigin:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	tex2DSliceUVDirAndOriginSRV:SetSampler(LinearClampSampler)
	extResourceMapping["g_tex2DSliceUVDirAndOrigin"] = tex2DSliceUVDirAndOriginSRV

	ResetShaderResourceBindings()

	-- Force garbage collection to make sure all graphics resources are released
	collectgarbage()
end

function CreateAmbientSkyLightTexture(AmbientSkyLightTexDim)
	tex2DAmbientSkyLight = Texture.Create{
		Type = "RESOURCE_DIM_TEX_2D", 
		Width = AmbientSkyLightTexDim, 
		Height = 1,
		Format = AmbientSkyLightTexFmt,
		MipLevels = 1,
		Usage = "USAGE_DEFAULT",
		BindFlags = {"BIND_RENDER_TARGET", "BIND_SHADER_RESOURCE"}
	}
	
	tex2DAmbientSkyLightSRV = tex2DAmbientSkyLight:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
	tex2DAmbientSkyLightRTV = tex2DAmbientSkyLight:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET")
	tex2DAmbientSkyLightSRV:SetSampler(LinearClampSampler)

	ResetShaderResourceBindings()
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
					Name = "LightScattering.lua:" .. Entry,
					DefaultVariableType = "SHADER_VARIABLE_TYPE_STATIC"
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

function CreateScreenSizeQuadPSO(_Name, PixelShader, DSSDesc, BSDesc, RTVFmts, DSVFmt)
	return PipelineState.Create
	{
		Name = _Name,
		GraphicsPipeline = 
		{
			RasterizerDesc = 
			{
				FillMode = "FILL_MODE_SOLID",
				CullMode = "CULL_MODE_NONE",
				FrontCounterClockwise = true
			},
			DepthStencilDesc = DSSDesc,
			BlendDesc = BSDesc,
			pVS = ScreenSizeQuadVS,
			pPS = PixelShader,
			RTVFormats = RTVFmts,
			DSVFormat = DSVFmt
		}
	}
end

function RenderScreenSizeQuad(PSO, StencilRef, NumQuads, SRB)

	Context.SetPipelineState(PSO)
	if SRB == nil then
		Context.CommitShaderResources("COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES")
	else
		Context.CommitShaderResources(SRB, "COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES")
	end

	Context.SetStencilRef(StencilRef)

	ScreenSizeQuadDrawAttrs.NumInstances = NumQuads or 1
	Context.Draw(ScreenSizeQuadDrawAttrs)
end


-----------------------------------[ Precomputing Optical Depth ]-----------------------------------
PrecomputeNetDensityToAtmTopPS = CreatePixelShader("Precomputation.fx", "PrecomputeNetDensityToAtmTopPS")
PrecomputeNetDensityToAtmTopPSO = CreateScreenSizeQuadPSO("PrecomputeNetDensityToAtmTop", PrecomputeNetDensityToAtmTopPS, DisableDepthDesc, DefaultBlendDesc, "TEX_FORMAT_RG32_FLOAT")
-- Bind required shader resources
-- All shader resources are static resources, so we bind them once directly to the shader
PrecomputeNetDensityToAtmTopPS:BindResources(extResourceMapping, "BIND_SHADER_RESOURCES_ALL_RESOLVED")

function PrecomputeNetDensityToAtmTop(NumPrecomputedHeights, NumPrecomputedAngles)
	
	if tex2DOccludedNetDensityToAtmTop == nil then
	    -- Create texture if it has not been created yet. 
		-- Do not recreate texture if it already exists as this may
		-- break static resource bindings.
		tex2DOccludedNetDensityToAtmTop = Texture.Create{
			Type = "RESOURCE_DIM_TEX_2D", 
			Width = NumPrecomputedHeights, Height = NumPrecomputedAngles,
			Format = "TEX_FORMAT_RG32_FLOAT",
			MipLevels = 1,
			Usage = "USAGE_DEFAULT",
			BindFlags = {"BIND_SHADER_RESOURCE", "BIND_RENDER_TARGET"}
		}

		-- Get SRV
		tex2DOccludedNetDensityToAtmTopSRV = tex2DOccludedNetDensityToAtmTop:GetDefaultView("TEXTURE_VIEW_SHADER_RESOURCE")
		-- Set linear sampler
		tex2DOccludedNetDensityToAtmTopSRV:SetSampler(LinearClampSampler)
		-- Add texture to the resource mapping
		extResourceMapping["g_tex2DOccludedNetDensityToAtmTop"] = tex2DOccludedNetDensityToAtmTopSRV
	end

	Context.SetRenderTargets( tex2DOccludedNetDensityToAtmTop:GetDefaultView("TEXTURE_VIEW_RENDER_TARGET") )
	-- Render quad
	RenderScreenSizeQuad(PrecomputeNetDensityToAtmTopPSO, 0)

	-- Force garbage collection to make sure all graphics resources are released
	collectgarbage()
end




function WindowResize(BackBufferWidth, BackBufferHeight)
	ptex2DCamSpaceZ = Texture.Create{
		Type = "RESOURCE_DIM_TEX_2D", Width = BackBufferWidth, Height = BackBufferHeight,
		Format = CamSpaceZFmt,
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



function CreateReconstructCameraSpaceZPSO(ReconstructCameraSpaceZPS)
	ReconstructCameraSpaceZPSO = CreateScreenSizeQuadPSO("ReconstructCameraSpaceZ", ReconstructCameraSpaceZPS, DisableDepthDesc, DefaultBlendDesc, CamSpaceZFmt)	
	ReconstructCameraSpaceZSRB = nil
end

function ReconstructCameraSpaceZ(DepthBufferSRV)
	if ReconstructCameraSpaceZSRB == nil then
		ReconstructCameraSpaceZSRB = ReconstructCameraSpaceZPSO:CreateShaderResourceBinding()
		ReconstructCameraSpaceZSRB:BindResources("SHADER_TYPE_PIXEL", extResourceMapping, {"BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED"})
	end

	-- Set dynamic variable g_tex2DDepthBuffer
	ReconstructCameraSpaceZSRB:GetVariable("SHADER_TYPE_PIXEL", "g_tex2DDepthBuffer"):Set(DepthBufferSRV)
	Context.SetRenderTargets(ptex2DCamSpaceZRTV)
	RenderScreenSizeQuad(ReconstructCameraSpaceZPSO, 0, 1, ReconstructCameraSpaceZSRB)
end



function CreateRenderSliceEndPointsPSO(RenderSliceEndPointsPS)
	RenderSliceEndPointsPSO = CreateScreenSizeQuadPSO("RenderSliceEndPoints", RenderSliceEndPointsPS, DisableDepthDesc, DefaultBlendDesc, SliceEndpointsFmt)	
end

function RenderSliceEndPoints()
	Context.SetRenderTargets(tex2DSliceEndpointsRTV)
	RenderScreenSizeQuad(RenderSliceEndPointsPSO, 0)
end


function CreateRenderCoordinateTexturePSO(RenderCoordinateTexturePS)
	RenderCoordinateTexturePSO = CreateScreenSizeQuadPSO("RenderCoordinateTexture", RenderCoordinateTexturePS, IncStencilAlwaysDSSDesc, DefaultBlendDesc, {CoordinateTexFmt, EpipolarCamSpaceZFmt}, EpipolarImageDepthFmt)	
	RenderCoordinateTextureSRB = nil
end

function RenderCoordinateTexture()
	if RenderCoordinateTextureSRB == nil then
		RenderCoordinateTextureSRB = RenderCoordinateTexturePSO:CreateShaderResourceBinding()
		RenderCoordinateTextureSRB:BindResources("SHADER_TYPE_PIXEL", extResourceMapping, {"BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED", "BIND_SHADER_RESOURCES_ALL_RESOLVED"})
	end

	Context.SetRenderTargets(tex2DCoordinateTextureRTV, tex2DEpipolarCamSpaceZRTV, tex2DEpipolarImageDSV)
	-- Clear both render targets with values that can't be correct projection space coordinates and camera space Z:
	Context.ClearRenderTarget(tex2DCoordinateTextureRTV, -1e+30, -1e+30, -1e+30, -1e+30)
	Context.ClearRenderTarget(tex2DEpipolarCamSpaceZRTV, -1e+30)
	Context.ClearDepthStencil(tex2DEpipolarImageDSV, 1.0, 0)
    -- Depth stencil state is configured to always increment stencil value. If coordinates are outside the screen,
    -- the pixel shader discards the pixel and stencil value is left untouched. All such pixels will be skipped from
    -- further processing
	RenderScreenSizeQuad(RenderCoordinateTexturePSO, 0, 1, RenderCoordinateTextureSRB)
end



function CreateRenderCoarseUnshadowedInsctrAndExtinctionPSO(RenderCoarseUnshadowedInctrPS)
	RenderCoarseUnshadowedInctrPSO = CreateScreenSizeQuadPSO("RenderCoarseUnshadowedInsctrAndExtinction", RenderCoarseUnshadowedInctrPS, StencilEqKeepStencilDSSDesc, DefaultBlendDesc, {EpipolarInsctrTexFmt, EpipolarExtinctionFmt}, EpipolarImageDepthFmt)
	RenderCoarseUnshadowedInctrSRB = nil
end

function CreateRenderCoarseUnshadowedInsctrPSO(RenderCoarseUnshadowedInctrPS)
	RenderCoarseUnshadowedInctrPSO = CreateScreenSizeQuadPSO("RenderCoarseUnshadowedInctr", RenderCoarseUnshadowedInctrPS, StencilEqKeepStencilDSSDesc, DefaultBlendDesc, EpipolarInsctrTexFmt, EpipolarImageDepthFmt)
	RenderCoarseUnshadowedInctrSRB = nil
end

function RenderCoarseUnshadowedInctr()
	if RenderCoarseUnshadowedInctrSRB == nil then
		RenderCoarseUnshadowedInctrSRB = RenderCoarseUnshadowedInctrPSO:CreateShaderResourceBinding()
		RenderCoarseUnshadowedInctrSRB:BindResources("SHADER_TYPE_PIXEL", extResourceMapping, {"BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED", "BIND_SHADER_RESOURCES_ALL_RESOLVED"})
	end

	RenderScreenSizeQuad(RenderCoarseUnshadowedInctrPSO, 1, 1, RenderCoarseUnshadowedInctrSRB)
end



function CreateMarkRayMarchingSamplesPSO(MarkRayMarchingSamplesPS)
	MarkRayMarchingSamplesPSO = CreateScreenSizeQuadPSO("MarkRayMarchingSamples", MarkRayMarchingSamplesPS, StencilEqIncStencilDSSDesc, DefaultBlendDesc, {}, EpipolarImageDepthFmt)
	MarkRayMarchingSamplesSRB = nil
end

function MarkRayMarchingSamples()
	if MarkRayMarchingSamplesSRB == nil then
		MarkRayMarchingSamplesSRB = MarkRayMarchingSamplesPSO:CreateShaderResourceBinding()
		MarkRayMarchingSamplesSRB:BindResources("SHADER_TYPE_PIXEL", extResourceMapping, {"BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED", "BIND_SHADER_RESOURCES_ALL_RESOLVED"})
	end

	Context.SetRenderTargets(tex2DEpipolarImageDSV)
	RenderScreenSizeQuad(MarkRayMarchingSamplesPSO, 1, 1, MarkRayMarchingSamplesSRB)
end



function CreateRenderSliceUVDirAndOriginPSO(RenderSliceUVDirAndOriginPS)
	RenderSliceUVDirAndOriginPSO = CreateScreenSizeQuadPSO("RenderSliceUVDirAndOrigin", RenderSliceUVDirAndOriginPS, DisableDepthDesc, DefaultBlendDesc, SliceUVDirAndOriginTexFmt)	
	RenderSliceUVDirAndOriginSRB = nil
end

function RenderSliceUVDirAndOrigin()
	if RenderSliceUVDirAndOriginSRB == nil then
		RenderSliceUVDirAndOriginSRB = RenderSliceUVDirAndOriginPSO:CreateShaderResourceBinding()
		RenderSliceUVDirAndOriginSRB:BindResources("SHADER_TYPE_PIXEL", extResourceMapping, {"BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED", "BIND_SHADER_RESOURCES_ALL_RESOLVED"})
	end

	Context.SetRenderTargets(tex2DSliceUVDirAndOriginRTV)
	RenderScreenSizeQuad(RenderSliceUVDirAndOriginPSO, 0, 1, RenderSliceUVDirAndOriginSRB)
end


function CreateInitMinMaxShadowMapPSO(InitMinMaxShadowMapPS, MinMaxShadowMapFmt)
	InitMinMaxShadowMapPSO = CreateScreenSizeQuadPSO("InitMinMaxShadowMap", InitMinMaxShadowMapPS, DisableDepthDesc, DefaultBlendDesc, MinMaxShadowMapFmt)
	InitMinMaxShadowMapSRB = nil
end

function InitMinMaxShadowMap(ShadowMapSRV)
	if InitMinMaxShadowMapSRB == nil then
		InitMinMaxShadowMapSRB = InitMinMaxShadowMapPSO:CreateShaderResourceBinding()
		InitMinMaxShadowMapSRB:BindResources("SHADER_TYPE_PIXEL", extResourceMapping, {"BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED", "BIND_SHADER_RESOURCES_ALL_RESOLVED"})
	end

	-- Set dynamic variable g_tex2DLightSpaceDepthMap
	InitMinMaxShadowMapSRB:GetVariable("SHADER_TYPE_PIXEL", "g_tex2DLightSpaceDepthMap"):Set(ShadowMapSRV)
	RenderScreenSizeQuad(InitMinMaxShadowMapPSO, 0, 1, InitMinMaxShadowMapSRB)
end



function CreateComputeMinMaxShadowMapLevelPSO(ComputeMinMaxShadowMapLevelPS, MinMaxShadowMapFmt)
	ComputeMinMaxShadowMapLevelPSO = CreateScreenSizeQuadPSO("ComputeMinMaxShadowMapLevel", ComputeMinMaxShadowMapLevelPS, DisableDepthDesc, DefaultBlendDesc, MinMaxShadowMapFmt)
end

function ComputeMinMaxShadowMapLevel(SRB)
	RenderScreenSizeQuad(ComputeMinMaxShadowMapLevelPSO, 0, 1, SRB)
end



function ClearInitialScatteredLight()
	-- On GL, we need to bind render target to pipeline to clear it
	Context.SetRenderTargets(tex2DInitialScatteredLightRTV)
	Context.ClearRenderTarget(tex2DInitialScatteredLightRTV, 0,0,0,0)
end



RayMarchPSO = {}
function CreateRayMarchPSO(RayMarchPS, Use1DMinMaxTree)
	RayMarchPSO[Use1DMinMaxTree] = CreateScreenSizeQuadPSO("RayMarch", RayMarchPS, StencilEqKeepStencilDSSDesc, AdditiveBlendBSDesc, EpipolarInsctrTexFmt, EpipolarImageDepthFmt )
	RayMarchSRB = nil
end

function RayMarch(Use1DMinMaxTree, NumQuads)
	if RayMarchSRB == nil then RayMarchSRB = {} end

	if RayMarchSRB[Use1DMinMaxTree] == nil then
		RayMarchSRB[Use1DMinMaxTree] = RayMarchPSO[Use1DMinMaxTree]:CreateShaderResourceBinding()
		RayMarchSRB[Use1DMinMaxTree]:BindResources("SHADER_TYPE_PIXEL", extResourceMapping, {"BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED", "BIND_SHADER_RESOURCES_ALL_RESOLVED"})
	end

	Context.SetRenderTargets(tex2DInitialScatteredLightRTV, tex2DEpipolarImageDSV)
	RenderScreenSizeQuad(RayMarchPSO[Use1DMinMaxTree], 2, NumQuads, RayMarchSRB[Use1DMinMaxTree])
end



function CreateInterpolateIrradiancePSO(InterpolateIrradiancePS)
	InterpolateIrradiancePSO = CreateScreenSizeQuadPSO("InterpolateIrradiance", InterpolateIrradiancePS, DisableDepthDesc, DefaultBlendDesc, EpipolarInsctrTexFmt)
	InterpolateIrradianceSRB = nil
end

function InterpolateIrradiance()
	if InterpolateIrradianceSRB == nil then
		InterpolateIrradianceSRB = InterpolateIrradiancePSO:CreateShaderResourceBinding()
		InterpolateIrradianceSRB:BindResources("SHADER_TYPE_PIXEL", extResourceMapping, {"BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED", "BIND_SHADER_RESOURCES_ALL_RESOLVED"})
	end

	Context.SetRenderTargets(tex2DEpipolarInscatteringRTV)
	RenderScreenSizeQuad(InterpolateIrradiancePSO, 0, 1, InterpolateIrradianceSRB)
end


function CreateUnwarpAndRenderLuminancePSO(UnwarpAndRenderLuminancePS)
	UnwarpAndRenderLuminancePSO = CreateScreenSizeQuadPSO("UnwarpAndRenderLuminance", UnwarpAndRenderLuminancePS, DisableDepthDesc, DefaultBlendDesc, LuminanceTexFmt)
	UnwarpAndRenderLuminanceSRB = nil
end

function UnwarpAndRenderLuminance(SrcColorBufferSRV)
	if UnwarpAndRenderLuminanceSRB == nil then
		UnwarpAndRenderLuminanceSRB= UnwarpAndRenderLuminancePSO:CreateShaderResourceBinding()
		UnwarpAndRenderLuminanceSRB:BindResources("SHADER_TYPE_PIXEL", extResourceMapping, "BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED")
	end

	-- Set dynamic variable g_tex2DColorBuffer
	UnwarpAndRenderLuminanceSRB:GetVariable("SHADER_TYPE_PIXEL", "g_tex2DColorBuffer"):Set(SrcColorBufferSRV)

	-- Disable depth testing - we need to render the entire image in low resolution
	RenderScreenSizeQuad(UnwarpAndRenderLuminancePSO, 0, 1, UnwarpAndRenderLuminanceSRB)
end


function CreateUnwarpEpipolarScatteringPSO(UnwarpEpipolarScatteringPS)
	UnwarpEpipolarScatteringPSO = CreateScreenSizeQuadPSO("UnwarpEpipolarScattering", UnwarpEpipolarScatteringPS, EnableDepthDesc, DefaultBlendDesc, MainBackBufferFmt, MainDepthBufferFmt)
	UnwarpEpipolarScatteringSRB = nil
end

function UnwarpEpipolarScattering(SrcColorBufferSRV)
	if UnwarpEpipolarScatteringSRB == nil then
		UnwarpEpipolarScatteringSRB= UnwarpEpipolarScatteringPSO:CreateShaderResourceBinding()
		UnwarpEpipolarScatteringSRB:BindResources("SHADER_TYPE_PIXEL", extResourceMapping, "BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED")
	end

	-- Set dynamic variable g_tex2DColorBuffer
	UnwarpEpipolarScatteringSRB:GetVariable("SHADER_TYPE_PIXEL", "g_tex2DColorBuffer"):Set(SrcColorBufferSRV )

	-- Enable depth testing to write 0.0 to the depth buffer. All pixel that require 
	-- inscattering correction (if enabled) will be discarded, so that 1.0 will be retained
	-- This 1.0 will then be used to perform inscattering correction
	RenderScreenSizeQuad(UnwarpEpipolarScatteringPSO, 0, 1, UnwarpEpipolarScatteringSRB)
end

FixInscatteringAtDepthBreaksPSO = {}
function CreateFixInscatteringAtDepthBreaksPSO(FixInsctrAtDepthBreaksPS, FixInsctrAtDepthBreaksLumOnlyPS)
	-- Luminance Only
	-- Disable depth and stencil tests to render all pixels
	-- Use default blend state to overwrite old luminance values
	FixInscatteringAtDepthBreaksPSO[0] = CreateScreenSizeQuadPSO("FixInsctrAtDepthBreaksLumOnly", FixInsctrAtDepthBreaksLumOnlyPS, DisableDepthDesc, DefaultBlendDesc, LuminanceTexFmt)

	-- Fix Inscattering
	-- Depth breaks are marked with 1.0 in depth, so we enable depth test
	-- to render only pixels that require correction
	-- Use default blend state - the rendering is always done in single pass
	FixInscatteringAtDepthBreaksPSO[1] = CreateScreenSizeQuadPSO("FixInsctrAtDepthBreaks", FixInsctrAtDepthBreaksPS, EnableDepthDesc, DefaultBlendDesc, MainBackBufferFmt, MainDepthBufferFmt)

	-- Full Screen Ray Marching
	-- Disable depth and stencil tests since we are performing 
	-- full screen ray marching
	-- Use default blend state - the rendering is always done in single pass
	FixInscatteringAtDepthBreaksPSO[2] = CreateScreenSizeQuadPSO("FixInsctrAtDepthBreaks", FixInsctrAtDepthBreaksPS, DisableDepthDesc, DefaultBlendDesc, MainBackBufferFmt)
	FixInscatteringAtDepthBreaksSRB = nil
end

function FixInscatteringAtDepthBreaks(Mode, SrcColorBufferSRV)
	if FixInscatteringAtDepthBreaksSRB == nil then FixInscatteringAtDepthBreaksSRB = {} end
	if FixInscatteringAtDepthBreaksSRB[Mode] == nil then
		FixInscatteringAtDepthBreaksSRB[Mode]= FixInscatteringAtDepthBreaksPSO[Mode]:CreateShaderResourceBinding()
		FixInscatteringAtDepthBreaksSRB[Mode]:BindResources("SHADER_TYPE_PIXEL", extResourceMapping, "BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED")
	end

	-- Set dynamic variable g_tex2DColorBuffer
	FixInscatteringAtDepthBreaksSRB[Mode]:GetVariable("SHADER_TYPE_PIXEL", "g_tex2DColorBuffer"):Set(SrcColorBufferSRV )

	RenderScreenSizeQuad(FixInscatteringAtDepthBreaksPSO[Mode], 0, 1, FixInscatteringAtDepthBreaksSRB[Mode])
end



function CreateUpdateAverageLuminancePSO(UpdateAverageLuminancePS)
	UpdateAverageLuminancePSO = CreateScreenSizeQuadPSO("UpdateAverageLuminance", UpdateAverageLuminancePS, DisableDepthDesc, AlphaBlendBSDesc, LuminanceTexFmt)
	UpdateAverageLuminanceSRB = nil
end

function UpdateAverageLuminance()
	if UpdateAverageLuminanceSRB == nil then
		UpdateAverageLuminanceSRB= UpdateAverageLuminancePSO:CreateShaderResourceBinding()
		UpdateAverageLuminanceSRB:BindResources("SHADER_TYPE_PIXEL", extResourceMapping, {"BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED", "BIND_SHADER_RESOURCES_ALL_RESOLVED"})
	end

	Context.SetRenderTargets(tex2DAverageLuminanceRTV)
	RenderScreenSizeQuad(UpdateAverageLuminancePSO, 0, 1, UpdateAverageLuminanceSRB)
end

SampleLocationsDrawAttrs = DrawAttribs.Create{
    NumVertices = 4,
    Topology = "PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP"
}


function CreateRenderSampleLocationsPSO(RenderSampleLocationsVS, RenderSampleLocationsPS)
	RenderSampleLocationsPSO = PipelineState.Create
	{
		GraphicsPipeline = 
		{
			RasterizerDesc = 
			{
				FillMode = "FILL_MODE_SOLID",
				CullMode = "CULL_MODE_NONE",
				FrontCounterClockwise = true
			},
			DepthStencilDesc = DisableDepthDesc,
			BlendDesc = AlphaBlendBSDesc,
			pVS = RenderSampleLocationsVS,
			pPS = RenderSampleLocationsPS,
			RTVFormats = MainBackBufferFmt
		}
	}
	RenderSampleLocationsSRB = nil
end

function RenderSampleLocations(TotalSamples)
	if RenderSampleLocationsSRB == nil then
		RenderSampleLocationsSRB= RenderSampleLocationsPSO:CreateShaderResourceBinding()
		RenderSampleLocationsSRB:BindResources("SHADER_TYPE_VERTEX", extResourceMapping, {"BIND_SHADER_RESOURCES_UPDATE_UNRESOLVED", "BIND_SHADER_RESOURCES_ALL_RESOLVED"})
	end

	Context.SetPipelineState(RenderSampleLocationsPSO)
	Context.CommitShaderResources(RenderSampleLocationsSRB, "COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES")
	SampleLocationsDrawAttrs.NumInstances = TotalSamples
	Context.Draw(SampleLocationsDrawAttrs)
end

SunVS = CreateVertexShader("Sun.fx", "SunVS")
SunPS = CreatePixelShader("Sun.fx", "SunPS")

RenderSunPSO = PipelineState.Create
{
	Name = "Render Sun",
	GraphicsPipeline = 
	{
		RasterizerDesc = 
		{
			FillMode = "FILL_MODE_SOLID",
			CullMode = "CULL_MODE_NONE",
			FrontCounterClockwise = true
		},
		DepthStencilDesc = CmpEqNoWritesDSSDesc,
		BlendDesc = DefaultBlendDesc,
		pVS = SunVS,
		pPS = SunPS,
		RTVFormats =  OffscreenBackBufferFmt,
		DSVFormat = MainDepthBufferFmt
	}
}

function RenderSun()
	Context.SetPipelineState(RenderSunPSO)

	SunVS:BindResources(extResourceMapping, "BIND_SHADER_RESOURCES_ALL_RESOLVED")
	SunPS:BindResources(extResourceMapping, "BIND_SHADER_RESOURCES_ALL_RESOLVED")
	
	Context.CommitShaderResources("COMMIT_SHADER_RESOURCES_FLAG_TRANSITION_RESOURCES")

	ScreenSizeQuadDrawAttrs.NumInstances = 1
	Context.Draw(ScreenSizeQuadDrawAttrs)
end

function CreatePrecomputeAmbientSkyLightPSO(PrecomputeAmbientSkyLightPS)
	PrecomputeAmbientSkyLightPSO = CreateScreenSizeQuadPSO("PrecomputeAmbientSkyLight", PrecomputeAmbientSkyLightPS, DisableDepthDesc, DefaultBlendDesc, AmbientSkyLightTexFmt)
end

function PrecomputeAmbientSkyLight()
	RenderScreenSizeQuad(PrecomputeAmbientSkyLightPSO, 0)
end
