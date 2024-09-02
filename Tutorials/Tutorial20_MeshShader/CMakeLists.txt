cmake_minimum_required (VERSION 3.6)

project(Tutorial20_MeshShader CXX)

set(SOURCE
    src/Tutorial20_MeshShader.cpp
)

set(INCLUDE
    src/Tutorial20_MeshShader.hpp
    src/voxelizer.h
    src/AssetLoader/DXSDKMeshLoader.hpp
    src/AssetLoader/GLTFBuilder.hpp
    src/AssetLoader/GLTFLoader.hpp
    src/AssetLoader/GLTFResourceManager.hpp
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

add_sample_app("Tutorial20_MeshShader" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")

target_link_libraries("Tutorial20_MeshShader" PRIVATE Diligent-AssetLoader)