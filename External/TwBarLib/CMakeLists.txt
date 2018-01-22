cmake_minimum_required (VERSION 3.6)

project(TwBarLib CXX)

set(SOURCE 
	src/TwBar.cpp
	src/TwColors.cpp
	src/TwFonts.cpp
	src/TwGraphImpl.cpp
	src/TwMgr.cpp
	src/TwPrecomp.cpp
	src/TwEventGLFW.c
	src/TwEventSDL.c
	src/TwEventSDL12.c
	src/TwEventSDL13.c
	src/TwEventSFML.cpp
)

if(PLATFORM_WIN32)
	list(APPEND SOURCE src/TwEventGLUT.c)
	list(APPEND SOURCE src/TwEventWin.c)
elseif(PLATFORM_ANDROID)
	set_source_files_properties(
		src/TwEventGLFW.c
		src/TwEventSDL.c
		src/TwEventSDL12.c
		src/TwEventSDL13.c
		PROPERTIES 
		LANGUAGE CXX
		# Setting LANGUAGE to CXX is not enough for clang, so use -x c++ to compile c files as c++ and suppress the warning
		COMPILE_FLAGS "-x c++"
	)
elseif(PLATFORM_LINUX)
	list(APPEND SOURCE src/TwEventX11.c)
endif()

set(PRIVATE_HEADERS 
	src/AntPerfTimer.h
	src/MiniSDL12.h
	src/MiniSDL13.h
	src/MiniSFML16.h
	src/TwBar.h
	src/TwColors.h
	src/TwFonts.h
	src/TwGraph.h
	src/TwGraphImpl.h
	src/TwMgr.h
	src/TwPlatformStubs.h
	src/TwPrecomp.h
)

if(PLATFORM_WIN32)
	list(APPEND PRIVATE_HEADERS src/resource.h)
endif()


set(PUBLIC_HEADERS 
	include/AntTweakBar.h
)

set(WIN32_RESOURCE_FILES
	res/AntTweakBar.rc	
	res/cur00000.cur
	res/cur00001.cur
	res/cur00002.cur
	res/cur00003.cur
	res/cur00004.cur
	res/cur00005.cur
	res/cur00006.cur
	res/cur00007.cur
	res/cur00008.cur
	res/cur00009.cur
	res/cur00010.cur
	res/cur00011.cur
	res/cur00012.cur
	res/cur00013.cur
	res/FontChars.txt
	res/FontFixed1.pgm
	res/FontLargeAA.pgm
	res/FontNormal.pgm
	res/FontSmall.pgm
)

set(DX_SHADERS
	src/Shaders/LineRectCstColorVS.hlsl
	src/Shaders/LineRectPS.hlsl
	src/Shaders/LineRectVS.hlsl
	src/Shaders/TextCstColorVS.hlsl
	src/Shaders/TextPS.hlsl
	src/Shaders/TextVS.hlsl
)

set(GL_SHADERS
	src/Shaders/LineRectCstColorVS.glsl
	src/Shaders/LineRectPS.glsl
	src/Shaders/LineRectVS.glsl
	src/Shaders/TextCstColorVS.glsl
	src/Shaders/TextPS.glsl
	src/Shaders/TextVS.glsl
)

set(TW_BAR_ALL_SOURCE ${SOURCE} ${PRIVATE_HEADERS} ${PUBLIC_HEADERS} 
	${DX_SHADERS}
	${GL_SHADERS}
)

set_property(SOURCE ${DX_SHADERS} PROPERTY VS_TOOL_OVERRIDE "None")

if(PLATFORM_WIN32)
	list(APPEND TW_BAR_ALL_SOURCE ${WIN32_RESOURCE_FILES})
endif()

add_library(
	TwBarLib STATIC ${TW_BAR_ALL_SOURCE}
)
set_common_target_properties(TwBarLib)

if(MSVC)
	target_compile_options(TwBarLib PRIVATE /wd4302 /wd4456)
endif()

target_link_libraries(TwBarLib 
PRIVATE 
	BuildSettings 
	Common 
	GraphicsEngineInterface
	PlatformInterface
)

target_include_directories(TwBarLib
PUBLIC
	include
PRIVATE
	src
)

if(PLATFORM_WIN32)
	target_include_directories(TwBarLib
	PRIVATE 
		build/Win32
	)
endif()

# Create custom target to convert HLSLDefinitions.fxh to HLSLDefinitions_inc.fxh
add_custom_target(ProcessTwBarLibShaders
SOURCES
	${DX_SHADERS}
	${GL_SHADERS}
)

if(NOT FILE2STRING_PATH STREQUAL "")
	set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/ShadersInc)
	file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

	foreach(FILE ${DX_SHADERS})
	  get_filename_component(FILE_NAME_WE ${FILE} NAME_WE)
	  add_custom_command(TARGET ProcessTwBarLibShaders 
						 # Unfortunately it is not possible to set TARGET directly to TwBarLib
						 # because PRE_BUILD is only supported on Visual Studio 8 or later. For all other generators 
						 # PRE_BUILD is treated as PRE_LINK.
						 COMMAND ${FILE2STRING_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/${FILE} ${SHADER_OUTPUT_DIR}/${FILE_NAME_WE}_DX.h
						 MAIN_DEPENDENCY ${FILE} # the primary input source file to the command
						 COMMENT "Processing shader ${FILE}"
						 VERBATIM
	  )
	endforeach(FILE)

	foreach(FILE ${GL_SHADERS})
	  get_filename_component(FILE_NAME_WE ${FILE} NAME_WE)
	  add_custom_command(TARGET ProcessTwBarLibShaders 
						 # Unfortunately it is not possible to set TARGET directly to TwBarLib
						 # because PRE_BUILD is only supported on Visual Studio 8 or later. For all other generators 
						 # PRE_BUILD is treated as PRE_LINK.
						 COMMAND ${FILE2STRING_PATH} ${CMAKE_CURRENT_SOURCE_DIR}/${FILE} ${SHADER_OUTPUT_DIR}/${FILE_NAME_WE}_GL.h
						 MAIN_DEPENDENCY ${FILE} # the primary input source file to the command
						 COMMENT "Processing shader ${FILE}"
						 VERBATIM
	  )
	endforeach(FILE)

	add_dependencies(TwBarLib ProcessTwBarLibShaders)
	set_target_properties(ProcessTwBarLibShaders PROPERTIES
		FOLDER Samples/External
	)
else()
	message(WARNING "File2String utility is currently unavailable on this host system. This is not an issues unless you modify TwBarLib shaders")
endif()

target_compile_definitions(TwBarLib PUBLIC -DTW_STATIC)
if(PLATFORM_LINUX)
	target_compile_definitions(TwBarLib PUBLIC _UNIX PRIVATE __PLACEMENT_NEW_INLINE)
	target_compile_options(TwBarLib PRIVATE -fno-strict-aliasing)
	target_link_libraries(TwBarLib PRIVATE glew-static)
endif()

if(PLATFORM_MACOS)
    target_compile_definitions(TwBarLib PUBLIC _MACOSX PRIVATE __PLACEMENT_NEW_INLINE)
    target_compile_options(TwBarLib PRIVATE -fno-strict-aliasing -x objective-c++)
    find_library(APP_KIT AppKit)
    if (NOT APP_KIT)
        message(FATAL_ERROR "AppKit not found")
    endif()
    target_link_libraries(TwBarLib PRIVATE ${APP_KIT})
endif()



source_group("Source Files" FILES ${SOURCE})
source_group("Private Header Files" FILES ${PRIVATE_HEADERS})
source_group("Public Header Files" FILES ${PUBLIC_HEADERS})
source_group("Shaders\\DX" FILES ${DX_SHADERS})
source_group("Shaders\\GL" FILES ${GL_SHADERS})

if(WIN32)
	source_group("Resource Files" FILES ${WIN32_RESOURCE_FILES})
endif()

set_target_properties(TwBarLib PROPERTIES
	FOLDER Samples/External
)
