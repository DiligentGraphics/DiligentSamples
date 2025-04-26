cmake_minimum_required (VERSION 3.10)

project(Tutorial16_BindlessResources CXX)

add_sample_app(Tutorial16_BindlessResources
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial16_BindlessResources.cpp
    INCLUDES
        src/Tutorial16_BindlessResources.hpp
    SHADERS
        assets/cube.vsh
        assets/cube.psh
    ASSETS
        assets/DGLogo0.png
        assets/DGLogo1.png
        assets/DGLogo2.png
        assets/DGLogo3.png
)
