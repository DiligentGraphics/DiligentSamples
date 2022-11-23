cmake_minimum_required (VERSION 3.6)

project(Tutorial26_StateCache CXX)

set(SOURCE
    src/Tutorial26_StateCache.cpp
)

set(INCLUDE
    src/Tutorial26_StateCache.hpp
)

set(SHADERS
    assets/screen_tri.vsh
    assets/g_buffer.psh
    assets/resolve.psh
    assets/path_trace.csh
    assets/RenderStates.json
    assets/structures.fxh
    assets/scene.fxh
    assets/hash.fxh
    assets/PBR_Common.fxh
)

set(ASSETS ${PSO_ARCHIVE})

add_sample_app("Tutorial26_StateCache" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")

target_link_libraries(Tutorial26_StateCache PRIVATE Diligent-RenderStateNotation)

add_custom_command(TARGET Tutorial26_StateCache PRE_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../../../DiligentFX/Shaders/Common/public/PBR_Common.fxh" "${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
