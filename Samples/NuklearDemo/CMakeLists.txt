cmake_minimum_required (VERSION 3.6)

project(NuklearDemo CXX)

set(SOURCE
    src/NuklearDemo.cpp
    src/NkDiligent.cpp
)

set(INCLUDE
    src/NuklearDemo.hpp
    src/NkDiligent.h
)

if(PLATFORM_WIN32)
    list(APPEND SOURCE src/NkInputWin32.cpp src/NkInputWin32.h src/NuklearDemoWin32.cpp)
endif()

set(SHADERS
)

set(ASSETS
)

add_sample_app("NuklearDemo" "DiligentSamples/Samples" "${SOURCE}" "${INCLUDE}" "${SHADERS}" "${ASSETS}")
target_include_directories(NuklearDemo PRIVATE ${DILIGENT_NUKLEAR_DIR})

if(MSVC)
    target_compile_definitions(NuklearDemo PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_source_files_properties(src/NkDiligent.cpp PROPERTY COMPILE_FLAGS
        /wd4127 # Conditional expression is constant
    )
endif()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "13.0")
        set_property(SOURCE
            src/NkDiligent.cpp
        APPEND_STRING PROPERTY
            COMPILE_FLAGS " -Wno-unused-but-set-variable"
        )
    endif()
endif()
