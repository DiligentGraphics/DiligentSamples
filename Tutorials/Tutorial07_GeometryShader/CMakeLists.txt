cmake_minimum_required (VERSION 3.10)

project(Tutorial07_GeometryShader CXX)

set(SOURCE
    src/Tutorial07_GeometryShader.cpp
    ../Common/src/TexturedCube.cpp
)

set(INCLUDE
    src/Tutorial07_GeometryShader.hpp
    ../Common/src/TexturedCube.hpp
)

set(SHADERS
    assets/cube.vsh
    assets/cube.psh
    assets/cube.gsh
    assets/structures.fxh
)

set(ASSETS
    assets/DGLogo.png
)

add_sample_app("Tutorial07_GeometryShader" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
