cmake_minimum_required (VERSION 3.6)

project(Tutorial07_GeometryShader CXX)

set(SOURCE
    src/Tutorial07_GeometryShader.cpp
    ../Common/src/TexturedCube.cpp
)

set(INCLUDE
    src/Tutorial07_GeometryShader.hpp
    ../Common/src/TexturedCube.hpp
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

add_sample_app("Tutorial07_GeometryShader" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
target_link_libraries(Tutorial07_GeometryShader PRIVATE Diligent-TextureLoader)

if(TARGET Diligent-HLSL2GLSLConverterLib)
    get_target_property(HLSL2GLSLConverterLib_SourceDir Diligent-HLSL2GLSLConverterLib SOURCE_DIR)
    target_link_libraries(Tutorial07_GeometryShader PRIVATE Diligent-HLSL2GLSLConverterLib)
    target_include_directories(Tutorial07_GeometryShader PRIVATE "${HLSL2GLSLConverterLib_SourceDir}/include")
    target_compile_definitions(Tutorial07_GeometryShader PRIVATE HLSL2GLSL_CONVERTER_SUPPORTED)
endif()
