cmake_minimum_required (VERSION 3.6)

project(Tutorial04_Instancing CXX)

set(SOURCE
    src/Tutorial04_Instancing.cpp
    ../Common/src/TexturedCube.cpp
    src/ufbx/ufbx.c
)

set(INCLUDE
    src/Tutorial04_Instancing.hpp
    ../Common/src/TexturedCube.hpp
    src/voxelizer.h
    src/ufbx/ufbx.h
)

set(SHADERS
    assets/cube_inst.vsh
    assets/cube_inst.psh
)

set(ASSETS
    assets/DGLogo.png
    assets/models/suzanne.fbx
)

add_sample_app("Tutorial04_Instancing" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
target_link_libraries(Tutorial04_Instancing PRIVATE Diligent-TextureLoader)