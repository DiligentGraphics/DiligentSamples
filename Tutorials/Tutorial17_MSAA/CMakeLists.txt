cmake_minimum_required (VERSION 3.6)

project(Tutorial17_MSAA CXX)

set(SOURCE
    src/Tutorial17_MSAA.cpp
    ../Common/src/TexturedCube.cpp
)

set(INCLUDE
    src/Tutorial17_MSAA.h
    ../Common/src/TexturedCube.h
)

set(SHADERS
    assets/cube.vsh
    assets/cube.psh
    assets/DGLogo.png
    assets/rendertarget.vsh
    assets/rendertarget.psh
)

add_sample_app("Tutorial17_MSAA" "Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
