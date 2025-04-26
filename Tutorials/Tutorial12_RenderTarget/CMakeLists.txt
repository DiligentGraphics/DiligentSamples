cmake_minimum_required (VERSION 3.10)

project(Tutorial12_RenderTarget CXX)

add_sample_app(Tutorial12_RenderTarget
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial12_RenderTarget.cpp
        ../Common/src/TexturedCube.cpp
    INCLUDES
        src/Tutorial12_RenderTarget.hpp
        ../Common/src/TexturedCube.hpp
    SHADERS
        assets/cube.vsh
        assets/cube.psh
        assets/rendertarget.vsh
        assets/rendertarget.psh
    ASSETS
        assets/DGLogo.png
)
