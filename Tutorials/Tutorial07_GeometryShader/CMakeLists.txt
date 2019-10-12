cmake_minimum_required (VERSION 3.6)

project(Tutorial07_GeometryShader CXX)

set(SOURCE
    src/Tutorial07_GeometryShader.cpp
)

set(INCLUDE
    src/Tutorial07_GeometryShader.h
)

set(SHADERS
    assets/cube.vsh
    assets/cube.psh
    assets/cube.gsh
    assets/structures.fxh
)

set(ASSETS
    assets/DGLogo.png
)

add_sample_app("Tutorial07_GeometryShader" "Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")

get_target_property(HLSL2GLSLConverterLib_SourceDir Diligent-HLSL2GLSLConverterLib SOURCE_DIR)
target_link_libraries(Tutorial07_GeometryShader PRIVATE Diligent-TextureLoader Diligent-HLSL2GLSLConverterLib)
target_include_directories(Tutorial07_GeometryShader PRIVATE "${HLSL2GLSLConverterLib_SourceDir}/include")