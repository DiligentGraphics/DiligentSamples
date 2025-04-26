cmake_minimum_required (VERSION 3.10)

project(Tutorial17_MSAA CXX)

add_sample_app(Tutorial17_MSAA
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial17_MSAA.cpp
        ../Common/src/TexturedCube.cpp
    INCLUDES
        src/Tutorial17_MSAA.hpp
        ../Common/src/TexturedCube.hpp
    SHADERS
        assets/cube.vsh
        assets/cube.psh
    ASSETS
        assets/DGLogo.png
)
