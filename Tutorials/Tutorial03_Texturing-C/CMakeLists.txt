cmake_minimum_required (VERSION 3.10)

project(Tutorial03_Texturing-C CXX)

add_sample_app(Tutorial03_Texturing-C
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial03_Texturing-C.cpp
        src/Tutorial03_Texturing.c
    INCLUDES
        src/Tutorial03_Texturing-C.hpp
    SHADERS
        assets/cube.vsh
        assets/cube.psh
    ASSETS
        assets/DGLogo.png
)
