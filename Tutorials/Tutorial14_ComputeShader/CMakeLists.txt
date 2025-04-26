cmake_minimum_required (VERSION 3.10)

project(Tutorial14_ComputeShader CXX)

add_sample_app(Tutorial14_ComputeShader
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial14_ComputeShader.cpp
    INCLUDES
        src/Tutorial14_ComputeShader.hpp
    SHADERS
        assets/particle.psh
        assets/particle.vsh
        assets/structures.fxh
        assets/reset_particle_lists.csh
        assets/collide_particles.csh
        assets/move_particles.csh
        assets/particles.fxh
)
