cmake_minimum_required (VERSION 3.6)

project(ShadowsSample CXX)

set(SOURCE
    src/ShadowsSample.cpp
)

set(INCLUDE
    src/ShadowsSample.h
)


set(SHADERS
    assets/shaders/MeshVS.vsh
    assets/shaders/MeshPS.psh
    ../../../DiligentFX/Shaders/Common/public/BasicStructures.fxh
    ../../../DiligentFX/Shaders/Common/public/Shadows.fxh
)

file(GLOB POWERPLANT_FILES LIST_DIRECTORIES false assets/Powerplant/*.*)

set(ASSETS
    ${POWERPLANT_FILES}
)

add_sample_app("ShadowsSample" "DiligentSamples/Samples" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
target_link_libraries(ShadowsSample PRIVATE Diligent-AssetLoader DiligentFX)

target_include_directories(ShadowsSample
PRIVATE
    ../../../DiligentFX/Shaders/Common/public/
)

add_custom_command(TARGET ShadowsSample PRE_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../../../DiligentFX/Shaders/Common/public/BasicStructures.fxh" "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../../../DiligentFX/Shaders/Common/public/Shadows.fxh" "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders"
)

set_source_files_properties(${POWERPLANT_FILES} PROPERTIES
    VS_DEPLOYMENT_LOCATION "Powerplant"
    MACOSX_PACKAGE_LOCATION "Resources/Powerplant"
)

set_source_files_properties(${SHADERS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "shaders"
    MACOSX_PACKAGE_LOCATION Resources/shaders
)

source_group("assets" FILES ${ASSETS})
source_group("assets\\shaders" FILES ${SHADERS})
