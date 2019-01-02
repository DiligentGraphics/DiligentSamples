if(PLATFORM_WIN32 OR PLATFORM_LINUX)
    cmake_minimum_required (VERSION 3.13)
else()
    cmake_minimum_required (VERSION 3.6)
endif()

function(add_sample SAMPLE_NAME SOURCE INCLUDE SHADERS ASSETS)

    set_source_files_properties(${SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")
	set(ALL_ASSETS ${ASSETS} ${SHADERS})
    add_target_platform_app(${SAMPLE_NAME} "${SOURCE}" "${INCLUDE}" "${ALL_ASSETS}")

    if(PLATFORM_WIN32)
        set_target_properties(${SAMPLE_NAME} PROPERTIES
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        )
        if(MSVC)
            copy_required_dlls(${SAMPLE_NAME})
        endif()
        append_sample_base_win32_source(${SAMPLE_NAME})
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

    if(PLATFORM_WIN32 OR PLATFORM_LINUX)
        # Copy assets to target folder
        add_custom_command(TARGET ${SAMPLE_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets"
                "\"$<TARGET_FILE_DIR:${SAMPLE_NAME}>\"")
    endif()

    if(INSTALL_DILIGENT_SAMPLES)
        # Install instructions
        file(RELATIVE_PATH TUTORIAL_REL_PATH "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
        install(TARGETS     ${SAMPLE_NAME}
                DESTINATION ${TUTORIAL_REL_PATH})
        if(PLATFORM_LINUX OR PLATFORM_WIN32)
            install(DIRECTORY   "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
                    DESTINATION ${TUTORIAL_REL_PATH})
            get_supported_backends(BACKEND_LIBRARIES)
            install(TARGETS  ${BACKEND_LIBRARIES}
                    RUNTIME DESTINATION ${TUTORIAL_REL_PATH}
                    LIBRARY DESTINATION ${TUTORIAL_REL_PATH}
                    ARCHIVE DESTINATION ${TUTORIAL_REL_PATH})
        endif()
    endif()

endfunction()

add_subdirectory(SampleBase)
add_subdirectory(AntTweakBar)
add_subdirectory(Atmosphere)
