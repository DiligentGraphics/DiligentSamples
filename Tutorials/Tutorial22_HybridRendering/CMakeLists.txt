cmake_minimum_required (VERSION 3.10)

project(Tutorial22_HybridRendering CXX)

add_sample_app(Tutorial22_HybridRendering
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial22_HybridRendering.cpp
    INCLUDES
        src/Tutorial22_HybridRendering.hpp
    SHADERS
        assets/Structures.fxh
        assets/Utils.fxh
        assets/RayQueryMtl.fxh
        assets/Rasterization.vsh
        assets/Rasterization.psh
        assets/PostProcess.vsh
        assets/PostProcess.psh
        assets/RayTracing.csh
    ASSETS
        assets/DGLogo0.png
        assets/DGLogo1.png
        assets/DGLogo2.png
        assets/DGLogo3.png
        assets/Marble.jpg
    DXC_REQUIRED
        YES
)
