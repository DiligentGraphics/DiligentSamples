cmake_minimum_required (VERSION 3.10)

project(Tutorial13_ShadowMap CXX)

add_sample_app(Tutorial13_ShadowMap
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial13_ShadowMap.cpp
        ../Common/src/TexturedCube.cpp
    INCLUDES
        src/Tutorial13_ShadowMap.hpp
        ../Common/src/TexturedCube.hpp
    SHADERS
        assets/cube.vsh
        assets/cube.psh
        assets/cube_shadow.vsh
        assets/plane.vsh
        assets/plane.psh
        assets/shadow_map_vis.vsh
        assets/shadow_map_vis.psh
        assets/structures.fxh
    ASSETS
        assets/DGLogo.png
)
