cmake_minimum_required (VERSION 3.6)

project(SampleBase)

if(PLATFORM_WIN32)
	set(SOURCE 
		src/Win32/SampleAppWin32.cpp
		src/SampleApp.cpp
	)
	set(INCLUDE 
		include/SampleApp.h
	)

elseif(PLATFORM_UNIVERSAL_WINDOWS)

	# Windows Runtime types cannot be included into static libraries
	# https://social.msdn.microsoft.com/Forums/en-US/269db513-64ef-4817-a025-43954f614eb3/lnk4264-why-are-static-libraries-not-recommended-when-authoring-windows-runtime-types?forum=winappswithnativecode
	# So as a workaround, we will include all source files into the target app project
	function(append_sample_base_uwp_source TARGET_NAME)
		get_target_property(SAMPLE_BASE_SOURCE_DIR SampleBase SOURCE_DIR)

		set(SAMPLE_BASE_UWP_SOURCE
			${SAMPLE_BASE_SOURCE_DIR}/src/UWP/TwEventUWP.cpp
			${SAMPLE_BASE_SOURCE_DIR}/src/UWP/SampleAppUWP.cpp
			${SAMPLE_BASE_SOURCE_DIR}/src/SampleApp.cpp
		)

		set(SAMPLE_BASE_UWP_INCLUDE
			${SAMPLE_BASE_SOURCE_DIR}/src/UWP/TwEventUWP.h
			${SAMPLE_BASE_SOURCE_DIR}/include/SampleApp.h
		)

		set(SAMPLE_BASE_UWP_INCLUDE_DIR
			${SAMPLE_BASE_SOURCE_DIR}/Src/UWP
		)

		target_sources(${TARGET_NAME} PRIVATE ${SAMPLE_BASE_UWP_SOURCE} ${SAMPLE_BASE_UWP_INCLUDE})
		source_group("src\\SampleBase" FILES ${SAMPLE_BASE_UWP_SOURCE})
		source_group("include\\SampleBase" FILES ${SAMPLE_BASE_UWP_INCLUDE})
		target_include_directories(${TARGET_NAME} PRIVATE ${SAMPLE_BASE_UWP_INCLUDE_DIR})
	endfunction()

elseif(PLATFORM_ANDROID)
	set(SOURCE
		src/Android/SampleAppAndroid.cpp
		src/SampleApp.cpp
	)
	set(INCLUDE 
		include/SampleApp.h
	)
elseif(PLATFORM_LINUX)
	set(SOURCE 
		src/Linux/SampleAppLinux.cpp
		src/SampleApp.cpp
	)
	set(INCLUDE 
		include/SampleApp.h
	)
elseif(PLATFORM_MACOS)
    set(SOURCE
        src/MacOS/Renderer.cpp
		src/SampleApp.cpp
    )
    set(INCLUDE
        src/MacOS/Renderer.h
		include/SampleApp.h
    )

	function(get_sample_base_macos_source APPLE_SOURCE APPLE_INCLUDE APPLE_RESOURCES APPLE_INFO_PLIST APPLE_INCLUDE_DIRS)
		get_target_property(SAMPLE_BASE_SOURCE_DIR SampleBase SOURCE_DIR)

		set(${APPLE_SOURCE}
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Source/main.m
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Source/Classes/OSX/WindowController.m
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Source/Classes/OSX/AppDelegate.m
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Source/Classes/OSX/FullscreenWindow.m
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Source/Classes/OSX/GLView.m
		)
        set_source_files_properties(${APPLE_SOURCE} PROPERTIES
            COMPILE_FLAGS "-x objective-c++"
        )
        set( APPLE_SOURCE ${APPLE_SOURCE} PARENT_SCOPE )

		set(${APPLE_INCLUDE}
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Source/Classes/OSX/WindowController.h
       		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Source/Classes/OSX/AppDelegate.h
       		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Source/Classes/OSX/FullscreenWindow.h
       		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Source/Classes/OSX/GLView.h
		    PARENT_SCOPE
		)

		set(${APPLE_RESOURCES}
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Data/OSX/Base.lproj/Main.storyboard
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Data/OSX/Images.xcassets/AppIcon.appiconset/dg.icns
		    PARENT_SCOPE
		)

		set(${APPLE_INFO_PLIST}
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Data/OSX/Info.plist
		    PARENT_SCOPE
		)

		set(${APPLE_INCLUDE_DIRS}
        		${SAMPLE_BASE_SOURCE_DIR}/Apple/Source/Classes/OSX
			PARENT_SCOPE
		)
	endfunction()

elseif(PLATFORM_IOS)
    set(SOURCE
        src/MacOS/Renderer.cpp
		src/SampleApp.cpp
    )
    set(INCLUDE
        src/MacOS/Renderer.h
		include/SampleApp.h
    )

	function(get_sample_base_ios_source APPLE_SOURCE APPLE_INCLUDE APPLE_RESOURCES APPLE_INFO_PLIST APPLE_INCLUDE_DIRS)
		get_target_property(SAMPLE_BASE_SOURCE_DIR SampleBase SOURCE_DIR)

		set(${APPLE_SOURCE}
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Source/main.m
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Source/Classes/iOS/AppDelegate.m
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Source/Classes/iOS/EAGLView.m
		)
        set_source_files_properties(${APPLE_SOURCE} PROPERTIES
            COMPILE_FLAGS "-x objective-c++"
        )
        set( APPLE_SOURCE ${APPLE_SOURCE} PARENT_SCOPE )

		set(${APPLE_INCLUDE}
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Source/Classes/iOS/AppDelegate.h
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Source/Classes/iOS/EAGLView.h
		    PARENT_SCOPE
		)

		set(${APPLE_RESOURCES}
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Data/iOS/Base.lproj/Main.storyboard
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Data/iOS/Base.lproj/LaunchScreen.xib
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Data/iOS/Images.xcassets/AppIcon.appiconset/dg-icon.png
		    PARENT_SCOPE
		)

		set(${APPLE_INFO_PLIST}
		    ${SAMPLE_BASE_SOURCE_DIR}/Apple/Data/iOS/info.plist
		    PARENT_SCOPE
		)

		set(${APPLE_INCLUDE_DIRS}
        		${SAMPLE_BASE_SOURCE_DIR}/Apple/Source/Classes/iOS
			PARENT_SCOPE
		)
	endfunction()

endif()

list(APPEND SOURCE src/SampleBase.cpp)
list(APPEND INCLUDE include/SampleBase.h)


add_library(SampleBase STATIC ${SOURCE} ${INCLUDE})
set_common_target_properties(SampleBase)

target_include_directories(SampleBase
PUBLIC
	include
)


if(MSVC)
	target_compile_options(SampleBase PRIVATE -DUNICODE)
	
	if(PLATFORM_UNIVERSAL_WINDOWS)
		# Disable w4189: local variable is initialized but not referenced
		# Disable w4063: case is not a valid value for switch of enum
		# Consume the windows runtime extensions (/ZW)
		target_compile_options(SampleBase INTERFACE /wd4189 /wd4063 /ZW)
	endif()
endif()

if(PLATFORM_WIN32)
	SET(ENGINE_LIBRARIES
		GraphicsEngineD3D11-shared
		GraphicsEngineD3D12-shared 
		GraphicsEngineOpenGL-shared
	)
elseif(PLATFORM_ANDROID)
	SET(ENGINE_LIBRARIES
		GraphicsEngineOpenGL-shared
	)
elseif(PLATFORM_UNIVERSAL_WINDOWS)
	SET(ENGINE_LIBRARIES
		GraphicsEngineD3D11-static
		GraphicsEngineD3D12-static
	)
elseif(PLATFORM_LINUX)
	SET(ENGINE_LIBRARIES
		GraphicsEngineOpenGL-shared
	)
elseif(PLATFORM_MACOS OR PLATFORM_IOS)
    SET(ENGINE_LIBRARIES
        GraphicsEngineOpenGL-shared
    )
else()
	message(FATAL_ERROR "Undefined platform")
endif()

target_link_libraries(SampleBase 
PRIVATE 
	BuildSettings 
PUBLIC
	NativeAppBase
	TwBarLib 
	Common 
	GraphicsTools
	TargetPlatform
	${ENGINE_LIBRARIES}
)

if(PLATFORM_UNIVERSAL_WINDOWS)
	target_link_libraries(SampleBase PRIVATE dxguid.lib)
elseif(PLATFORM_ANDROID)
	target_link_libraries(SampleBase PRIVATE GLESv3 PUBLIC NativeAppGlue)
elseif(PLATFORM_LINUX)
	target_link_libraries(SampleBase PRIVATE GL X11)
elseif(PLATFORM_MACOS OR PLATFORM_IOS)
    target_include_directories(SampleBase PUBLIC
        src/MacOS
    )
endif()

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})

set_target_properties(SampleBase PROPERTIES
	FOLDER Samples
)
