cmake_minimum_required (VERSION 3.6)

project(Tutorial10_RingBuffer CXX)

set(SOURCE
    src/Tutorial10_RingBuffer.cpp
)

set(INCLUDE
    src/Tutorial10_RingBuffer.h
)

set(SHADERS
    assets/ripples.vsh
    assets/ripples.psh
)

set(ASSETS)

add_tutorial("Tutorial10_RingBuffer" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
target_link_libraries(Tutorial10_RingBuffer PRIVATE TextureLoader)