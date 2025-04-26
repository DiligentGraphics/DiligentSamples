cmake_minimum_required (VERSION 3.10)

project(Tutorial28_HelloOpenXR CXX)

add_sample_app(Tutorial28_HelloOpenXR
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial28_HelloOpenXR.cpp
        ../Common/src/TexturedCube.cpp
    INCLUDES
        ../Common/src/TexturedCube.hpp
    SHADERS
        assets/cube.vsh
        assets/cube.psh
        assets/shader_constants.h
)

target_link_libraries(Tutorial28_HelloOpenXR PRIVATE
    Diligent-Common
    Diligent-GraphicsTools
    Diligent-GraphicsAccessories
    Diligent-TextureLoader
    openxr_loader
)
