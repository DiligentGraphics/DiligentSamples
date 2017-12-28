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
	assets/shaders/atmosphere/AtmosphereShadersCommon.fxh
	assets/shaders/atmosphere/CoarseInsctr.fx
	assets/shaders/atmosphere/InterpolateIrradiance.fx
	assets/shaders/atmosphere/LookUpTables.fxh
	assets/shaders/atmosphere/MarkRayMarchingSamples.fx
	assets/shaders/atmosphere/MinMaxBinTree.fx
	assets/shaders/atmosphere/Precomputation.fx
	assets/shaders/atmosphere/RayMarch.fx
	assets/shaders/atmosphere/ReconstructCameraSpaceZ.fx
	assets/shaders/atmosphere/RefineSampleLocations.fx
	assets/shaders/atmosphere/RenderCoordinateTexture.fx
	assets/shaders/atmosphere/RenderSampling.fx
	assets/shaders/atmosphere/RenderSliceEndPoints.fx
	assets/shaders/atmosphere/ScatteringIntegrals.fxh
	assets/shaders/atmosphere/ScreenSizeQuadVS.fx
	assets/shaders/atmosphere/Sun.fx
	assets/shaders/atmosphere/ToneMapping.fxh
	assets/shaders/atmosphere/UnshadowedScattering.fxh
	assets/shaders/atmosphere/UnwarpEpipolarScattering.fx
	assets/shaders/atmosphere/UpdateAverageLuminance.fx
)

set(TERRAIN_SHADERS
	assets/shaders/terrain/GenerateNormalMapPS.fx
	assets/shaders/terrain/HemispherePS.fx
	assets/shaders/terrain/HemisphereVS.fx
	assets/shaders/terrain/HemisphereZOnlyVS.fx
	assets/shaders/terrain/ScreenSizeQuadVS.fx
	assets/shaders/terrain/TerrainShadersCommon.fxh
)

set(SHADERS
	assets/shaders/HostSharedTerrainStructs.fxh
	assets/shaders/LightScattering.lua
	assets/shaders/Structures.fxh
	assets/shaders/Terrain.lua
)

set(TERRAIN_TEXTURES
	assets/Terrain/HeightMap.tif
	assets/Terrain/Mask.png
)

set(TERRAIN_TILES
	assets/Terrain/Tiles/cliff_DM.dds
	assets/Terrain/Tiles/cliff_NM.dds
	assets/Terrain/Tiles/grass_DM.dds
	assets/Terrain/Tiles/grass_NM.dds
	assets/Terrain/Tiles/grassDark_DM.dds
	assets/Terrain/Tiles/gravel_DM.dds
	assets/Terrain/Tiles/gravel_NM.dds
	assets/Terrain/Tiles/snow_DM.dds
	assets/Terrain/Tiles/Snow_NM.jpg
)

set(ASSETS ${ATMOSPHERE_SHADERS} ${TERRAIN_SHADERS} ${SHADERS} ${TERRAIN_TEXTURES} ${TERRAIN_TILES})

if(PLATFORM_WIN32 OR PLATFORM_UNIVERSAL_WINDOWS)
	add_executable(AtmosphereSample WIN32 ${SOURCE} ${INCLUDE} ${ASSETS})

	if(PLATFORM_WIN32)
		set_target_properties(AtmosphereSample PROPERTIES 
			VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
		)
		copy_required_dlls(AtmosphereSample)
	endif()

elseif(PLATFORM_ANDROID)

	list(APPEND SOURCE src/AndroidMain.cpp)
	add_library(AtmosphereSample SHARED ${SOURCE} ${INCLUDE})

elseif(PLATFORM_LINUX)

	add_executable(AtmosphereSample ${SOURCE} ${INCLUDE})
	
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
	assets/shaders
)

target_link_libraries(AtmosphereSample
PRIVATE
	BuildSettings
	GraphicsAccessories
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
