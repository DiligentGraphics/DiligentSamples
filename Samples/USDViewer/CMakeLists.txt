cmake_minimum_required (VERSION 3.13)

project(USDViewer CXX)


include(FetchContent)

# Fetch ImGuizmo
message("Fetching ImGuizmo repository...")
FetchContent_Declare(
    ImGuizmo
    GIT_REPOSITORY https://github.com/CedricGuillemet/ImGuizmo
    GIT_TAG        ba662b1
)
FetchContent_MakeAvailable(ImGuizmo)


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

file(COPY ../GLTFViewer/assets/textures/papermill.ktx DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures)

set(ASSETS
    assets/textures/papermill.ktx
    assets/cube.usd
)

add_sample_app("USDViewer" "DiligentSamples/Samples" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")

if(${CMAKE_GENERATOR} MATCHES "Visual Studio")
    set_target_properties(USDViewer PROPERTIES
        VS_DEBUGGER_ENVIRONMENT "PATH=%PATH%;${DILIGENT_USD_PATH}/$<CONFIG>/lib;${DILIGENT_USD_PATH}/$<CONFIG>/bin"
    )
endif()

set(IMGUIZMO_SOURCE
    ${imguizmo_SOURCE_DIR}/ImGuizmo.h
    ${imguizmo_SOURCE_DIR}/ImGuizmo.cpp
)
target_sources(USDViewer PRIVATE ${IMGUIZMO_SOURCE})
target_include_directories(USDViewer PRIVATE ${imguizmo_SOURCE_DIR})
source_group("ImGuizmo" FILES ${IMGUIZMO_SOURCE})


target_link_libraries(USDViewer
PRIVATE
    USD-Libraries
    Diligent-Hydrogent
    DiligentFX
    usd_usdGeom
)
set_target_properties(USDViewer PROPERTIES CXX_STANDARD 17)
