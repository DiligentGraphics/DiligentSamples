cmake_minimum_required (VERSION 3.10)

project(Tutorial21_RayTracing CXX)

add_sample_app(Tutorial21_RayTracing
    IDE_FOLDER
        DiligentSamples/Tutorials
    SOURCES
        src/Tutorial21_RayTracing.cpp
    INCLUDES
        src/Tutorial21_RayTracing.hpp
    SHADERS
        assets/structures.fxh
        assets/RayUtils.fxh
        assets/CubePrimaryHit.rchit
        assets/GlassPrimaryHit.rchit
        assets/SpherePrimaryHit.rchit
        assets/SphereIntersection.rint
        assets/Ground.rchit
        assets/RayTrace.rgen
        assets/PrimaryMiss.rmiss
        assets/ShadowMiss.rmiss
        assets/ImageBlit.psh
        assets/ImageBlit.vsh
    ASSETS
        assets/DGLogo0.png
        assets/DGLogo1.png
        assets/DGLogo2.png
        assets/DGLogo3.png
        assets/Ground.jpg
)
