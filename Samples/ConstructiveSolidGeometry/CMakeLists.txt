cmake_minimum_required (VERSION 3.10)

project(ConstructiveSolidGeometry CXX)

file(COPY ../GLTFViewer/assets/textures/papermill.ktx DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/assets/Textures)

set(SOURCE
    src/ConstructiveSolidGeometry.cpp
    ../../Tutorials/Common/src/TexturedCube.cpp
)

set(INCLUDE
    src/ConstructiveSolidGeometry.hpp
    src/PipelineStateManager.hpp
    src/STLLoader.hpp
    ../../Tutorials/Common/src/TexturedCube.hpp
)

set(SHADERS
    assets/Shaders/CSGHelpers.hlsli
    assets/Shaders/ConstantBufferDeclarations.hlsli
    assets/Shaders/RayIntersection.hlsli
    assets/Shaders/GenerateCSGGeometry.hlsl
    assets/Shaders/ClearABuffer.hlsl
    assets/Shaders/ResolveCSG.hlsl
    assets/Shaders/ApplyToneMap.hlsl
    assets/Shaders/GenerateCSGMesh.hlsl
    assets/Shaders/GammaCorrection.hlsl
)

set(RENDER_STATES
    assets/RenderStates/RenderStatesLib.drsn
    assets/RenderStates/ToneMapping.drsn
    assets/RenderStates/GammaCorrection.drsn
    assets/RenderStates/ClearABuffer.drsn
    assets/RenderStates/GenerateCSGGeometry.drsn
    assets/RenderStates/GenerateCSGMesh.drsn
    assets/RenderStates/ResolveCSG.drsn
)

set(MODELS
    assets/Models/vh_f_knee_l_NIH3D.stl
)

set(TEXTURES
    assets/Textures/papermill.ktx
)

set(ASSETS
    ${TEXTURES}
    ${MODELS}
    ${RENDER_STATES}
)

add_sample_app(ConstructiveSolidGeometry
    IDE_FOLDER DiligentSamples/Samples
    SOURCES ${SOURCE}
    INCLUDES ${INCLUDE}
    SHADERS ${SHADERS}
    ASSETS ${ASSETS}
)

set_common_target_properties(ConstructiveSolidGeometry 20)

target_link_libraries(ConstructiveSolidGeometry
PRIVATE
    Diligent-RenderStateNotation
    DiligentFX
)

target_include_directories(ConstructiveSolidGeometry
PRIVATE
    ../../../DiligentFX/Shaders/Common/public/
)

set_source_files_properties(${SHADERS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "Shaders"
    MACOSX_PACKAGE_LOCATION Resources/Shaders
)

set_source_files_properties(${RENDER_STATES} PROPERTIES
    VS_DEPLOYMENT_LOCATION "RenderStates"
    MACOSX_PACKAGE_LOCATION Resources/RenderStates
)

set_source_files_properties(${TEXTURES} PROPERTIES
    VS_DEPLOYMENT_LOCATION "Textures"
    MACOSX_PACKAGE_LOCATION "Resources/Textures"
)

set_source_files_properties(${MODELS} PROPERTIES
    VS_DEPLOYMENT_LOCATION "Models"
    MACOSX_PACKAGE_LOCATION "Resources/Models"
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/assets PREFIX Assets FILES ${ASSETS} ${SHADERS})
