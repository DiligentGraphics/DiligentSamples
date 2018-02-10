cmake_minimum_required (VERSION 3.6)

function(add_tutorial TUTORIAL_NAME SOURCE INCLUDE SHADERS ASSETS)

	set_source_files_properties(${SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")

	if(PLATFORM_WIN32)
		
		add_win32_app(${TUTORIAL_NAME} "${SOURCE}" "${INCLUDE}" "${ASSETS}")
		set_target_properties(${TUTORIAL_NAME} PROPERTIES 
			VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
		)
		copy_required_dlls(${TUTORIAL_NAME})

	elseif(PLATFORM_UNIVERSAL_WINDOWS)
			add_uwp_app(${TUTORIAL_NAME} "${SOURCE}" "${INCLUDE}" "${ASSETS}")
			append_sample_base_uwp_source(${TUTORIAL_NAME})
			target_sources(${TUTORIAL_NAME} PRIVATE ${ASSETS})
			set_source_files_properties(${ASSETS} PROPERTIES 
				VS_DEPLOYMENT_CONTENT 1
				VS_DEPLOYMENT_LOCATION "."
			)

	elseif(PLATFORM_ANDROID)

		list(APPEND SOURCE ../Common/Android/AndroidMain.cpp)
		add_library(${TUTORIAL_NAME} SHARED ${SOURCE} ${INCLUDE})

	elseif(PLATFORM_LINUX)

		add_linux_app(${TUTORIAL_NAME} "${SOURCE}" "${INCLUDE}" "${ASSETS}")

	elseif(PLATFORM_MACOS OR PLATFORM_IOS)
		if(PLATFORM_MACOS)
		    get_sample_base_macos_source(APPLE_SOURCE APPLE_INCLUDE APPLE_RESOURCES APPLE_INFO_PLIST APPLE_INCLUDE_DIRS)
		else()
   		    get_sample_base_ios_source(APPLE_SOURCE APPLE_INCLUDE APPLE_RESOURCES APPLE_INFO_PLIST APPLE_INCLUDE_DIRS)
		endif()
		
		add_executable(${TUTORIAL_NAME} MACOSX_BUNDLE ${SOURCE} ${APPLE_SOURCE} ${INCLUDE} ${APPLE_INCLUDE} ${ASSETS} ${APPLE_RESOURCES})
		set(RESOURCES ${APPLE_RESOURCES} ${ASSETS})
		set_target_properties(${TUTORIAL_NAME} PROPERTIES
			MACOSX_BUNDLE_GUI_IDENTIFIER "com.diligentengine.tutorials.${TUTORIAL_NAME}"
			MACOSX_BUNDLE_INFO_PLIST "${APPLE_INFO_PLIST}"
			RESOURCE "${RESOURCES}"
		)
  
		source_group("Apple" FILES ${APPLE_SOURCE})
		source_group("Apple" FILES ${APPLE_INCLUDE})
		source_group("Resources" FILES ${APPLE_RESOURCES})
		target_include_directories(${TUTORIAL_NAME} PRIVATE ${APPLE_INCLUDE_DIRS})
		if(PLATFORM_MACOS)
                     find_package(OpenGL REQUIRED)

    		     find_library(CORE_VIDEO CoreVideo)
   		     if (NOT CORE_VIDEO)
        		 message(FATAL_ERROR "CoreVideo not found")
    		     endif()
		     target_link_libraries(${TUTORIAL_NAME} PRIVATE ${OPENGL_LIBRARY} ${CORE_VIDEO})
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

	    	    target_link_libraries(${TUTORIAL_NAME} PRIVATE ${OPENGLES} ${UIKIT} ${CORE_ANIMATION})
		endif()		     
	else()
		message(FATAL_ERROR "Unknown platform")
	endif()


	target_include_directories(${TUTORIAL_NAME}
	PRIVATE	
		src
	)

	target_link_libraries(${TUTORIAL_NAME}
	PRIVATE
		NativeAppBase	
		BuildSettings
		SampleBase 
	)
	set_common_target_properties(${TUTORIAL_NAME})

	if(MSVC)
		# Disable MSVC-specific warnings
		# - w4201: nonstandard extension used: nameless struct/union
		target_compile_options(${TUTORIAL_NAME} PRIVATE /wd4201)
	endif()
	
	set_target_properties(${TUTORIAL_NAME} PROPERTIES
		FOLDER Tutorials
	)

	source_group("src" FILES ${SOURCE} ${INCLUDE})
	source_group("assets" FILES ${ASSETS})	

endfunction()


add_subdirectory(Tutorial01_HelloTriangle)
add_subdirectory(Tutorial02_Cube)
add_subdirectory(Tutorial03_Texturing)
add_subdirectory(Tutorial04_Instancing)
add_subdirectory(Tutorial05_TextureArray)
add_subdirectory(Tutorial06_Multithreading)
add_subdirectory(Tutorial07_GeometryShader)
add_subdirectory(Tutorial08_Tessellation)