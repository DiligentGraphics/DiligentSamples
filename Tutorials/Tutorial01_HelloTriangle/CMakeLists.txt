cmake_minimum_required (VERSION 3.10)

project(Tutorial01_HelloTriangle CXX)

add_sample_app(Tutorial01_HelloTriangle
    IDE_FOLDER 
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial01_HelloTriangle.cpp
    INCLUDES
        src/Tutorial01_HelloTriangle.hpp
)
