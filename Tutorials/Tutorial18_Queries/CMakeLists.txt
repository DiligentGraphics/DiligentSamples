cmake_minimum_required (VERSION 3.10)

project(Tutorial18_Queries CXX)

add_sample_app(Tutorial18_Queries
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial18_Queries.cpp
        ../Common/src/TexturedCube.cpp
    INCLUDES
        src/Tutorial18_Queries.hpp
        ../Common/src/TexturedCube.hpp
    SHADERS
        assets/cube.vsh
        assets/cube.psh
    ASSETS
        assets/DGLogo.png
)
