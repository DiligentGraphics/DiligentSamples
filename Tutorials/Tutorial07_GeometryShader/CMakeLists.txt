cmake_minimum_required (VERSION 3.6)

project(Tutorial07_GeometryShader CXX)

set(SOURCE
    src/Tutorial07_GeometryShader.cpp
)

set(INCLUDE
    src/Tutorial07_GeometryShader.h
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

add_tutorial("Tutorial07_GeometryShader" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
target_link_libraries(Tutorial07_GeometryShader PRIVATE TextureLoader)