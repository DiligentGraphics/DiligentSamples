cmake_minimum_required (VERSION 3.6)

project(Tutorial20_MeshShader CXX)

set(SOURCE
    src/Tutorial20_MeshShader.cpp
)

set(INCLUDE
    src/Tutorial20_MeshShader.hpp
    src/voxelizer.h
)

set(SHADERS
    assets/cube_ash.hlsl
    assets/cube_msh.hlsl
    assets/cube_psh.hlsl
    assets/structures.fxh
)

set(ASSETS
    assets/DGLogo.png
    assets/models/DamagedHelmet/DamagedHelmet.gltf
)

include(FetchContent)
FetchContent_Declare(
  TinyGLTF
  GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
  GIT_TAG v2.9.3
  GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(TinyGLTF)


add_sample_app("Tutorial20_MeshShader" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")

# target_link_libraries("Tutorial20_MeshShader" PRIVATE Diligent-AssetLoader)

target_link_libraries("Tutorial20_MeshShader" PRIVATE TinyGLTF)