cmake_minimum_required (VERSION 3.10)

project(Asteroids CXX)

set(SOURCE
    src/asteroids_d3d11.cpp
    src/asteroids_d3d12.cpp
    src/asteroids_DE.cpp
    src/camera.cpp
    src/DDSTextureLoader.cpp
    src/mesh.cpp
    src/simplexnoise1234.c
    src/simulation.cpp
    src/texture.cpp
    src/WinWrapper.cpp
)

set(INCLUDE
    src/asteroids_d3d11.h
    src/asteroids_d3d12.h
    src/asteroids_DE.h
    src/camera.h
    src/dds.h
    src/DDSTextureLoader.h
    src/descriptor.h
    src/mesh.h
    src/noise.h
    src/settings.h
    src/simplexnoise1234.h
    src/simulation.h
    src/subset_d3d12.h
    src/texture.h
    src/upload_heap.h
    src/util.h
)

set(SHADERS
    assets/shaders/asteroid_ps.psh
    assets/shaders/asteroid_ps_d3d11.psh
    assets/shaders/asteroid_ps_diligent.psh
    assets/shaders/font_ps.psh
    assets/shaders/skybox_ps.psh
    assets/shaders/sprite_ps.psh

    assets/shaders/asteroid_vs.vsh
    assets/shaders/asteroid_vs_diligent.vsh
    assets/shaders/skybox_vs.vsh
    assets/shaders/sprite_vs.vsh
)
set_source_files_properties(${SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")

set(COMPILED_SHADERS_DIR ${CMAKE_CURRENT_BINARY_DIR}/CompiledShaders)
file(MAKE_DIRECTORY "${COMPILED_SHADERS_DIR}")

foreach(SRC_SHADER ${SHADERS})
    get_filename_component(SHADER_NAME ${SRC_SHADER} NAME_WE)
    get_filename_component(SHADER_EXT ${SRC_SHADER} EXT)
    set(COMPILED_SHADER ${COMPILED_SHADERS_DIR}/${SHADER_NAME}.h)
    list(APPEND COMPILED_SHADERS ${COMPILED_SHADER})

    set(PROFILE vs_5_0)
    if(${SHADER_NAME} STREQUAL "asteroid_ps")
        set(PROFILE ps_5_1)
    elseif(${SHADER_EXT} STREQUAL ".psh")
        set(PROFILE ps_5_0)
    endif()

    add_custom_command(OUTPUT ${COMPILED_SHADER} # We must use full path here!
                       COMMAND fxc /T ${PROFILE} /E ${SHADER_NAME} /Vn g_${SHADER_NAME} /Fh "${COMPILED_SHADER}" "${SRC_SHADER}"
                       WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
                       COMMENT "Compiling ${SRC_SHADER}"
                       VERBATIM
    )
endforeach(SRC_SHADER)

# NB: we must use the full path, otherwise the build system will not be able to properly detect
#     changes and shader compilation custom command will run every time
set_source_files_properties(${COMPILED_SHADERS} PROPERTIES GENERATED TRUE)


set(GUI
    src/font.h
    src/gui.h
    src/intel_clear_bd_50_usascii.inl
    src/sprite.h
    src/stb_font_consolas_bold_50_usascii.inl
)

set(MEDIA
    assets/media/DiligentD3D11.dds
    assets/media/DiligentD3D12.dds
    assets/media/DiligentGL.dds
    assets/media/directx11.dds
    assets/media/directx12.dds
    assets/media/starbox_1024.dds 
)

if(WIN32)
    add_executable(Asteroids WIN32 
        ${SOURCE} 
        ${INCLUDE} 
        ${SHADERS}
        ${GUI}
        ${MEDIA}
        SDK/Include/d3dx12.h
        assets/shaders/common_defines.h
        assets/shaders/shader_common.h
        readme.md
        # A target created in the same directory (CMakeLists.txt file) that specifies any output of the
        # custom command as a source file is given a rule to generate the file using the command at build time.
        ${COMPILED_SHADERS}
    )
    set_target_properties(Asteroids PROPERTIES 
        LINK_FLAGS "/SUBSYSTEM:CONSOLE"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    )
    copy_required_dlls(Asteroids)

    add_custom_command(TARGET Asteroids POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "\"$<TARGET_FILE_DIR:Asteroids>\"")
else()
    message(FATAL_ERROR "Unsupported platform")
endif()

target_include_directories(Asteroids
PRIVATE
    src
    SDK/Include
    assets/shaders
    ${CMAKE_CURRENT_BINARY_DIR}/CompiledShaders
)

get_supported_backends(ENGINE_LIBRARIES)

target_link_libraries(Asteroids
PRIVATE
    Diligent-BuildSettings
    Diligent-TargetPlatform
    Diligent-TextureLoader
    Diligent-Common
    Diligent-GraphicsTools
    ${ENGINE_LIBRARIES}
    d3d11.lib
    d3d12.lib
    ninput.lib
    winmm.lib
    dxgi.lib
    shcore.lib
    dxguid.lib
)

set_common_target_properties(Asteroids)

if(MSVC)
    target_compile_definitions(Asteroids PRIVATE NOMINMAX)
    # Disable MSVC-specific warnings
    # - w4201: nonstandard extension used: nameless struct/union
    # - w4324: structure was padded due to alignment specifier
    # - w4238: nonstandard extension used: class rvalue used as lvalue
    target_compile_options(Asteroids PRIVATE /wd4201 /wd4324 /wd4238)
endif()

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("shaders" FILES 
    ${SHADERS}
    assets/shaders/common_defines.h
    assets/shaders/shader_common.h
)
source_group("generated" FILES ${COMPILED_SHADERS})
source_group("SDK" FILES SDK/Include/d3dx12.h)
source_group("GUI" FILES ${GUI})
source_group("media" FILES ${MEDIA})

set_target_properties(Asteroids PROPERTIES
    FOLDER DiligentSamples/Samples
)
