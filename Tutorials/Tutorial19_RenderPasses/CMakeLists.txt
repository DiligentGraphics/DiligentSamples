cmake_minimum_required (VERSION 3.10)

project(Tutorial19_RenderPasses CXX)

add_sample_app(Tutorial19_RenderPasses
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial19_RenderPasses.cpp
        ../Common/src/TexturedCube.cpp
    INCLUDES
        src/Tutorial19_RenderPasses.hpp
        ../Common/src/TexturedCube.hpp
    SHADERS
        assets/cube.vsh
        assets/cube.psh
        assets/light_volume.vsh
        assets/light_volume_glsl.psh
        assets/light_volume_hlsl.psh
        assets/ambient_light.vsh
        assets/ambient_light_glsl.psh
        assets/ambient_light_hlsl.psh
        assets/shader_structs.fxh
    ASSETS
        assets/DGLogo.png
)
