cmake_minimum_required (VERSION 3.10)

project(Tutorial29_OIT CXX)

set(SOURCE
    src/Tutorial29_OIT.cpp
)

set(INCLUDE
    src/Tutorial29_OIT.hpp
)

set(SHADERS
    assets/common.fxh
    assets/geometry.vsh
    assets/alpha_blend.psh
)

set(ASSETS
)

add_sample_app("Tutorial29_OIT" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
