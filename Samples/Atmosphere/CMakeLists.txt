cmake_minimum_required (VERSION 3.6)

project(AtmosphereSample CXX)

set(SOURCE
    src/AtmosphereSample.cpp
    src/Terrain/EarthHemisphere.cpp
    src/Terrain/ElevationDataSource.cpp
)

set(INCLUDE
    src/AtmosphereSample.h
    src/Terrain/DynamicQuadTreeNode.h
    src/Terrain/EarthHemisphere.h
    src/Terrain/ElevationDataSource.h
    src/Terrain/HierarchyArray.h
    src/pch.h
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
	../../../DiligentFX/Shaders/PostProcess/EpipolarLightScattering/public/Structures.fxh
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

set(ALL_SHADERS ${TERRAIN_SHADERS} ${SHADERS})
set(ASSETS ${TERRAIN_TEXTURES} ${TERRAIN_TILES})
add_sample_app("AtmosphereSample" "Samples" "${SOURCE}" "${INCLUDE}" "${ALL_SHADERS}" "${ASSETS}")
target_link_libraries(AtmosphereSample PRIVATE RenderScript)

set_source_files_properties(${TERRAIN_SHADERS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "shaders/terrain"
    MACOSX_PACKAGE_LOCATION "Resources/shaders/terrain"
)
set_source_files_properties(${SHADERS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "shaders"
    MACOSX_PACKAGE_LOCATION "Resources/shaders"
)
set_source_files_properties(${TERRAIN_TEXTURES} PROPERTIES
    VS_DEPLOYMENT_LOCATION "Terrain"
    MACOSX_PACKAGE_LOCATION "Resources/Terrain"
)
set_source_files_properties(${TERRAIN_TILES} PROPERTIES
    VS_DEPLOYMENT_LOCATION "Terrain/Tiles"
    MACOSX_PACKAGE_LOCATION "Resources/Terrain/Tiles"
)

target_include_directories(AtmosphereSample
PRIVATE
    src
    src/Terrain
    ../../../DiligentFX/Shaders/PostProcess/EpipolarLightScattering/public
    assets/shaders
)

target_link_libraries(AtmosphereSample
PRIVATE
    GraphicsAccessories
    TextureLoader
    DiligentFX
)

add_custom_command(TARGET AtmosphereSample PRE_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../../../DiligentFX/Shaders/PostProcess/EpipolarLightScattering/public/Structures.fxh" "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders")

source_group("assets\\shaders\\terrain" FILES ${TERRAIN_SHADERS})
source_group("assets\\shaders" FILES ${SHADERS})
source_group("assets\\Terrain" FILES ${TERRAIN_TEXTURES})
source_group("assets\\Terrain\\Tiles" FILES ${TERRAIN_TILES})
