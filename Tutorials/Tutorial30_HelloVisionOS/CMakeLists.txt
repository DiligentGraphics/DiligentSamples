cmake_minimum_required (VERSION 3.28)

project(Tutorial30_HelloVisionOS CXX Swift)

set(TUTORIAL_SOURCE
    src/Tutorial30_Immersive.cpp
    src/Tutorial30_Preview.cpp
    src/Tutorial30_RenderEngine.cpp
    src/Tutorial30_Scene.cpp
)

set(TUTORIAL_INCLUDE
    src/Tutorial30_Immersive.hpp
    src/Tutorial30_Preview.hpp
    src/Tutorial30_RenderEngine.hpp
    src/Tutorial30_Scene.hpp
)

set(VISIONOS_SWIFT_SOURCE
    src/visionOS/VisionOSApp.swift
    src/visionOS/ImmersiveModel.swift
    src/visionOS/ImmersiveView.swift
    src/visionOS/LauncherView.swift
    src/visionOS/PreviewView.swift
)

set(VISIONOS_BRIDGE_SOURCE
    src/visionOS/VisionOSAppBridge.mm
)

set(VISIONOS_BRIDGE_INCLUDE
    src/visionOS/VisionOSAppBridge.h
)

set(SHADERS
    assets/cube.vsh
    assets/cube.psh
    assets/shader_constants.fxh
)

set(INFO_PLIST
    ${CMAKE_CURRENT_SOURCE_DIR}/src/visionOS/Info.plist
)

set_source_files_properties(${SHADERS} PROPERTIES
    VS_TOOL_OVERRIDE "None"
    MACOSX_PACKAGE_LOCATION "Resources"
)

find_library(QUARTZCORE_FRAMEWORK QuartzCore)
find_library(UIKIT_FRAMEWORK      UIKit)
if(NOT QUARTZCORE_FRAMEWORK OR NOT UIKIT_FRAMEWORK)
    message(FATAL_ERROR "QuartzCore/UIKit frameworks not found")
endif()

add_library(Tutorial30_HelloVisionOS_Native STATIC
    ${TUTORIAL_SOURCE}
    ${TUTORIAL_INCLUDE}
    ${VISIONOS_BRIDGE_SOURCE}
    ${VISIONOS_BRIDGE_INCLUDE}
)

target_include_directories(Tutorial30_HelloVisionOS_Native
    PUBLIC  src/visionOS   # Swift bridging header path
    PRIVATE src
)

target_link_libraries(Tutorial30_HelloVisionOS_Native
    PRIVATE
        Diligent-BuildSettings
        Diligent-Common
        Diligent-GraphicsTools
        Diligent-GraphicsAccessories
        Diligent-GraphicsEngineMetal-static
        ${QUARTZCORE_FRAMEWORK}
        ${UIKIT_FRAMEWORK}
)
set_common_target_properties(Tutorial30_HelloVisionOS_Native)
set_target_properties(Tutorial30_HelloVisionOS_Native PROPERTIES
    FOLDER "DiligentSamples/Tutorials/Tutorial30_HelloVisionOS"
)

add_executable(Tutorial30_HelloVisionOS MACOSX_BUNDLE
    ${VISIONOS_SWIFT_SOURCE}
    ${SHADERS}
)

set_target_properties(Tutorial30_HelloVisionOS PROPERTIES
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.diligentengine.samples.Tutorial30-HelloVisionOS"
    MACOSX_BUNDLE_INFO_PLIST "${INFO_PLIST}"
    BUILD_RPATH "@executable_path"
    FOLDER "DiligentSamples/Tutorials"
    XCODE_ATTRIBUTE_SWIFT_VERSION "5.0"
    XCODE_ATTRIBUTE_SWIFT_OBJC_BRIDGING_HEADER
        "${CMAKE_CURRENT_SOURCE_DIR}/src/visionOS/VisionOSAppBridge.h"
    XCODE_ATTRIBUTE_CLANG_ENABLE_MODULES "YES"
)

target_link_libraries(Tutorial30_HelloVisionOS PRIVATE
    Tutorial30_HelloVisionOS_Native
)

source_group("src"      FILES ${TUTORIAL_SOURCE} ${TUTORIAL_INCLUDE})
source_group("visionOS" FILES ${VISIONOS_SWIFT_SOURCE} ${VISIONOS_BRIDGE_SOURCE} ${VISIONOS_BRIDGE_INCLUDE})
source_group("assets"   FILES ${SHADERS})

target_sources(Tutorial30_HelloVisionOS PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/readme.md")
set_source_files_properties(
    "${CMAKE_CURRENT_SOURCE_DIR}/readme.md" PROPERTIES HEADER_FILE_ONLY TRUE
)
