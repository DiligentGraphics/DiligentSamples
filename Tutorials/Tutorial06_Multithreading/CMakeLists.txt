cmake_minimum_required (VERSION 3.10)

project(Tutorial06_Multithreading CXX)

add_sample_app(Tutorial06_Multithreading
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial06_Multithreading.cpp
        ../Common/src/TexturedCube.cpp
    INCLUDES
        src/Tutorial06_Multithreading.hpp
        ../Common/src/TexturedCube.hpp
    SHADERS
        assets/cube.vsh
        assets/cube.psh
    ASSETS
        assets/DGLogo0.png
        assets/DGLogo1.png
        assets/DGLogo2.png
        assets/DGLogo3.png
)

if(PLATFORM_LINUX)
    target_link_libraries(Tutorial06_Multithreading PRIVATE pthread)
endif()
