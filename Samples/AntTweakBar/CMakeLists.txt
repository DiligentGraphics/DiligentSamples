cmake_minimum_required (VERSION 3.6)

project(AntTweakBarSample CXX)

set(SOURCE
    src/MengerSponge.cpp
)

set(INCLUDE
    src/MengerSpongeSample.h
)


set(SHADERS
    assets/shaders/MainPS_DX.hlsl
    assets/shaders/MainPS_GL.glsl
    assets/shaders/MainVS_DX.hlsl
    assets/shaders/MainVS_GL.glsl
)

set(ASSETS
    assets/MengerSponge.lua
)

add_sample_app("AntTweakBarSample" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
target_link_libraries(AntTweakBarSample PRIVATE RenderScript)

set_source_files_properties(${ASSETS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "."
    MACOSX_PACKAGE_LOCATION Resources
)

set_source_files_properties(${SHADERS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "shaders"
    MACOSX_PACKAGE_LOCATION Resources/shaders
)

source_group("assets" FILES ${ASSETS})
source_group("assets\\shaders" FILES ${SHADERS})
