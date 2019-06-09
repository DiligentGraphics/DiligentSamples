cmake_minimum_required (VERSION 3.6)

project(Tutorial00_HelloLinux CXX)

set(SOURCE
    src/Tutorial00_HelloLinux.cpp
)

add_executable(Tutorial00_HelloLinux WIN32 ${SOURCE})
target_compile_options(Tutorial00_HelloLinux PRIVATE -DENGINE_DLL)

target_include_directories(Tutorial00_HelloLinux PRIVATE "../../../DiligentCore")

set_target_properties(Tutorial00_HelloLinux PROPERTIES
    FOLDER Tutorials
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(Tutorial00_HelloLinux PRIVATE GL X11 Diligent-GraphicsEngineOpenGL-shared)

if(VULKAN_SUPPORTED)
    target_link_libraries(Tutorial00_HelloLinux PRIVATE Diligent-GraphicsEngineVk-shared xcb)
endif()
