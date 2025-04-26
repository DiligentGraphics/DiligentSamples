cmake_minimum_required (VERSION 3.10)

project(Tutorial09_Quads CXX)

add_sample_app(Tutorial09_Quads
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial09_Quads.cpp
    INCLUDES
        src/Tutorial09_Quads.hpp
    SHADERS
        assets/quad.vsh
        assets/quad.psh
        assets/quad_batch.vsh
        assets/quad_batch.psh
    ASSETS
        assets/DGLogo0.png
        assets/DGLogo1.png
        assets/DGLogo2.png
        assets/DGLogo3.png
)

if(PLATFORM_LINUX)
    target_link_libraries(Tutorial09_Quads PRIVATE pthread)
endif()
