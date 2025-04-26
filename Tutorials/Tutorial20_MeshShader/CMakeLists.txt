cmake_minimum_required (VERSION 3.10)

project(Tutorial20_MeshShader CXX)

add_sample_app(Tutorial20_MeshShader
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial20_MeshShader.cpp
    INCLUDES
        src/Tutorial20_MeshShader.hpp
    SHADERS
        assets/cube.ash
        assets/cube.msh
        assets/cube.psh
        assets/structures.fxh
    ASSETS
        assets/DGLogo.png
)
