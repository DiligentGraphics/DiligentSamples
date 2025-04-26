cmake_minimum_required (VERSION 3.10)

project(Tutorial02_Cube CXX)

add_sample_app(Tutorial02_Cube
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial02_Cube.cpp
    INCLUDES
        src/Tutorial02_Cube.hpp
    SHADERS
        assets/cube.vsh
        assets/cube.psh
)
