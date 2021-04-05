cmake_minimum_required (VERSION 3.6)

project(Animation00_Playback CXX)

set(SOURCE
    src/Animation00_Playback.cpp
    ../Common/src/AnimationUtilities.cpp
    ../Common/src/PlaybackController.cpp
)

set(INCLUDE
    src/Animation00_Playback.hpp
    ../Common/src/AnimationUtilities.hpp
    ../Common/src/PlaybackController.hpp
)

set(SHADERS
    assets/skeleton.vsh
    assets/skeleton.psh
    assets/plane.vsh
    assets/plane.psh    
)

set(ASSETS
    assets/pab_crossarms.ozz
    assets/pab_skeleton.ozz
)

add_sample_app("Animation00_Playback" "DiligentSamples/Samples/Animation" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")

target_link_libraries(Animation00_Playback
PRIVATE
    ozz_base
    ozz_animation
)