cmake_minimum_required (VERSION 3.8)

project(AntTweakBarSample CXX)

set(SOURCE
	src/MengerSponge.cpp
)

set(INCLUDE
	src/MengerSpongeSample.h
)

if(WIN32)
	#add_definitions(-DNOMINMAX)
	add_executable(AntTweakBarSample WIN32 ${SOURCE} ${INCLUDE})
	set_target_properties(AntTweakBarSample PROPERTIES 
		VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build"
	)
	add_custom_command(TARGET AntTweakBarSample POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"$<TARGET_FILE:GraphicsEngineD3D11-shared>"
			"$<TARGET_FILE_DIR:AntTweakBarSample>")
	add_custom_command(TARGET AntTweakBarSample POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"$<TARGET_FILE:GraphicsEngineD3D12-shared>"
			"$<TARGET_FILE_DIR:AntTweakBarSample>")
	add_custom_command(TARGET AntTweakBarSample POST_BUILD
		COMMAND ${CMAKE_COMMAND} -E copy_if_different
			"$<TARGET_FILE:GraphicsEngineOpenGL-shared>"
			"$<TARGET_FILE_DIR:AntTweakBarSample>")
elseif(APPLE)
	add_executable(AntTweakBarSample MACOSX_BUNDLE Main.cpp)
elseif(UNIX)
	add_executable(AntTweakBarSample Main.cpp)
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

set_target_properties(AntTweakBarSample PROPERTIES
	FOLDER Samples
)
