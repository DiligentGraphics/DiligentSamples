cmake_minimum_required (VERSION 3.6)

function(add_sample SAMPLE_NAME SOURCE INCLUDE SHADERS ASSETS)

    set_source_files_properties(${SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")
	set(ALL_ASSETS ${ASSETS} ${SHADERS})
    add_target_platform_app(${SAMPLE_NAME} "${SOURCE}" "${INCLUDE}" "${ALL_ASSETS}")

    if(PLATFORM_WIN32)
        set_target_properties(${SAMPLE_NAME} PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        )
        copy_required_dlls(${SAMPLE_NAME})
    elseif(PLATFORM_UNIVERSAL_WINDOWS)
		append_sample_base_uwp_source(${SAMPLE_NAME})
    endif()

    target_include_directories(${SAMPLE_NAME}
    PRIVATE
        src
    )

    target_link_libraries(${SAMPLE_NAME}
    PRIVATE
        NativeAppBase
        BuildSettings
        SampleBase
        RenderScript
    )
    set_common_target_properties(${SAMPLE_NAME})

    if(MSVC)
        # Disable MSVC-specific warnings
        # - w4201: nonstandard extension used: nameless struct/unio
        target_compile_options(${SAMPLE_NAME} PRIVATE /wd4201)
    endif()

    set_target_properties(${SAMPLE_NAME} PROPERTIES
        FOLDER Samples
    )

    source_group("src" FILES ${SOURCE})
    source_group("include" FILES ${INCLUDE})
endfunction()

add_subdirectory(SampleBase)
add_subdirectory(AntTweakBar)
add_subdirectory(Atmosphere)
