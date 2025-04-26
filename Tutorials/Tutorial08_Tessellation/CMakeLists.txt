cmake_minimum_required (VERSION 3.10)

project(Tutorial08_Tessellation CXX)

add_sample_app(Tutorial08_Tessellation
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial08_Tessellation.cpp
    INCLUDES
        src/Tutorial08_Tessellation.hpp
    SHADERS
        assets/terrain.vsh
        assets/terrain.psh
        assets/terrain_wire.psh
        assets/terrain.gsh
        assets/terrain.hsh
        assets/terrain.dsh
        assets/structures.fxh
    ASSETS
        assets/ps_height_1k.png
        assets/ps_texture_2k.png
)
