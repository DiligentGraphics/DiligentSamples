cmake_minimum_required (VERSION 3.13)

project(USDViewer CXX)

set(SOURCE
    src/USDViewer.cpp
)

set(INCLUDE
    src/USDViewer.hpp
)

set(SHADERS
    assets/shaders/vs.vsh
    assets/shaders/ps.psh
)

set(ASSETS
    assets/cube.usd
)

add_sample_app("USDViewer" "DiligentSamples/Samples" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")

if(${CMAKE_GENERATOR} MATCHES "Visual Studio")
    set_target_properties(USDViewer PROPERTIES
        VS_DEBUGGER_ENVIRONMENT "PATH=%PATH%;${DILIGENT_USD_PATH}/$<CONFIG>/lib;${DILIGENT_USD_PATH}/$<CONFIG>/bin"
    )
endif()

target_link_libraries(USDViewer
PRIVATE
    Diligent-Hydrogent
    DiligentFX
)
