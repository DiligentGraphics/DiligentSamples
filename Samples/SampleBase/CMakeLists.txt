cmake_minimum_required (VERSION 3.3)

project(SampleBase)

if(PLATFORM_WIN32)
	set(SOURCE 
		src/Win32/WinMain.cpp
	)
elseif(PLATFORM_ANDROID)
	set(SOURCE 
		src/Android/AndroidMainImpl.cpp
	)
endif()

set(INCLUDE
	include/SampleBase.h
)


add_library(SampleBase STATIC ${SOURCE} ${INCLUDE})

target_include_directories(SampleBase
PUBLIC
	include
)

if(MSVC)
	target_compile_options(SampleBase PRIVATE -DUNICODE)

	# Enable link-time code generation for release builds (I was not able to 
	# find any way to set these settings through interface library BuildSettings)
	set_target_properties(SampleBase PROPERTIES
		STATIC_LIBRARY_FLAGS_RELEASE /LTCG
		STATIC_LIBRARY_FLAGS_MINSIZEREL /LTCG
		STATIC_LIBRARY_FLAGS_RELWITHDEBINFO /LTCG
	)
endif()

target_link_libraries(SampleBase 
PRIVATE 
	BuildSettings 
PUBLIC
	TwBarLib 
	Common 
	GraphicsEngine
	GraphicsEngineD3D11-shared
	GraphicsEngineD3D12-shared 
	GraphicsEngineOpenGL-shared
	GraphicsTools
	TargetPlatform
)

source_group("Source Files" FILES ${SOURCE})
source_group("Header Files" FILES ${INCLUDE})

set_target_properties(SampleBase PROPERTIES
	FOLDER Samples
)
