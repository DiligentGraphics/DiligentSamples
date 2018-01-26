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

add_sample("AntTweakBarSample" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")

source_group("assets" FILES ${ASSETS})
source_group("assets\\shaders" FILES ${SHADERS})
