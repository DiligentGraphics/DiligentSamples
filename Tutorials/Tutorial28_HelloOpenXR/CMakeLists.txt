cmake_minimum_required (VERSION 3.10)

project(Tutorial28_HelloOpenXR CXX)

set(SOURCE
    src/Tutorial28_HelloOpenXR.cpp
    ../Common/src/TexturedCube.cpp
)

set(INCLUDE
    ../Common/src/TexturedCube.hpp
)

set(SHADERS
    assets/cube.vsh
    assets/cube.psh
)

set(ASSETS)

add_sample_app("Tutorial28_HelloOpenXR" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")

target_link_libraries(Tutorial28_HelloOpenXR PRIVATE
    Diligent-Common
    Diligent-GraphicsTools
    Diligent-GraphicsAccessories
    Diligent-TextureLoader
    openxr_loader
)
