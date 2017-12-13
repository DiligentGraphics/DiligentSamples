cmake_minimum_required (VERSION 3.8)

project(AntTweakBarSample CXX)

set(SOURCE
	src/MengerSponge.cpp
)

set(INCLUDE
	src/MengerSpongeSample.h
)

set(SHADERS
	build/assets/shaders/MainPS_DX.hlsl
	build/assets/shaders/MainPS_GL.glsl
	build/assets/shaders/MainVS_DX.hlsl
	build/assets/shaders/MainVS_GL.glsl
	build/assets/MengerSponge.lua
)

foreach(FILE ${SHADERS})
	set_source_files_properties(
		${FILE} PROPERTIES HEADER_FILE_ONLY TRUE
	)
endforeach(FILE)

if(PLATFORM_WIN32)
	#add_definitions(-DNOMINMAX)
	add_executable(AntTweakBarSample WIN32 ${SOURCE} ${INCLUDE} ${SHADERS})
	set_target_properties(AntTweakBarSample PROPERTIES 
		VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
	)
	copy_required_dlls(AntTweakBarSample)

#elseif(APPLE)
#	add_executable(AntTweakBarSample MACOSX_BUNDLE Main.cpp)
#elseif(UNIX)
#	add_executable(AntTweakBarSample Main.cpp)
else()
	message(FATAL_ERROR "Unknown platform")
endif()

target_include_directories(AntTweakBarSample
PRIVATE	
	Src
)

target_link_libraries(AntTweakBarSample
PRIVATE
	SampleBase 
	Lua
	RenderScript
)

if(MSVC)
	# Disable MSVC-specific warnings
	# - w4201: nonstandard extension used: nameless struct/unio
	target_compile_options(AntTweakBarSample PRIVATE /wd4201)
	# Enable link-time code generation for release builds (I was not able to 
	# find any way to set these settings through interface library BuildSettings)
	set_target_properties(AntTweakBarSample PROPERTIES
		LINK_FLAGS_RELEASE /LTCG
		LINK_FLAGS_MINSIZEREL /LTCG
		LINK_FLAGS_RELWITHDEBINFO /LTCG
	)
endif()


set_target_properties(AntTweakBarSample PROPERTIES
	FOLDER Samples
)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("shaders" FILES ${SHADERS})
