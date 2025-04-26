cmake_minimum_required (VERSION 3.10)

project(Tutorial07_GeometryShader CXX)

add_sample_app(Tutorial07_GeometryShader
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial07_GeometryShader.cpp
        ../Common/src/TexturedCube.cpp
    INCLUDES
        src/Tutorial07_GeometryShader.hpp
        ../Common/src/TexturedCube.hpp
    SHADERS
        assets/cube.vsh
        assets/cube.psh
        assets/cube.gsh
        assets/structures.fxh
    ASSETS
        assets/DGLogo.png
)
