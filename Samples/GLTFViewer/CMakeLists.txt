cmake_minimum_required (VERSION 3.6)

project(GLTFViewer CXX)

set(SOURCE
    src/GLTFViewer.cpp
)

set(INCLUDE
    src/GLTFViewer.hpp
)

set(SHADERS
    assets/shaders/BoundBox.vsh
    assets/shaders/BoundBox.psh
)

set(EXTERNAL_SHADERS
    ../../../DiligentFX/Shaders/PostProcess/ToneMapping/public/ToneMappingStructures.fxh
    ../../../DiligentFX/Shaders/PostProcess/ToneMapping/public/ToneMapping.fxh
    ../../../DiligentFX/Shaders/Common/public/BasicStructures.fxh
    ../../../DiligentFX/Shaders/Common/public/ShaderUtilities.fxh
)

set(ALL_SHADERS ${SHADERS} ${EXTERNAL_SHADERS})

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

set(RENDER_STATES assets/render_states/RenderStates.json)

set(ASSETS
    ${MODELS}
    ${TEXTURES}
    ${RENDER_STATES}
)

add_sample_app("GLTFViewer" "DiligentSamples/Samples" "${SOURCE}" "${INCLUDE}" "${ALL_SHADERS}" "${ASSETS}")
target_link_libraries(GLTFViewer 
PRIVATE
    Diligent-AssetLoader 
    Diligent-RenderStateNotation 
    DiligentFX
)

target_include_directories(GLTFViewer PRIVATE
    ../../../DiligentFX/Shaders/PostProcess/ToneMapping/public/
)

foreach(FILE ${EXTERNAL_SHADERS})
    # Copy external shaders
    add_custom_command(TARGET GLTFViewer PRE_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}" "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders"
    )
endforeach(FILE)

set_source_files_properties(${ALL_SHADERS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "shaders"
    MACOSX_PACKAGE_LOCATION Resources/shaders
)

set_source_files_properties(${RENDER_STATES} PROPERTIES
    VS_DEPLOYMENT_LOCATION "render_states"
    MACOSX_PACKAGE_LOCATION "Resources/render_states"
)

set_source_files_properties(${TEXTURES} PROPERTIES
    VS_DEPLOYMENT_LOCATION "textures"
    MACOSX_PACKAGE_LOCATION "Resources/textures"
)

set_source_files_properties(${BOOM_BOX_MODEL} PROPERTIES
    VS_DEPLOYMENT_LOCATION "models/BoomBoxWithAxes"
    MACOSX_PACKAGE_LOCATION "Resources/models/BoomBoxWithAxes"
)

set_source_files_properties(${CESIUM_MAN_MODEL} PROPERTIES
    VS_DEPLOYMENT_LOCATION "models/CesiumMan"
    MACOSX_PACKAGE_LOCATION "Resources/models/CesiumMan"
)

set_source_files_properties(${DAMAGED_HELMENT_MODEL} PROPERTIES
    VS_DEPLOYMENT_LOCATION "models/DamagedHelmet"
    MACOSX_PACKAGE_LOCATION "Resources/models/DamagedHelmet"
)

set_source_files_properties(${FLIGHT_HELMENT_MODEL} PROPERTIES
    VS_DEPLOYMENT_LOCATION "models/FlightHelmet"
    MACOSX_PACKAGE_LOCATION "Resources/models/FlightHelmet"
)

set_source_files_properties(${METAL_ROUGH_SPHERES_MODEL} PROPERTIES
    VS_DEPLOYMENT_LOCATION "models/MetalRoughSpheres"
    MACOSX_PACKAGE_LOCATION "Resources/models/MetalRoughSpheres"
)

set_source_files_properties(${NORMAL_TANGENT_TEST_MODEL} PROPERTIES
    VS_DEPLOYMENT_LOCATION "models/NormalTangentTest"
    MACOSX_PACKAGE_LOCATION "Resources/models/NormalTangentTest"
)

# We have to use a different group name (Assets with capital A) to override grouping that was set by add_sample_app
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/assets PREFIX Assets FILES ${ASSETS} ${SHADERS})
source_group("Assets\\shaders" FILES ${EXTERNAL_SHADERS})
