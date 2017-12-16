cmake_minimum_required (VERSION 3.6)

project(SampleBase)

if(PLATFORM_WIN32)
	set(SOURCE 
		src/Win32/WinMain.cpp
	)
elseif(PLATFORM_UNIVERSAL_WINDOWS)

	# Windows Runtime types cannot be included into static libraries
	# https://social.msdn.microsoft.com/Forums/en-US/269db513-64ef-4817-a025-43954f614eb3/lnk4264-why-are-static-libraries-not-recommended-when-authoring-windows-runtime-types?forum=winappswithnativecode
	# So as a workaround, we will include all source files into the target app project
	function(get_sample_base_uwp_source UWP_SOURCE UWP_INCLUDE UWP_INCLUDE_DIR)
		get_target_property(SAMPLE_BASE_SOURCE_DIR SampleBase SOURCE_DIR)

		set(${UWP_SOURCE}
			${SAMPLE_BASE_SOURCE_DIR}/src/UWP/Common/DeviceResources.cpp
			${SAMPLE_BASE_SOURCE_DIR}/src/UWP/App.cpp
			${SAMPLE_BASE_SOURCE_DIR}/src/UWP/SampleAppMain.cpp
			${SAMPLE_BASE_SOURCE_DIR}/src/UWP/TwEventUWP.cpp
			PARENT_SCOPE
		)

		set(${UWP_INCLUDE}
			${SAMPLE_BASE_SOURCE_DIR}/src/UWP/Common/DeviceResources.h
			${SAMPLE_BASE_SOURCE_DIR}/src/UWP/Common/DirectXHelper.h
			${SAMPLE_BASE_SOURCE_DIR}/src/UWP/Common/StepTimer.h
			${SAMPLE_BASE_SOURCE_DIR}/src/UWP/App.h
			${SAMPLE_BASE_SOURCE_DIR}/src/UWP/pch2.h
			${SAMPLE_BASE_SOURCE_DIR}/src/UWP/SampleAppMain.h
			${SAMPLE_BASE_SOURCE_DIR}/src/UWP/TwEventUWP.h
			PARENT_SCOPE
		)

		set(${UWP_INCLUDE_DIR}
			${SAMPLE_BASE_SOURCE_DIR}/Src/UWP
			PARENT_SCOPE
		)
	endfunction(get_sample_base_uwp_source)

	# This is only necessary to have at least one source file so that
	# SampleBase.lib will is created and link is not failed
	set(SOURCE 
		src/UWP/dummy.cpp
	)

elseif(PLATFORM_ANDROID)
	set(SOURCE 
		src/Android/AndroidMainImpl.cpp
	)
endif()

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
endif()

target_link_libraries(SampleBase 
PRIVATE 
	BuildSettings 
PUBLIC
	TwBarLib 
	Common 
	GraphicsEngine
	GraphicsTools
	TargetPlatform
	${ENGINE_LIBRARIES}
)

if(PLATFORM_UNIVERSAL_WINDOWS)
	target_link_libraries(SampleBase PRIVATE dxguid.lib)
elseif(PLATFORM_ANDROID)
	target_link_libraries(SampleBase PRIVATE NDKHelper GLESv3 android PUBLIC NativeAppGlue)
	target_include_directories(SampleBase PRIVATE
		${ANDROID_NDK}/sources/android/cpufeatures
	)
endif()

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})

set_target_properties(SampleBase PROPERTIES
	FOLDER Samples
)
