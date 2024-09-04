cmake_minimum_required (VERSION 3.6)

project(Tutorial20_MeshShader CXX)

set(SOURCE
    src/Tutorial20_MeshShader.cpp
    src/ufbx/ufbx.c
)

set(INCLUDE
    src/Tutorial20_MeshShader.hpp
    src/voxelizer.h
    src/ufbx/ufbx.h
)

set(SHADERS
    assets/cube_ash.hlsl
    assets/cube_msh.hlsl
    assets/cube_psh.hlsl
    assets/structures.fxh
)

set(ASSETS
    assets/DGLogo.png
    assets/models/suzanne.fbx
)

add_sample_app("Tutorial20_MeshShader" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
