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

set(ALL_SHADERS ${ATMOSPHERE_SHADERS} ${TERRAIN_SHADERS} ${SHADERS})
set(ASSETS ${TERRAIN_TEXTURES} ${TERRAIN_TILES})
add_sample("AtmosphereSample" "${SOURCE}" "${INCLUDE}" "${ALL_SHADERS}" "${ASSETS}")

if(PLATFORM_UNIVERSAL_WINDOWS)
	set_property(SOURCE ${ATMOSPHERE_SHADERS} PROPERTY VS_DEPLOYMENT_LOCATION "shaders")
	set_property(SOURCE ${ATMOSPHERE_SHADERS} PROPERTY VS_DEPLOYMENT_LOCATION "shaders/atmosphere")
	set_property(SOURCE ${TERRAIN_SHADERS} PROPERTY VS_DEPLOYMENT_LOCATION "shaders/terrain")
	set_property(SOURCE ${SHADERS} PROPERTY VS_DEPLOYMENT_LOCATION "shaders")
	set_property(SOURCE ${TERRAIN_TEXTURES} PROPERTY VS_DEPLOYMENT_LOCATION "Terrain")
	set_property(SOURCE ${TERRAIN_TILES} PROPERTY VS_DEPLOYMENT_LOCATION "Terrain/Tiles")
elseif(PLATFORM_MACOS)
	set_property(SOURCE ${ATMOSPHERE_SHADERS} PROPERTY MACOSX_PACKAGE_LOCATION "Resources/shaders")
	set_property(SOURCE ${ATMOSPHERE_SHADERS} PROPERTY MACOSX_PACKAGE_LOCATION "Resources/shaders/atmosphere")
	set_property(SOURCE ${TERRAIN_SHADERS} PROPERTY MACOSX_PACKAGE_LOCATION "Resources/shaders/terrain")
	set_property(SOURCE ${SHADERS} PROPERTY MACOSX_PACKAGE_LOCATION "Resources/shaders")
	set_property(SOURCE ${TERRAIN_TEXTURES} PROPERTY MACOSX_PACKAGE_LOCATION "Resources/Terrain")
	set_property(SOURCE ${TERRAIN_TILES} PROPERTY MACOSX_PACKAGE_LOCATION "Resources/Terrain/Tiles")	
endif()

target_include_directories(AtmosphereSample
PRIVATE
	src
	src/Terrain
	assets/shaders
)

target_link_libraries(AtmosphereSample
PRIVATE
    GraphicsAccessories
	TextureLoader
)

source_group("assets\\shaders\\atmosphere" FILES ${ATMOSPHERE_SHADERS})
source_group("assets\\shaders\\terrain" FILES ${TERRAIN_SHADERS})
source_group("assets\\shaders" FILES ${SHADERS})
source_group("assets\\Terrain" FILES ${TERRAIN_TEXTURES})
source_group("assets\\Terrain\\Tiles" FILES ${TERRAIN_TILES})
