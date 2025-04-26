cmake_minimum_required (VERSION 3.10)

project(Tutorial11_ResourceUpdates CXX)

add_sample_app(Tutorial11_ResourceUpdates
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial11_ResourceUpdates.cpp
    INCLUDES
        src/Tutorial11_ResourceUpdates.hpp
    SHADERS
        assets/cube.vsh
        assets/cube.psh
    ASSETS
        assets/DGLogo0.png
        assets/DGLogo1.png
        assets/DGLogo2.png
        assets/DGLogo3.png
)
