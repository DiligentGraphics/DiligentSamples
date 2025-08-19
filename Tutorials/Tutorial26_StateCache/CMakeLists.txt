cmake_minimum_required (VERSION 3.10)

project(Tutorial26_StateCache CXX)

add_sample_app(Tutorial26_StateCache
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial26_StateCache.cpp
    INCLUDES
        src/Tutorial26_StateCache.hpp
    SHADERS
        assets/screen_tri.vsh
        assets/g_buffer.psh
        assets/resolve.psh
        assets/path_trace.psh
        assets/RenderStates.json
        assets/structures.fxh
        assets/scene.fxh
        assets/hash.fxh
        assets/PBR_Common.fxh
)

target_link_libraries(Tutorial26_StateCache
PRIVATE
    Diligent-RenderStateNotation
    Diligent-Archiver-static
)

add_custom_command(TARGET Tutorial26_StateCache PRE_BUILD 
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/../../../DiligentFX/Shaders/Common/public/PBR_Common.fxh" "${CMAKE_CURRENT_SOURCE_DIR}/assets"
)
