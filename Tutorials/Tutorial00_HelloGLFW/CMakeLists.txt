cmake_minimum_required (VERSION 3.6)

project(Tutorial00_HelloGLFW CXX)

set(SOURCE
    src/Tutorial00_HelloGLFW.cpp
)

add_executable(Tutorial00_HelloGLFW WIN32 ${SOURCE})
target_include_directories(Tutorial00_HelloGLFW PRIVATE "../../../DiligentCore")

target_compile_definitions(Tutorial00_HelloGLFW 
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

set_target_properties(Tutorial00_HelloGLFW PROPERTIES
    FOLDER DiligentSamples/Tutorials
)

target_compile_definitions(Tutorial00_HelloGLFW PRIVATE ENGINE_DLL=1)
target_link_libraries(Tutorial00_HelloGLFW PRIVATE Diligent-BuildSettings)

set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
set(GLFW_INSTALL OFF CACHE INTERNAL "" FORCE)
add_subdirectory("../../ThirdParty/glfw" ThirdParty.glfw)
set_target_properties(glfw PROPERTIES FOLDER DiligentSamples/ThirdParty)
target_link_libraries(Tutorial00_HelloGLFW PRIVATE glfw)

if(D3D11_SUPPORTED)
    target_link_libraries(Tutorial00_HelloGLFW PRIVATE Diligent-GraphicsEngineD3D11-shared)
endif()
if(D3D12_SUPPORTED)
    target_link_libraries(Tutorial00_HelloGLFW PRIVATE Diligent-GraphicsEngineD3D12-shared)
endif()
if(GL_SUPPORTED)
    target_link_libraries(Tutorial00_HelloGLFW PRIVATE Diligent-GraphicsEngineOpenGL-shared)
endif()
if(VULKAN_SUPPORTED)
    target_link_libraries(Tutorial00_HelloGLFW PRIVATE Diligent-GraphicsEngineVk-shared)
endif()
if(PLATFORM_WIN32)
    copy_required_dlls(Tutorial00_HelloGLFW)
endif()

if(MSVC)
    foreach(CONFIG ${CMAKE_CONFIGURATION_TYPES})
		string(TOUPPER ${CONFIG} OUT_CONFIG)
        set_target_properties(Tutorial00_HelloGLFW PROPERTIES LINK_FLAGS_${OUT_CONFIG} "/SUBSYSTEM:CONSOLE")
        message(STATUS "LINK_FLAGS_${OUT_CONFIG}")
    endforeach()
endif()
