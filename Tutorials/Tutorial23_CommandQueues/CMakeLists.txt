cmake_minimum_required (VERSION 3.6)

project(Tutorial23_CommandQueues CXX)

set(SOURCE
    src/Tutorial23_CommandQueues.cpp
    src/Buildings.cpp
    src/Terrain.cpp
    src/Profiler.cpp
)

set(INCLUDE
    src/Tutorial23_CommandQueues.hpp
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

add_sample_app("Tutorial23_CommandQueues" "DiligentSamples/Tutorials" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")

if(PLATFORM_LINUX)
    target_link_libraries(Tutorial23_CommandQueues PRIVATE pthread)
endif()
