cmake_minimum_required (VERSION 3.8)

project(AtmosphereSample CXX)

set(SOURCE
	src/AtmosphereSample.cpp
	src/Terrain/EarthHemisphere.cpp
	src/Terrain/ElevationDataSource.cpp
	src/LightSctrPostProcess.cpp
	src/pch.cpp
)

set(INCLUDE
	src/AtmosphereSample.h
	src/Terrain/DynamicQuadTreeNode.h
	src/Terrain/EarthHemisphere.h
	src/Terrain/ElevationDataSource.h
	src/Terrain/HierarchyArray.h
	src/LightSctrPostProcess.h
	src/pch.h
)

if(WIN32)
	add_executable(AtmosphereSample WIN32 ${SOURCE} ${INCLUDE})
	set_target_properties(AtmosphereSample PROPERTIES 
		VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build/assets"
	)
elseif(APPLE)
	add_executable(AtmosphereSample MACOSX_BUNDLE Main.cpp)
elseif(UNIX)
	add_executable(AtmosphereSample Main.cpp)
else()
	message(FATAL_ERROR "Unknown platform")
endif()

target_include_directories(AtmosphereSample
PRIVATE
	Src
	Src/Terrain
	build/assets/shaders
)

target_link_libraries(AtmosphereSample
PRIVATE
	SampleBase 
	Lua
	TextureLoader
	RenderScript
)

target_link_libraries(AtmosphereSample ${LINK_LIBRARIES})

source_group("Source Files" FILES ${SOURCE})
source_group("Header Files" FILES ${PRIVATE_HEADERS})
