cmake_minimum_required (VERSION 3.6)

project(Tutorial27_PostProcessing CXX)

file(COPY ../../Samples/GLTFViewer/assets/textures/papermill.ktx DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures)

set(SOURCE
    src/Tutorial27_PostProcessing.cpp
    ../Common/src/TexturedCube.cpp
)

set(INCLUDE
    src/Tutorial27_PostProcessing.hpp
    ../Common/src/TexturedCube.hpp
)

set(SHADERS
    assets/shaders/GeometryStructures.fxh
    assets/shaders/GenerateGeometry.vsh
    assets/shaders/GenerateGeometry.psh
    assets/shaders/ComputeLighting.fx
    assets/shaders/ApplyToneMap.fx 
)

set(TEXTURES
    assets/textures/papermill.ktx
)

set(ASSETS
    ${TEXTURES}
)

set(ALL_SHADERS ${SHADERS})

add_sample_app("Tutorial27_PostProcessing" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${ALL_SHADERS}" "${ASSETS}")

target_link_libraries(Tutorial27_PostProcessing PRIVATE DiligentFX)

if (PLATFORM_WIN32 AND GL_SUPPORTED)
    target_link_libraries(Tutorial27_PostProcessing PRIVATE Diligent-GLAdapterSelector)
endif()

set_source_files_properties(${ALL_SHADERS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "shaders"
    MACOSX_PACKAGE_LOCATION Resources/shaders
)

set_source_files_properties(${TEXTURES} PROPERTIES
    VS_DEPLOYMENT_LOCATION "textures"
    MACOSX_PACKAGE_LOCATION "Resources/textures"
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/assets PREFIX Assets FILES ${ASSETS} ${SHADERS})
