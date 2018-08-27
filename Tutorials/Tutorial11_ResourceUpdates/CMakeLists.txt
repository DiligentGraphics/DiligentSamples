cmake_minimum_required (VERSION 3.6)

project(Tutorial11_ResourceUpdates CXX)

set(SOURCE
    src/Tutorial11_ResourceUpdates.cpp
)

set(INCLUDE
    src/Tutorial11_ResourceUpdates.h
)

set(SHADERS
    assets/cube.vsh
    assets/cube.psh
)

set(ASSETS
    assets/DGLogo.png
)

add_tutorial("Tutorial11_ResourceUpdates" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
target_link_libraries(Tutorial11_ResourceUpdates PRIVATE TextureLoader)