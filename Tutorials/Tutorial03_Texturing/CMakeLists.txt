cmake_minimum_required (VERSION 3.6)

project(Tutorial03_Texturing CXX)

set(SOURCE
	src/Tutorial03_Texturing.cpp
)

set(INCLUDE
	src/Tutorial03_Texturing.h
)

set(SHADERS
	assets/cube.vsh
	assets/cube.psh
)

set(ASSETS
	assets/DGLogo.png
)

add_tutorial("Tutorial03_Texturing" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
target_link_libraries(Tutorial03_Texturing PRIVATE TextureLoader)