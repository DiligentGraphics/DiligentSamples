cmake_minimum_required (VERSION 3.6)

project(Tutorial24_VRS CXX)

set(SOURCE
    src/Tutorial24_VRS.cpp
    ../Common/src/TexturedCube.cpp
)

set(INCLUDE
    src/Tutorial24_VRS.hpp
    ../Common/src/TexturedCube.hpp
)

set(SHADERS
    assets/Structures.fxh
    assets/CubeVRS.psh
    assets/CubeVRS.vsh
    assets/ImageBlit.psh
    assets/ImageBlit.vsh
    # mobile vulkan
    assets/CubeFDM_vs.glsl
    assets/CubeFDM_fs.glsl
)

set(ASSETS
    assets/DGLogo.png
)

if(PLATFORM_MACOS OR PLATFORM_IOS)
    list(APPEND SOURCE
        src/Tutorial24_VRS.mm)
    list(APPEND SHADERS
        assets/ImageBlit.msl)
endif()

add_sample_app("Tutorial24_VRS" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")

if(NOT METAL_SUPPORTED)
    target_include_directories(Tutorial24_VRS PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../../DiligentCore/Graphics/GraphicsEngineMetal/interface")
endif()
