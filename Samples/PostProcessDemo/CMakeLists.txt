cmake_minimum_required (VERSION 3.6)

project(PostProcessDemo CXX)

set(SOURCE
    src/PostProcessDemo.cpp
)

set(INCLUDE
    src/PostProcessDemo.hpp
)

set(SHADERS
    assets/shaders/GenerateRoughnessNormal.fx
    assets/shaders/ComputeMotionVectors.fx
    assets/shaders/ApplyPostEffects.fx
)

set(EXTERNAL_SHADERS
    ../../../DiligentFX/Shaders/Common/public/BasicStructures.fxh
    ../../../DiligentFX/Shaders/Common/private/FullScreenTriangleVS.fx
    ../../../DiligentFX/Shaders/Common/private/FullScreenTriangleVSOutput.fxh
    ../../../DiligentFX/Shaders/PostProcess/ToneMapping/public/ToneMappingStructures.fxh
    ../../../DiligentFX/Shaders/PostProcess/ToneMapping/public/ToneMapping.fxh
)

set(ALL_SHADERS ${SHADERS} ${EXTERNAL_SHADERS})

add_sample_app("PostProcessDemo" "DiligentSamples/Samples" "${SOURCE}" "${INCLUDE}" "${ALL_SHADERS}" "${ASSETS}")

target_link_libraries(PostProcessDemo 
PRIVATE 
    Diligent-AssetLoader
    Diligent-RenderStateNotation
    DiligentFX
)

target_include_directories(PostProcessDemo
PRIVATE
    ../../../DiligentFX/Shaders/Common/public/
)

foreach(FILE ${EXTERNAL_SHADERS})
    # Copy external shaders
    add_custom_command(TARGET PostProcessDemo PRE_BUILD 
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/${FILE}" "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders"
    )
endforeach(FILE)

set_source_files_properties(${ALL_SHADERS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "shaders"
    MACOSX_PACKAGE_LOCATION Resources/shaders
)

# We have to use a different group name (Assets with capital A) to override grouping that was set by add_sample_app
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/assets PREFIX Assets FILES ${ASSETS} ${SHADERS})
source_group("Assets\\shaders" FILES ${EXTERNAL_SHADERS})
