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
    assets/CubeFDM_vs.glsl
    assets/CubeFDM_fs.glsl
    assets/CubeVRS.psh
    assets/CubeVRS.vsh
    assets/ImageBlit.psh
    assets/ImageBlit.vsh
)

set(ASSETS
    assets/DGLogo.png
)

add_sample_app("Tutorial24_VRS" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
