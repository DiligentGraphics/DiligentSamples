cmake_minimum_required (VERSION 3.6)

project(Tutorial00_HelloWin32 CXX)

set(SOURCE
    src/Tutorial00_HelloWin32.cpp
)

add_executable(Tutorial00_HelloWin32 WIN32 ${SOURCE})
target_compile_options(Tutorial00_HelloWin32 PRIVATE -DUNICODE -DENGINE_DLL)

target_include_directories(Tutorial00_HelloWin32 PRIVATE "../../../DiligentCore")

set_target_properties(Tutorial00_HelloWin32 PROPERTIES
    FOLDER Tutorials
)

add_dependencies(Tutorial00_HelloWin32 GraphicsEngineD3D11-shared GraphicsEngineOpenGL-shared)
if(D3D12_SUPPORTED)
    add_dependencies(Tutorial00_HelloWin32 GraphicsEngineD3D12-shared)
endif()
if(VULKAN_SUPPORTED)
    add_dependencies(Tutorial00_HelloWin32 GraphicsEngineVk-shared)
endif()
copy_required_dlls(Tutorial00_HelloWin32)