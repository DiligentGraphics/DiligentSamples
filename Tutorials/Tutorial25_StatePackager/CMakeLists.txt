cmake_minimum_required (VERSION 3.6)

project(Tutorial25_StatePackager CXX)

set(SOURCE
    src/Tutorial25_StatePackager.cpp
)

set(INCLUDE
    src/Tutorial25_StatePackager.hpp
)

set(PSO_ARCHIVE ${CMAKE_CURRENT_SOURCE_DIR}/assets/StateArchive.bin)
# NB: we must use the full path, otherwise the build system will not be able to properly detect
#     changes and PSO packaging custom command will run every time
set_source_files_properties(${PSO_ARCHIVE} PROPERTIES GENERATED TRUE)

set(SHADERS
    assets/screen_tri.vsh
    assets/g_buffer.psh
    assets/resolve.psh
    assets/path_trace.csh
    assets/RenderStates.json
    assets/structures.fxh
    assets/scene.fxh
    assets/hash.fxh
)

set(ASSETS ${PSO_ARCHIVE})

add_sample_app("Tutorial25_StatePackager" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")

set(DEVICE_FLAGS --dx11 --dx12)
# Compute shaders are not supported in OpenGL on MacOS
if (NOT ${DILIGENT_NO_HLSL} AND NOT PLATFORM_MACOS)
    list(APPEND DEVICE_FLAGS --opengl)
endif()
if((NOT ${DILIGENT_NO_GLSLANG}) AND (NOT ${DILIGENT_NO_HLSL}))
    list(APPEND DEVICE_FLAGS --vulkan --metal_macos --metal_ios)
endif()

# Set up custom commands to archive PSOs
add_custom_command(OUTPUT ${PSO_ARCHIVE} # We must use full path here!
                   COMMAND $<TARGET_FILE:Diligent-RenderStatePackager> -i RenderStates.json -r assets -s assets -o "${PSO_ARCHIVE}" ${DEVICE_FLAGS} --print_contents
                   WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                   MAIN_DEPENDENCY "${CMAKE_CURRENT_SOURCE_DIR}/assets/RenderStates.json"
                   DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/screen_tri.vsh"
                           "${CMAKE_CURRENT_SOURCE_DIR}/assets/g_buffer.psh"
                           "${CMAKE_CURRENT_SOURCE_DIR}/assets/resolve.psh"
                           "${CMAKE_CURRENT_SOURCE_DIR}/assets/path_trace.csh"
                           "${CMAKE_CURRENT_SOURCE_DIR}/assets/structures.fxh"
                           "${CMAKE_CURRENT_SOURCE_DIR}/assets/scene.fxh"
                           "${CMAKE_CURRENT_SOURCE_DIR}/assets/hash.fxh"
                           "$<TARGET_FILE:Diligent-RenderStatePackager>"
                   COMMENT "Creating render state archive..."
                   VERBATIM
)

source_group("generated" FILES ${PSO_ARCHIVE})
