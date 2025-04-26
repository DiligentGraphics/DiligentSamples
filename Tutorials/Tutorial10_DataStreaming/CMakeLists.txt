cmake_minimum_required (VERSION 3.10)

project(Tutorial10_DataStreaming CXX)

add_sample_app(Tutorial10_DataStreaming
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial10_DataStreaming.cpp
    INCLUDES
        src/Tutorial10_DataStreaming.hpp
    SHADERS
        assets/polygon.vsh
        assets/polygon.psh
        assets/polygon_batch.vsh
        assets/polygon_batch.psh
    ASSETS
        assets/DGLogo0.png
        assets/DGLogo1.png
        assets/DGLogo2.png
        assets/DGLogo3.png
)

if(PLATFORM_LINUX)
    target_link_libraries(Tutorial10_DataStreaming PRIVATE pthread)
endif()
