cmake_minimum_required (VERSION 3.6)

project(Tutorial23_AsyncQueues CXX)

set(SOURCE
    src/Tutorial23_AsyncQueues.cpp
    src/Buildings.cpp
    src/Terrain.cpp
    src/Profiler.cpp
)

set(INCLUDE
    src/Tutorial23_AsyncQueues.hpp
    src/Buildings.hpp
    src/Terrain.hpp
    src/Profiler.hpp
)

set(SHADERS
    assets/Structures.fxh
    assets/GenerateTerrain.csh
    assets/DrawBuilding.vsh
    assets/DrawBuilding.psh
    assets/DrawTerrain.vsh
    assets/DrawTerrain.psh
    assets/PostProcess.vsh
    assets/PostProcess.psh
    assets/DownSample.psh
)

set(ASSETS
    assets/Sand.jpg
)

add_sample_app("Tutorial23_AsyncQueues" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
