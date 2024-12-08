cmake_minimum_required (VERSION 3.10)

project(Tutorial28_HelloOpenXR CXX)

set(SOURCE
    src/Tutorial28_HelloOpenXR.cpp
)

add_executable(Tutorial28_HelloOpenXR WIN32 ${SOURCE})

target_compile_definitions(Tutorial28_HelloOpenXR PRIVATE UNICODE)

set_target_properties(Tutorial28_HelloOpenXR PROPERTIES
    FOLDER DiligentSamples/Tutorials
)

target_compile_definitions(Tutorial28_HelloOpenXR PRIVATE ENGINE_DLL=1)
target_link_libraries(Tutorial28_HelloOpenXR PRIVATE
    Diligent-BuildSettings
    Diligent-Common
    openxr_loader
)

if(D3D11_SUPPORTED)
    target_link_libraries(Tutorial28_HelloOpenXR PRIVATE Diligent-GraphicsEngineD3D11-shared)
endif()
if(D3D12_SUPPORTED)
    target_link_libraries(Tutorial28_HelloOpenXR PRIVATE Diligent-GraphicsEngineD3D12-shared)
endif()
if(GL_SUPPORTED)
    target_link_libraries(Tutorial28_HelloOpenXR PRIVATE Diligent-GraphicsEngineOpenGL-shared)
endif()
if(VULKAN_SUPPORTED)
    target_link_libraries(Tutorial28_HelloOpenXR PRIVATE Diligent-GraphicsEngineVk-shared)
endif()
copy_required_dlls(Tutorial28_HelloOpenXR)
