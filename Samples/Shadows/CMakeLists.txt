cmake_minimum_required (VERSION 3.6)

project(Shadows CXX)

set(SOURCE
    src/ShadowsSample.cpp
)

set(INCLUDE
    src/ShadowsSample.hpp
)

set(SHADERS
    assets/shaders/MeshVS.vsh
    assets/shaders/MeshPS.psh
)
set(ALL_SHADERS ${SHADERS})

set(RENDER_STATES 
    assets/render_states/RenderStates.json
)

file(GLOB POWERPLANT_FILES LIST_DIRECTORIES false assets/Powerplant/*.*)

set(ASSETS
    ${POWERPLANT_FILES}
    ${RENDER_STATES}
)

add_sample_app("Shadows" "DiligentSamples/Samples" "${SOURCE}" "${INCLUDE}" "${ALL_SHADERS}" "${ASSETS}")
target_link_libraries(Shadows 
PRIVATE 
    Diligent-AssetLoader
    Diligent-RenderStateNotation
    DiligentFX
)

target_include_directories(Shadows
PRIVATE
    ../../../DiligentFX/Shaders/Common/public/
)

set_source_files_properties(${POWERPLANT_FILES} PROPERTIES
    VS_DEPLOYMENT_LOCATION "Powerplant"
    MACOSX_PACKAGE_LOCATION "Resources/Powerplant"
)

set_source_files_properties(${ALL_SHADERS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "shaders"
    MACOSX_PACKAGE_LOCATION Resources/shaders
)

set_source_files_properties(${RENDER_STATES} PROPERTIES
    VS_DEPLOYMENT_LOCATION "render_states"
    MACOSX_PACKAGE_LOCATION Resources/render_states
)

# We have to use a different group name (Assets with capital A) to override grouping that was set by add_sample_app
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/assets PREFIX Assets FILES ${ASSETS} ${SHADERS})
