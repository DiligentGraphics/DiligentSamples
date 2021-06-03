cmake_minimum_required (VERSION 3.6)

project(Tutorial00_HelloWin32 CXX)

set(SOURCE
    src/Tutorial00_HelloWin32.cpp
)

add_executable(Tutorial00_HelloWin32 WIN32 ${SOURCE})
target_include_directories(Tutorial00_HelloWin32 PRIVATE "../../../DiligentCore")

target_compile_definitions(Tutorial00_HelloWin32 PRIVATE UNICODE)

set_target_properties(Tutorial00_HelloWin32 PROPERTIES
    FOLDER DiligentSamples/Tutorials
)

target_compile_definitions(Tutorial00_HelloWin32 PRIVATE ENGINE_DLL=1)
target_link_libraries(Tutorial00_HelloWin32 PRIVATE Diligent-BuildSettings)

if(D3D11_SUPPORTED)
    target_link_libraries(Tutorial00_HelloWin32 PRIVATE Diligent-GraphicsEngineD3D11-shared)
endif()
if(D3D12_SUPPORTED)
    target_link_libraries(Tutorial00_HelloWin32 PRIVATE Diligent-GraphicsEngineD3D12-shared)
endif()
if(GL_SUPPORTED)
    target_link_libraries(Tutorial00_HelloWin32 PRIVATE Diligent-GraphicsEngineOpenGL-shared)
endif()
if(VULKAN_SUPPORTED)
    target_link_libraries(Tutorial00_HelloWin32 PRIVATE Diligent-GraphicsEngineVk-shared)
endif()
copy_required_dlls(Tutorial00_HelloWin32)
