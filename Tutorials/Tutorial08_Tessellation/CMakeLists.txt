cmake_minimum_required (VERSION 3.6)

project(Tutorial08_Tessellation CXX)

set(SOURCE
	src/Tutorial08_Tessellation.cpp
)

set(INCLUDE
	src/Tutorial08_Tessellation.h
)

set(SHADERS
	assets/terrain.vsh
	assets/terrain.psh
	assets/terrain_wire.psh
	assets/terrain.gsh
	assets/terrain.hsh
	assets/terrain.dsh
	assets/structures.h
)

set(ASSETS
	${SHADERS}
	assets/ps_height_1k.png
	assets/ps_texture_2k.png
)

add_tutorial("Tutorial08_Tessellation" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
target_link_libraries(Tutorial08_Tessellation PRIVATE TextureLoader)