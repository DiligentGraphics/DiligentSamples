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

# Fetch assets
message("Fetching USD assets...")
FetchContent_Declare(
    USDAssets
    GIT_REPOSITORY https://github.com/DiligentGraphics/USDAssets
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/assets/usd
)
FetchContent_MakeAvailable(USDAssets)

set(SOURCE
    src/USDViewer.cpp
)

set(INCLUDE
    src/USDViewer.hpp
)

set(SHADERS
)

file(COPY ../GLTFViewer/assets/textures/papermill.ktx DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/assets/textures)
file(GLOB USD_ASSETS LIST_DIRECTORIES false assets/usd/*.usd*)

set(ASSETS
    assets/textures/papermill.ktx
    assets/cube.usd
    ${USD_ASSETS}
)

add_sample_app("USDViewer" "DiligentSamples/Samples" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")

set_source_files_properties(${USD_ASSETS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "usd"
    MACOSX_PACKAGE_LOCATION Resources/usd
)
set_source_files_properties(assets/textures/papermill.ktx PROPERTIES
    VS_DEPLOYMENT_LOCATION "textures"
    MACOSX_PACKAGE_LOCATION "Resources/textures"
)

if(${CMAKE_GENERATOR} MATCHES "Visual Studio")
    if (DILIGENT_USD_FILEFORMAT_PLUGINS_PATH)
        set_target_properties(USDViewer PROPERTIES
            VS_DEBUGGER_ENVIRONMENT "PATH=%PATH%;${DILIGENT_USD_PATH}/$<CONFIG>/lib;${DILIGENT_USD_PATH}/$<CONFIG>/bin;${DILIGENT_USD_FILEFORMAT_PLUGINS_PATH}/$<CONFIG>/bin;${DILIGENT_USD_FILEFORMAT_PLUGINS_PATH}/$<CONFIG>/plugin/usd\nPXR_PLUGINPATH_NAME=%PXR_PLUGINPATH_NAME%;${DILIGENT_USD_FILEFORMAT_PLUGINS_PATH}/$<CONFIG>/plugin/usd"
        )
    else()
        set_target_properties(USDViewer PROPERTIES
            VS_DEBUGGER_ENVIRONMENT "PATH=%PATH%;${DILIGENT_USD_PATH}/$<CONFIG>/lib;${DILIGENT_USD_PATH}/$<CONFIG>/bin"
        )
    endif()
endif()

set(IMGUIZMO_SOURCE
    ${imguizmo_SOURCE_DIR}/ImGuizmo.h
    ${imguizmo_SOURCE_DIR}/ImGuizmo.cpp
)
target_sources(USDViewer PRIVATE ${IMGUIZMO_SOURCE})
target_include_directories(USDViewer PRIVATE ${imguizmo_SOURCE_DIR})
source_group("ImGuizmo" FILES ${IMGUIZMO_SOURCE})

get_target_property(DILIGENT_FX_SOURCE_DIR DiligentFX SOURCE_DIR)
get_target_property(HYDROGENT_SOURCE_DIR Diligent-Hydrogent SOURCE_DIR)

target_link_libraries(USDViewer
PRIVATE
    Diligent-Hydrogent
    DiligentFX
)
set_target_properties(USDViewer PROPERTIES CXX_STANDARD 17)
target_compile_definitions(USDViewer PRIVATE DILIGENT_FX_SHADERS_DIR="${DILIGENT_FX_SOURCE_DIR}/Shaders" HYDROGENT_SHADERS_DIR="${HYDROGENT_SHADER_DIR}/shaders")

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/assets PREFIX Assets FILES ${ASSETS} ${SHADERS})
