cmake_minimum_required (VERSION 3.10)

project(Tutorial23_CommandQueues CXX)

add_sample_app(Tutorial23_CommandQueues
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial23_CommandQueues.cpp
        src/Buildings.cpp
        src/Terrain.cpp
        src/Profiler.cpp
    INCLUDES
        src/Tutorial23_CommandQueues.hpp
        src/Buildings.hpp
        src/Terrain.hpp
        src/Profiler.hpp
    SHADERS
        assets/Structures.fxh
        assets/GenerateTerrain.csh
        assets/DrawBuilding.vsh
        assets/DrawBuilding.psh
        assets/DrawTerrain.vsh
        assets/DrawTerrain.psh
        assets/PostProcess.vsh
        assets/PostProcess.psh
        assets/DownSample.psh
    ASSETS
        assets/Sand.jpg
)

if(PLATFORM_LINUX)
    target_link_libraries(Tutorial23_CommandQueues PRIVATE pthread)
endif()
