cmake_minimum_required (VERSION 3.10)

project(Tutorial05_TextureArray CXX)

add_sample_app(Tutorial05_TextureArray
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial05_TextureArray.cpp
        ../Common/src/TexturedCube.cpp
    INCLUDES
        src/Tutorial05_TextureArray.hpp
        ../Common/src/TexturedCube.hpp
    SHADERS
        assets/cube_inst.vsh
        assets/cube_inst.psh
        assets/draw_args.csh
    ASSETS
        assets/DGLogo0.png
        assets/DGLogo1.png
        assets/DGLogo2.png
        assets/DGLogo3.png
)
