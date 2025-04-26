cmake_minimum_required (VERSION 3.10)

project(Tutorial03_Texturing CXX)

add_sample_app(Tutorial03_Texturing
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial03_Texturing.cpp
    INCLUDES
        src/Tutorial03_Texturing.hpp
    SHADERS
        assets/cube.vsh
        assets/cube.psh
    ASSETS
        assets/DGLogo.png
)
