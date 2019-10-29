cmake_minimum_required (VERSION 3.6)

project(NuklearDemo CXX)

set(SOURCE
    src/NuklearDemo.cpp
    src/NkDiligent.cpp
)

set(INCLUDE
    src/NuklearDemo.h
    src/NkDiligent.h
)

if(PLATFORM_WIN32)
    list(APPEND SOURCE src/NkInputWin32.cpp src/NkInputWin32.h src/NuklearDemoWin32.cpp)
endif()

set(SHADERS
)

set(ASSETS
)

add_sample_app("NuklearDemo" "Samples" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
