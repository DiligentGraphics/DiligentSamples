cmake_minimum_required (VERSION 3.10)

project(Atmosphere CXX)

set(SOURCE
    src/AtmosphereSample.cpp
    src/Terrain/EarthHemisphere.cpp
    src/Terrain/ElevationDataSource.cpp
)

set(INCLUDE
    src/AtmosphereSample.hpp
    src/Terrain/EarthHemisphere.hpp
    src/Terrain/ElevationDataSource.hpp
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
)

set(RENDER_STATES
    assets/render_states/RenderStates.json
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
set(ASSETS ${TERRAIN_TEXTURES} ${TERRAIN_TILES} ${RENDER_STATES})
add_sample_app(Atmosphere
    IDE_FOLDER DiligentSamples/Samples
    SOURCES ${SOURCE}
    INCLUDES ${INCLUDE}
    SHADERS ${ALL_SHADERS}
    ASSETS ${ASSETS}
)

set_source_files_properties(${TERRAIN_SHADERS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "shaders/terrain"
    MACOSX_PACKAGE_LOCATION "Resources/shaders/terrain"
)
set_source_files_properties(${SHADERS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "shaders"
    MACOSX_PACKAGE_LOCATION "Resources/shaders"
)
set_source_files_properties(${RENDER_STATES} PROPERTIES
    VS_DEPLOYMENT_LOCATION "render_states"
    MACOSX_PACKAGE_LOCATION "Resources/render_states"
)
set_source_files_properties(${TERRAIN_TEXTURES} PROPERTIES
    VS_DEPLOYMENT_LOCATION "Terrain"
    MACOSX_PACKAGE_LOCATION "Resources/Terrain"
)
set_source_files_properties(${TERRAIN_TILES} PROPERTIES
    VS_DEPLOYMENT_LOCATION "Terrain/Tiles"
    MACOSX_PACKAGE_LOCATION "Resources/Terrain/Tiles"
)

target_include_directories(Atmosphere
PRIVATE
    src
    src/Terrain
    ../../../DiligentFX/Shaders/Common/public/
    ../../../DiligentFX/Shaders/PostProcess/EpipolarLightScattering/public
    ../../../DiligentFX/Shaders/PostProcess/ToneMapping/public
)

target_link_libraries(Atmosphere
PRIVATE
    Diligent-GraphicsAccessories
    Diligent-TextureLoader
    Diligent-RenderStateNotation
    DiligentFX
)

# We have to use a different group name (Assets with capital A) to override grouping that was set by add_sample_app
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/assets PREFIX Assets FILES ${ASSETS} ${ALL_SHADERS})
