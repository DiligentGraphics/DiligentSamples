cmake_minimum_required (VERSION 3.6)

project(Tutorial14_ComputeShader CXX)

set(SOURCE
    src/Tutorial14_ComputeShader.cpp
)

set(INCLUDE
    src/Tutorial14_ComputeShader.h
)

set(SHADERS
    assets/particle.psh
    assets/particle.vsh
    assets/structures.fxh
    assets/update_particles.csh
)

set(ASSETS)

add_sample_app("Tutorial14_ComputeShader" "Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
