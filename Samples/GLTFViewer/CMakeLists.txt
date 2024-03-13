cmake_minimum_required (VERSION 3.6)

project(GLTFViewer CXX)

set(SOURCE
    src/GLTFViewer.cpp
)

set(INCLUDE
    src/GLTFViewer.hpp
)

set(SHADERS
    assets/shaders/ApplyPostEffects.psh
)

set(ALL_SHADERS ${SHADERS})

include(FetchContent)

# Fetch assets
message("Fetching GLTF assets...")
FetchContent_Declare(
    GLTFAssets
    GIT_REPOSITORY https://github.com/DiligentGraphics/GLTFAssets
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/models
)
FetchContent_MakeAvailable(GLTFAssets)

file(GLOB_RECURSE MODELS LIST_DIRECTORIES false assets/models/*.*)

set(TEXTURES
    assets/textures/papermill.ktx
    assets/textures/sheen_albedo_scaling.jpg
    assets/textures/charlie_preintegrated.jpg
)

set(ASSETS
    ${MODELS}
    ${TEXTURES}
)

add_sample_app("GLTFViewer" "DiligentSamples/Samples" "${SOURCE}" "${INCLUDE}" "${ALL_SHADERS}" "${ASSETS}")
target_link_libraries(GLTFViewer 
PRIVATE
    Diligent-AssetLoader 
    DiligentFX
)

target_include_directories(GLTFViewer PRIVATE
    ../../../DiligentFX/Shaders/PostProcess/ToneMapping/public/
)

set_source_files_properties(${ALL_SHADERS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "shaders"
    MACOSX_PACKAGE_LOCATION Resources/shaders
)

set_source_files_properties(${TEXTURES} PROPERTIES
    VS_DEPLOYMENT_LOCATION "textures"
    MACOSX_PACKAGE_LOCATION "Resources/textures"
)

foreach(FILE ${MODELS})
    file(RELATIVE_PATH REL_PATH "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${FILE}")
    get_filename_component(REL_DIR "${REL_PATH}" DIRECTORY)
    set_source_files_properties(${FILE} PROPERTIES
        VS_DEPLOYMENT_LOCATION "${REL_DIR}"
        MACOSX_PACKAGE_LOCATION "Resources/${REL_DIR}"
    )
endforeach(FILE)

# We have to use a different group name (Assets with capital A) to override grouping that was set by add_sample_app
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/assets PREFIX Assets FILES ${ASSETS} ${SHADERS})
