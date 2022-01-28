cmake_minimum_required (VERSION 3.8)

project(GLFWDemo CXX)

set(SOURCES
    src/GLFWDemo.cpp
    src/GLFWDemo.hpp
    src/Game.cpp
    src/Game.hpp
    readme.md
)
if(PLATFORM_MACOS)
    list(APPEND SOURCES src/SurfaceHelper.mm)
endif()

set(SHADERS
    assets/DrawMap.hlsl
    assets/GenerateSDF.hlsl
    assets/Structures.fxh
)

set(RENDER_STATES
    assets/RenderStates.json
)

set(ASSETS ${SHADERS} ${RENDER_STATES})

set_source_files_properties(${RENDER_STATES} PROPERTIES VS_TOOL_OVERRIDE "None")
set_source_files_properties(${SHADERS}       PROPERTIES VS_TOOL_OVERRIDE "None")

if(PLATFORM_MACOS)
    add_executable(GLFWDemo MACOSX_BUNDLE ${SOURCES} ${ASSETS})
else()
    add_executable(GLFWDemo ${SOURCES} ${ASSETS})
endif()

set_target_properties(GLFWDemo PROPERTIES CXX_STANDARD 14 CXX_STANDARD_REQUIRED YES)
target_compile_features(GLFWDemo PUBLIC cxx_std_14)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES} ${ASSETS})
target_include_directories(GLFWDemo PRIVATE "../../../DiligentCore")
if(METAL_SUPPORTED)
    target_include_directories(GLFWDemo PRIVATE "../../../DiligentCorePro")
endif()

target_compile_definitions(GLFWDemo PRIVATE UNICODE)

set_target_properties(GLFWDemo PROPERTIES
    FOLDER DiligentSamples/Samples
)

target_compile_definitions(GLFWDemo PRIVATE ENGINE_DLL=1)
target_link_libraries(GLFWDemo
PRIVATE
    Diligent-Common
    Diligent-GraphicsTools
    Diligent-RenderStateNotation
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
if(METAL_SUPPORTED)
    target_link_libraries(GLFWDemo PRIVATE Diligent-GraphicsEngineMetal-shared)
endif()
if(PLATFORM_WIN32)
    copy_required_dlls(GLFWDemo)
endif()

if(MSVC)
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

if(PLATFORM_MACOS AND VULKAN_LIB_PATH)
    # Configure rpath so that executables can find vulkan library
    set_target_properties(GLFWDemo PROPERTIES
        BUILD_RPATH "${VULKAN_LIB_PATH}"
    )
endif()

set_source_files_properties(${ASSETS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "."
    MACOSX_PACKAGE_LOCATION "Resources"
)
