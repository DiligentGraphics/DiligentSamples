cmake_minimum_required (VERSION 3.6)

project(GLFWDemo CXX)

set(SOURCES
    src/GLFWSample.cpp
    src/GLFWSample.hpp
    src/Game.cpp
    src/Game.hpp
)
set(SHADERS
    assets/DrawMap.hlsl
    assets/GenerateSDF.hlsl
    assets/Structures.h
)
set_source_files_properties(${SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")

add_executable(GLFWDemo WIN32 ${SOURCES} ${SHADERS})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${SHADERS})
target_include_directories(GLFWDemo PRIVATE "../../../DiligentCore")

target_compile_definitions(GLFWDemo 
PRIVATE 
    UNICODE
    D3D11_SUPPORTED=$<BOOL:${D3D11_SUPPORTED}>
    D3D12_SUPPORTED=$<BOOL:${D3D12_SUPPORTED}>
    GL_SUPPORTED=$<BOOL:${GL_SUPPORTED}>
    VULKAN_SUPPORTED=$<BOOL:${VULKAN_SUPPORTED}>
    PLATFORM_WIN32=$<BOOL:${PLATFORM_WIN32}>
    PLATFORM_LINUX=$<BOOL:${PLATFORM_LINUX}>
    PLATFORM_MACOS=$<BOOL:${PLATFORM_MACOS}>
)

set_target_properties(GLFWDemo PROPERTIES
    FOLDER DiligentSamples/Samples
)

target_compile_definitions(GLFWDemo PRIVATE ENGINE_DLL=1)
target_link_libraries(GLFWDemo
PRIVATE
    Diligent-Common
    Diligent-GraphicsTools
    glfw
)

if(D3D11_SUPPORTED)
    target_link_libraries(GLFWDemo PRIVATE Diligent-GraphicsEngineD3D11-shared)
endif()
if(D3D12_SUPPORTED)
    target_link_libraries(GLFWDemo PRIVATE Diligent-GraphicsEngineD3D12-shared)
endif()
if(GL_SUPPORTED)
    target_link_libraries(GLFWDemo PRIVATE Diligent-GraphicsEngineOpenGL-shared)
endif()
if(VULKAN_SUPPORTED)
    target_link_libraries(GLFWDemo PRIVATE Diligent-GraphicsEngineVk-shared)
endif()
if(PLATFORM_WIN32)
    copy_required_dlls(GLFWDemo)
endif()

if(MSVC)
    foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
        string(TOUPPER ${CONFIG} OUT_CONFIG)
        set_target_properties(GLFWDemo PROPERTIES LINK_FLAGS_${OUT_CONFIG} "/SUBSYSTEM:CONSOLE")
    endforeach()
    set_target_properties(GLFWDemo PROPERTIES 
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    )
endif()

if(PLATFORM_WIN32 OR PLATFORM_LINUX)
    # Copy assets to target folder
    add_custom_command(TARGET GLFWDemo POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "\"$<TARGET_FILE_DIR:GLFWDemo>\"")
endif()
