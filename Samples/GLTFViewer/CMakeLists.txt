cmake_minimum_required (VERSION 3.6)

project(GLTFViewer CXX)

set(SOURCE
    src/GLTFViewer.cpp
)

set(INCLUDE
    src/GLTFViewer.h
)

set(ASSETS
    assets/models/DamagedHelmet.gltf
)

add_sample_app("GLTFViewer" "Samples" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
target_link_libraries(GLTFViewer PRIVATE AssetLoader DiligentFX)

set_source_files_properties(${ASSETS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "."
    MACOSX_PACKAGE_LOCATION Resources
)

set_source_files_properties(${SHADERS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "shaders"
    MACOSX_PACKAGE_LOCATION Resources/shaders
)

source_group("assets" FILES ${ASSETS})
source_group("assets\\shaders" FILES ${SHADERS})
