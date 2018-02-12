cmake_minimum_required (VERSION 3.6)

project(Tutorial02_Cube CXX)

set(SOURCE
	src/Tutorial02_Cube.cpp
)

set(INCLUDE
	src/Tutorial02_Cube.h
)

set(SHADERS
	assets/cube.vsh
	assets/cube.psh
)

set(ASSETS)

add_tutorial("Tutorial02_Cube" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
