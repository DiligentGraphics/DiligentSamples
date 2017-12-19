cmake_minimum_required (VERSION 3.6)

project(AtmosphereSample CXX)

set(SOURCE
	src/AtmosphereSample.cpp
	src/Terrain/EarthHemisphere.cpp
	src/Terrain/ElevationDataSource.cpp
	src/LightSctrPostProcess.cpp
)

set(INCLUDE
	src/AtmosphereSample.h
	src/Terrain/DynamicQuadTreeNode.h
	src/Terrain/EarthHemisphere.h
	src/Terrain/ElevationDataSource.h
	src/Terrain/HierarchyArray.h
	src/LightSctrPostProcess.h
	src/pch.h
)

set(ATMOSPHERE_SHADERS
	build/assets/shaders/atmosphere/AtmosphereShadersCommon.fxh
	build/assets/shaders/atmosphere/CoarseInsctr.fx
	build/assets/shaders/atmosphere/InterpolateIrradiance.fx
	build/assets/shaders/atmosphere/LookUpTables.fxh
	build/assets/shaders/atmosphere/MarkRayMarchingSamples.fx
	build/assets/shaders/atmosphere/MinMaxBinTree.fx
	build/assets/shaders/atmosphere/Precomputation.fx
	build/assets/shaders/atmosphere/RayMarch.fx
	build/assets/shaders/atmosphere/ReconstructCameraSpaceZ.fx
	build/assets/shaders/atmosphere/RefineSampleLocations.fx
	build/assets/shaders/atmosphere/RenderCoordinateTexture.fx
	build/assets/shaders/atmosphere/RenderSampling.fx
	build/assets/shaders/atmosphere/RenderSliceEndPoints.fx
	build/assets/shaders/atmosphere/ScatteringIntegrals.fxh
	build/assets/shaders/atmosphere/ScreenSizeQuadVS.fx
	build/assets/shaders/atmosphere/Sun.fx
	build/assets/shaders/atmosphere/ToneMapping.fxh
	build/assets/shaders/atmosphere/UnshadowedScattering.fxh
	build/assets/shaders/atmosphere/UnwarpEpipolarScattering.fx
	build/assets/shaders/atmosphere/UpdateAverageLuminance.fx
)

set(TERRAIN_SHADERS
	build/assets/shaders/terrain/GenerateNormalMapPS.fx
	build/assets/shaders/terrain/HemispherePS.fx
	build/assets/shaders/terrain/HemisphereVS.fx
	build/assets/shaders/terrain/HemisphereZOnlyVS.fx
	build/assets/shaders/terrain/ScreenSizeQuadVS.fx
	build/assets/shaders/terrain/TerrainShadersCommon.fxh
)

set(SHADERS
	build/assets/shaders/HostSharedTerrainStructs.fxh
	build/assets/shaders/LightScattering.lua
	build/assets/shaders/Structures.fxh
	build/assets/shaders/Terrain.lua
)

set(TERRAIN_TEXTURES
	build/assets/Terrain/HeightMap.tif
	build/assets/Terrain/Mask.png
)

set(TERRAIN_TILES
	build/assets/Terrain/Tiles/cliff_DM.dds
	build/assets/Terrain/Tiles/cliff_NM.dds
	build/assets/Terrain/Tiles/grass_DM.dds
	build/assets/Terrain/Tiles/grass_NM.dds
	build/assets/Terrain/Tiles/grassDark_DM.dds
	build/assets/Terrain/Tiles/gravel_DM.dds
	build/assets/Terrain/Tiles/gravel_NM.dds
	build/assets/Terrain/Tiles/snow_DM.dds
	build/assets/Terrain/Tiles/Snow_NM.jpg
)

set(ASSETS ${ATMOSPHERE_SHADERS} ${TERRAIN_SHADERS} ${SHADERS} ${TERRAIN_TEXTURES} ${TERRAIN_TILES})

if(PLATFORM_WIN32 OR PLATFORM_UNIVERSAL_WINDOWS)
	add_executable(AtmosphereSample WIN32 ${SOURCE} ${INCLUDE} ${ASSETS})

	if(PLATFORM_WIN32)
		set_target_properties(AtmosphereSample PROPERTIES 
			VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build/assets"
		)
		copy_required_dlls(AtmosphereSample)
	endif()

elseif(PLATFORM_ANDROID)

	list(APPEND SOURCE src/AndroidMain.cpp)
	add_library(AtmosphereSample SHARED ${SOURCE} ${INCLUDE})

#elseif(APPLE)
#	add_executable(AtmosphereSample MACOSX_BUNDLE Main.cpp)
#elseif(UNIX)
#	add_executable(AtmosphereSample Main.cpp)
else()
	message(FATAL_ERROR "Unknown platform")
endif()

if(PLATFORM_UNIVERSAL_WINDOWS)
	get_sample_base_uwp_source(UWP_SOURCE UWP_INCLUDE UWP_INCLUDE_DIR)
	source_group("UWP\\src" FILES ${UWP_SOURCE})
	source_group("UWP\\include" FILES ${UWP_INCLUDE})
	target_include_directories(AtmosphereSample PRIVATE ${UWP_INCLUDE_DIR})
	target_sources(AtmosphereSample PRIVATE ${UWP_SOURCE} ${UWP_INCLUDE})

	set_property(SOURCE ${ASSETS} PROPERTY VS_DEPLOYMENT_CONTENT 1)
	set_property(SOURCE ${ATMOSPHERE_SHADERS} PROPERTY VS_DEPLOYMENT_LOCATION "assets/shaders")
	set_property(SOURCE ${ATMOSPHERE_SHADERS} PROPERTY VS_DEPLOYMENT_LOCATION "assets/shaders/atmosphere")
	set_property(SOURCE ${TERRAIN_SHADERS} PROPERTY VS_DEPLOYMENT_LOCATION "assets/shaders/terrain")
	set_property(SOURCE ${SHADERS} PROPERTY VS_DEPLOYMENT_LOCATION "assets/shaders")
	set_property(SOURCE ${TERRAIN_TEXTURES} PROPERTY VS_DEPLOYMENT_LOCATION "assets/Terrain")
	set_property(SOURCE ${TERRAIN_TILES} PROPERTY VS_DEPLOYMENT_LOCATION "assets/Terrain/Tiles")
endif()

target_include_directories(AtmosphereSample
PRIVATE
	src
	src/Terrain
	build/assets/shaders
)

target_link_libraries(AtmosphereSample
PRIVATE
	SampleBase 
	Lua
	TextureLoader
	RenderScript
)

target_link_libraries(AtmosphereSample ${LINK_LIBRARIES})
set_common_target_properties(AtmosphereSample)

if(MSVC)
	# Disable MSVC-specific warnings
	# - w4201: nonstandard extension used: nameless struct/unio
	target_compile_options(AtmosphereSample PRIVATE /wd4201)
endif()

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("assets\\shaders\\atmosphere" FILES ${ATMOSPHERE_SHADERS})
source_group("assets\\shaders\\terrain" FILES ${TERRAIN_SHADERS})
source_group("assets\\shaders" FILES ${SHADERS})
source_group("assets\\Terrain" FILES ${TERRAIN_TEXTURES})
source_group("assets\\Terrain\\Tiles" FILES ${TERRAIN_TILES})

set_target_properties(AtmosphereSample PROPERTIES
	FOLDER Samples
)
