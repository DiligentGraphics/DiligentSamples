cmake_minimum_required (VERSION 3.6)

function(add_sample SAMPLE_NAME SOURCE INCLUDE SHADERS ASSETS)

    if(PLATFORM_WIN32 OR PLATFORM_UNIVERSAL_WINDOWS)
        add_executable(${SAMPLE_NAME} WIN32 ${SOURCE} ${INCLUDE} ${ASSETS} ${SHADERS})

        if(PLATFORM_WIN32)
            set_target_properties(${SAMPLE_NAME} PROPERTIES
                VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            )
            copy_required_dlls(${SAMPLE_NAME})
        endif()

        if(PLATFORM_UNIVERSAL_WINDOWS)
            get_sample_base_uwp_source(UWP_SOURCE UWP_INCLUDE UWP_INCLUDE_DIR)
            source_group("UWP\\src" FILES ${UWP_SOURCE})
            source_group("UWP\\include" FILES ${UWP_INCLUDE})
            target_include_directories(${SAMPLE_NAME} PRIVATE ${UWP_INCLUDE_DIR})
            target_sources(${SAMPLE_NAME} PRIVATE ${UWP_SOURCE} ${UWP_INCLUDE})

            set_source_files_properties(${ASSETS} ${SHADERS} PROPERTIES VS_DEPLOYMENT_CONTENT 1)
        endif()

        set_source_files_properties(${SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")

    elseif(PLATFORM_ANDROID)

        list(APPEND SOURCE src/AndroidMain.cpp)
        add_library(${SAMPLE_NAME} SHARED ${SOURCE} ${INCLUDE})

    elseif(PLATFORM_LINUX)

        add_executable(${SAMPLE_NAME} ${SOURCE} ${INCLUDE})

    elseif(PLATFORM_MACOS OR PLATFORM_IOS)
	if(PLATFORM_MACOS)
	    get_sample_base_macos_source(APPLE_SOURCE APPLE_INCLUDE APPLE_RESOURCES APPLE_INFO_PLIST APPLE_INCLUDE_DIRS)
	else()
   	    get_sample_base_ios_source(APPLE_SOURCE APPLE_INCLUDE APPLE_RESOURCES APPLE_INFO_PLIST APPLE_INCLUDE_DIRS)
	endif()
        add_executable(${SAMPLE_NAME} MACOSX_BUNDLE ${SOURCE} ${APPLE_SOURCE} ${INCLUDE} ${APPLE_INCLUDE} ${ASSETS} ${SHADERS} ${APPLE_RESOURCES})
        set_target_properties(${SAMPLE_NAME} PROPERTIES
            MACOSX_BUNDLE_GUI_IDENTIFIER "com.diligentengine.samples.${SAMPLE_NAME}"
            MACOSX_BUNDLE_INFO_PLIST "${APPLE_INFO_PLIST}"
            RESOURCE "${APPLE_RESOURCES}"
        )
        source_group("MacOS" FILES ${APPLE_SOURCE})
        source_group("MacOS" FILES ${APPLE_INCLUDE})
        source_group("Resources" FILES ${APPLE_RESOURCES})
        target_include_directories(${SAMPLE_NAME} PRIVATE ${APPLE_INCLUDE_DIRS})

	if(PLATFORM_MACOS)
            find_package(OpenGL REQUIRED)

            find_library(CORE_VIDEO CoreVideo)
            if (NOT CORE_VIDEO)
                 message(FATAL_ERROR "CoreVideo not found")
            endif()
	    target_link_libraries(${SAMPLE_NAME} PRIVATE ${OPENGL_LIBRARY} ${CORE_VIDEO})
	else()
	    find_library(OPENGLES OpenGLES)
            if (NOT OPENGLES)
                message(FATAL_ERROR "OpenGLES not found")
            endif()

	    find_library(UIKIT UIKit)
            if (NOT UIKIT)
                 message(FATAL_ERROR "UIKIT not found")
            endif()

     	    find_library(CORE_ANIMATION QuartzCore)
      	    if (NOT CORE_ANIMATION)
        	 message(FATAL_ERROR "QuartzCore (CoreAnimation) not found")
    	    endif()

	    target_link_libraries(${SAMPLE_NAME} PRIVATE ${OPENGLES} ${UIKIT} ${CORE_ANIMATION})
	endif()

    else()
        message(FATAL_ERROR "Unknown platform")
    endif()

    target_include_directories(${SAMPLE_NAME}
    PRIVATE
        src
    )

    target_link_libraries(${SAMPLE_NAME}
    PRIVATE
        BuildSettings
        SampleBase
        Lua
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
