cmake_minimum_required (VERSION 3.6)

function(add_tutorial TUTORIAL_NAME SOURCE INCLUDE SHADERS ASSETS)

	if(PLATFORM_WIN32 OR PLATFORM_UNIVERSAL_WINDOWS)
		add_executable(${TUTORIAL_NAME} WIN32 ${SOURCE} ${INCLUDE} ${ASSETS})
		
		if(PLATFORM_WIN32)
			set_target_properties(${TUTORIAL_NAME} PROPERTIES 
				VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/assets"
			)
			copy_required_dlls(${TUTORIAL_NAME})
		endif()
		set_source_files_properties(${SHADERS} PROPERTIES VS_TOOL_OVERRIDE "None")

		if(PLATFORM_UNIVERSAL_WINDOWS)
			get_sample_base_uwp_source(UWP_SOURCE UWP_INCLUDE UWP_INCLUDE_DIR)
			source_group("UWP\\src" FILES ${UWP_SOURCE})
			source_group("UWP\\include" FILES ${UWP_INCLUDE})
			target_include_directories(${TUTORIAL_NAME} PRIVATE ${UWP_INCLUDE_DIR})
			target_sources(${TUTORIAL_NAME} PRIVATE ${UWP_SOURCE} ${UWP_INCLUDE})

			set_property(SOURCE ${ASSETS} PROPERTY VS_DEPLOYMENT_CONTENT 1)
			set_property(SOURCE ${ASSETS} PROPERTY VS_DEPLOYMENT_LOCATION ".")
		endif()

	elseif(PLATFORM_ANDROID)

		list(APPEND SOURCE ../Common/Android/AndroidMain.cpp)
		add_library(${TUTORIAL_NAME} SHARED ${SOURCE} ${INCLUDE})

	elseif(PLATFORM_LINUX)

		add_executable(${TUTORIAL_NAME} ${SOURCE} ${INCLUDE})

	elseif(PLATFORM_MACOS)
		get_sample_base_macos_source(MAC_SOURCE MAC_INCLUDE MAC_RESOURCES MAC_INFO_PLIST MAC_INCLUDE_DIRS)
		add_executable(${TUTORIAL_NAME} MACOSX_BUNDLE ${SOURCE} ${MAC_SOURCE} ${INCLUDE} ${MAC_INCLUDE} ${ASSETS} ${MAC_RESOURCES})
		set(RESOURCES ${MAC_RESOURCES} ${ASSETS})
		set_target_properties(${TUTORIAL_NAME} PROPERTIES
			MACOSX_BUNDLE_GUI_IDENTIFIER "com.diligentengine.tutorials.${TUTORIAL_NAME}"
			MACOSX_BUNDLE_INFO_PLIST "${MAC_INFO_PLIST}"
			RESOURCE "${RESOURCES}"
		)
  
		source_group("MacOS" FILES ${MAC_SOURCE})
		source_group("MacOS" FILES ${MAC_INCLUDE})
		source_group("Resources" FILES ${MAC_RESOURCES})
		target_include_directories(${TUTORIAL_NAME} PRIVATE ${MAC_INCLUDE_DIRS})
                find_package(OpenGL REQUIRED)

    		find_library(CORE_VIDEO CoreVideo)
   		if (NOT CORE_VIDEO)
        		message(FATAL_ERROR "CoreVideo not found")
    		endif()
		target_link_libraries(${TUTORIAL_NAME} PRIVATE ${OPENGL_LIBRARY} ${CORE_VIDEO})  
	else()
		message(FATAL_ERROR "Unknown platform")
	endif()


	target_include_directories(${TUTORIAL_NAME}
	PRIVATE	
		src
	)

	target_link_libraries(${TUTORIAL_NAME}
	PRIVATE
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