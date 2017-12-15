cmake_minimum_required (VERSION 3.10)

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
)
set_property(SOURCE build/assets/shaders/MainPS_DX.hlsl PROPERTY VS_TOOL_OVERRIDE "None")
set_property(SOURCE build/assets/shaders/MainVS_DX.hlsl PROPERTY VS_TOOL_OVERRIDE "None")

set(ASSETS
	${SHADERS}
	build/assets/MengerSponge.lua
)

if(PLATFORM_WIN32 OR PLATFORM_UNIVERSAL_WINDOWS)
	add_executable(AntTweakBarSample WIN32 ${SOURCE} ${INCLUDE} ${ASSETS})

	if(PLATFORM_WIN32)
		set_target_properties(AntTweakBarSample PROPERTIES 
			VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
		)
		copy_required_dlls(AntTweakBarSample)
	endif()

#elseif(APPLE)
#	add_executable(AntTweakBarSample MACOSX_BUNDLE Main.cpp)
#elseif(UNIX)
#	add_executable(AntTweakBarSample Main.cpp)
else()
	message(FATAL_ERROR "Unknown platform")
endif()

if(PLATFORM_UNIVERSAL_WINDOWS)
	get_sample_base_uwp_source(UWP_SOURCE UWP_INCLUDE UWP_INCLUDE_DIR)
	source_group("UWP\\src" FILES ${UWP_SOURCE})
	source_group("UWP\\include" FILES ${UWP_INCLUDE})
	target_include_directories(AntTweakBarSample PRIVATE ${UWP_INCLUDE_DIR})
	target_sources(AntTweakBarSample PRIVATE ${UWP_SOURCE} ${UWP_INCLUDE})

	set_property(SOURCE ${ASSETS} PROPERTY VS_DEPLOYMENT_CONTENT 1)
	set_property(SOURCE ${SHADERS} PROPERTY VS_DEPLOYMENT_LOCATION "assets/shaders")
	set_property(SOURCE build/assets/MengerSponge.lua PROPERTY VS_DEPLOYMENT_LOCATION "assets")
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
set_common_target_properties(AntTweakBarSample)

if(MSVC)
	# Disable MSVC-specific warnings
	# - w4201: nonstandard extension used: nameless struct/unio
	target_compile_options(AntTweakBarSample PRIVATE /wd4201)
endif()


set_target_properties(AntTweakBarSample PROPERTIES
	FOLDER Samples
)

source_group("src" FILES ${SOURCE})
source_group("include" FILES ${INCLUDE})
source_group("assets\\shaders" FILES ${SHADERS})
source_group("assets" FILES build/assets/MengerSponge.lua)
