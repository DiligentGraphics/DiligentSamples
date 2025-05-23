cmake_minimum_required (VERSION 3.10)

project(Tutorial00_HelloLinux CXX)

set(SOURCE
    src/Tutorial00_HelloLinux.cpp
)

add_executable(Tutorial00_HelloLinux ${SOURCE})
target_compile_options(Tutorial00_HelloLinux PRIVATE -DENGINE_DLL)

set_target_properties(Tutorial00_HelloLinux PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES)
target_compile_features(Tutorial00_HelloLinux PRIVATE cxx_std_14)

target_include_directories(Tutorial00_HelloLinux PRIVATE "../../../DiligentCore")

set_target_properties(Tutorial00_HelloLinux PROPERTIES
    FOLDER DiligentSamples/Tutorials
    CXX_STANDARD 11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

target_link_libraries(Tutorial00_HelloLinux PRIVATE Diligent-BuildSettings)

if(GL_SUPPORTED)
    find_package(X11 REQUIRED)
    find_package(OpenGL REQUIRED)
    target_link_libraries(Tutorial00_HelloLinux PRIVATE Diligent-GraphicsEngineOpenGL-shared OpenGL::GL X11::X11)
endif()

if(VULKAN_SUPPORTED)
    find_library(XCB_LIBRARY xcb)
    target_link_libraries(Tutorial00_HelloLinux PRIVATE Diligent-GraphicsEngineVk-shared ${XCB_LIBRARY})
endif()
