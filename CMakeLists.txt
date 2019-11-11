if(PLATFORM_WIN32 OR PLATFORM_LINUX)
    cmake_minimum_required (VERSION 3.13)
    option(INSTALL_DILIGENT_SAMPLES "Enable installation of samples and tutorials" ON)
else()
    cmake_minimum_required (VERSION 3.6)
    set(INSTALL_DILIGENT_SAMPLES OFF)
endif()

function(add_sample_app APP_NAME IDE_FOLDER SOURCE INCLUDE SHADERS ASSETS)

	set_source_files_properties(${SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")
	set(ALL_ASSETS ${ASSETS} ${SHADERS})
	add_target_platform_app(${APP_NAME} "${SOURCE}" "${INCLUDE}" "${ALL_ASSETS}")

	set_source_files_properties(${ALL_ASSETS} PROPERTIES 
		VS_DEPLOYMENT_LOCATION "."
		MACOSX_PACKAGE_LOCATION "Resources" 
	)

	if(PLATFORM_WIN32)
		set_target_properties(${APP_NAME} PROPERTIES 
			VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
		)
		copy_required_dlls(${APP_NAME})
        append_sample_base_win32_source(${APP_NAME})
	elseif(PLATFORM_UNIVERSAL_WINDOWS)
			append_sample_base_uwp_source(${APP_NAME})
	endif()

	target_include_directories(${APP_NAME}
	PRIVATE	
		src
	)

	target_link_libraries(${APP_NAME}
	PRIVATE
		NativeAppBase
		Diligent-BuildSettings
		SampleBase 
	)
	set_common_target_properties(${APP_NAME})

	if(MSVC)
		# Disable MSVC-specific warnings
		# - w4201: nonstandard extension used: nameless struct/union
		target_compile_options(${APP_NAME} PRIVATE /wd4201)
	endif()

	set_target_properties(${APP_NAME} PROPERTIES
		FOLDER ${IDE_FOLDER}
	)

	source_group("src" FILES ${SOURCE} ${INCLUDE})
	source_group("assets" FILES ${ALL_ASSETS})	

    target_sources(${APP_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/readme.md")
    set_source_files_properties(
        "${CMAKE_CURRENT_SOURCE_DIR}/readme.md" PROPERTIES HEADER_FILE_ONLY TRUE
    )

    if(PLATFORM_WIN32 OR PLATFORM_LINUX)
        # Copy assets to target folder
        add_custom_command(TARGET ${APP_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/assets"
                "\"$<TARGET_FILE_DIR:${APP_NAME}>\"")
    endif()

    if(INSTALL_DILIGENT_SAMPLES)
        # Install instructions
        file(RELATIVE_PATH TUTORIAL_REL_PATH "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
        install(TARGETS     ${APP_NAME}
				DESTINATION ${TUTORIAL_REL_PATH})

        if(PLATFORM_LINUX OR PLATFORM_WIN32)
            install(DIRECTORY   "${CMAKE_CURRENT_SOURCE_DIR}/assets/"
					DESTINATION ${TUTORIAL_REL_PATH})
		endif()

		if(PLATFORM_WIN32)
            get_supported_backends(BACKEND_LIBRARIES)
            install(TARGETS  ${BACKEND_LIBRARIES}
                    RUNTIME DESTINATION ${TUTORIAL_REL_PATH}
                    LIBRARY DESTINATION ${TUTORIAL_REL_PATH}
					ARCHIVE DESTINATION ${TUTORIAL_REL_PATH})
		endif()

		if(PLATFORM_LINUX)
			set_target_properties(${APP_NAME} PROPERTIES
				INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${DILIGENT_CORE_INSTALL_DIR}/bin"
			)
		endif()
    endif()

endfunction()

add_subdirectory(ThirdParty)
if(TARGET NativeAppBase AND TARGET Diligent-TextureLoader AND TARGET Diligent-Imgui)
    add_subdirectory(SampleBase)
endif()
if(TARGET SampleBase)
    add_subdirectory(Samples)
    add_subdirectory(Tutorials)
endif()

file(RELATIVE_PATH SAMPLES_ROOT_REL_PATH "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
install(FILES License.txt DESTINATION "${SAMPLES_ROOT_REL_PATH}/Licenses" RENAME DiligentEngine-License.txt)
