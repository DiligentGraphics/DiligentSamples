cmake_minimum_required (VERSION 3.6)

project(GLTFViewer CXX)

set(SOURCE
    src/GLTFViewer.cpp
)

set(INCLUDE
    src/GLTFViewer.h
)

set(SHADERS
    assets/shaders/env_map.vsh
    assets/shaders/env_map.psh
    ../../../DiligentFX/Shaders/PostProcess/ToneMapping/public/ToneMappingStructures.fxh
    ../../../DiligentFX/Shaders/PostProcess/ToneMapping/public/ToneMapping.fxh
    ../../../DiligentFX/Shaders/Common/public/BasicStructures.fxh
)

file(GLOB_RECURSE MODELS LIST_DIRECTORIES false assets/models/*.*)

set(ASSETS
    ${MODELS}
    assets/textures/papermill.ktx
)

add_sample_app("GLTFViewer" "Samples" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
target_link_libraries(GLTFViewer PRIVATE AssetLoader DiligentFX)

target_include_directories(GLTFViewer PRIVATE
    ../../../DiligentFX/Shaders/PostProcess/ToneMapping/public/
)

add_custom_command(TARGET GLTFViewer PRE_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../../../DiligentFX/Shaders/PostProcess/ToneMapping/public/ToneMappingStructures.fxh" "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../../../DiligentFX/Shaders/PostProcess/ToneMapping/public/ToneMapping.fxh" "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../../../DiligentFX/Shaders/Common/public/BasicStructures.fxh" "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders"
)

set_source_files_properties(${ASSETS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "."
    MACOSX_PACKAGE_LOCATION Resources
)

set_source_files_properties(${SHADERS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "shaders"
    MACOSX_PACKAGE_LOCATION Resources/shaders
)

source_group("assets" FILES ${ASSETS})
source_group("assets\\shaders" FILES ${SHADERS})
