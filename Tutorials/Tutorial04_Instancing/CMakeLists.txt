cmake_minimum_required (VERSION 3.10)

project(Tutorial04_Instancing CXX)

add_sample_app(Tutorial04_Instancing
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial04_Instancing.cpp
        ../Common/src/TexturedCube.cpp
    INCLUDES
        src/Tutorial04_Instancing.hpp
        ../Common/src/TexturedCube.hpp
    SHADERS
        assets/cube_inst.vsh
        assets/cube_inst.psh
    ASSETS
        assets/DGLogo.png
)
