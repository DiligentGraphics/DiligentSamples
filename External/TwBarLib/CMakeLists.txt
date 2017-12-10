cmake_minimum_required (VERSION 3.3)

project(TwBarLib CXX)

set(SOURCE 
	src/TwBar.cpp
	src/TwColors.cpp
	src/TwFonts.cpp
	src/TwGraphImpl.cpp
	src/TwMgr.cpp
	src/TwPrecomp.cpp
)

if(PLATFORM_WIN32)
	list(APPEND SOURCE src/TwEventGLFW.c)
	list(APPEND SOURCE src/TwEventGLUT.c)
	list(APPEND SOURCE src/TwEventSDL.c)
	list(APPEND SOURCE src/TwEventSDL12.c)
	list(APPEND SOURCE src/TwEventSDL13.c)
	list(APPEND SOURCE src/TwEventSFML.cpp)
	list(APPEND SOURCE src/TwEventWin.c)
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
#	list(APPEND PRIVATE_HEADERS build/Win32/res/TwXCursors.h)
endif()


set(PUBLIC_HEADERS 
	include/AntTweakBar.h
)

set(WIN32_RESOURCE_FILES
	build/Win32/AntTweakBar.rc	
	build/Win32/res/cur00000.cur
	build/Win32/res/cur00001.cur
	build/Win32/res/cur00002.cur
	build/Win32/res/cur00003.cur
	build/Win32/res/cur00004.cur
	build/Win32/res/cur00005.cur
	build/Win32/res/cur00006.cur
	build/Win32/res/cur00007.cur
	build/Win32/res/cur00008.cur
	build/Win32/res/cur00009.cur
	build/Win32/res/cur00010.cur
	build/Win32/res/cur00011.cur
	build/Win32/res/cur00012.cur
	build/Win32/res/cur00013.cur
	build/Win32/res/FontChars.txt
	build/Win32/res/FontFixed1.pgm
	build/Win32/res/FontLargeAA.pgm
	build/Win32/res/FontNormal.pgm
	build/Win32/res/FontSmall.pgm
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

foreach(FILE ${DX_SHADERS})
	set_source_files_properties(
		${FILE} PROPERTIES HEADER_FILE_ONLY TRUE
	)
endforeach(FILE)

if(WIN32)
	list(APPEND TW_BAR_ALL_SOURCE ${WIN32_RESOURCE_FILES})
endif()

add_library(
	TwBarLib STATIC ${TW_BAR_ALL_SOURCE}
)

if(MSVC)
	target_compile_options(TwBarLib PRIVATE /wd4302 /wd4456)
endif()

target_link_libraries(TwBarLib BuildSettings Common GraphicsEngine PlatformInterface)

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
	#${DX_SHADERS}
	${GL_SHADERS}
)
add_dependencies(ProcessTwBarLibShaders File2String)

set(SHADER_OUTPUT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/ShadersInc)
file(MAKE_DIRECTORY ${SHADER_OUTPUT_DIR})

foreach(FILE ${DX_SHADERS})
  get_filename_component(FILE_NAME_WE ${FILE} NAME_WE)
  add_custom_command(TARGET ProcessTwBarLibShaders 
					 # Unfortunately it is not possible to set TARGET directly to GraphicsEngineD3D12-static/shared
					 # because PRE_BUILD is only supported on Visual Studio 8 or later. For all other generators 
					 # PRE_BUILD is treated as PRE_LINK.
					 COMMAND $<TARGET_FILE:File2String> ${CMAKE_CURRENT_SOURCE_DIR}/${FILE} ${SHADER_OUTPUT_DIR}/${FILE_NAME_WE}_DX.h
                     MAIN_DEPENDENCY ${FILE} # the primary input source file to the command
                     COMMENT "Processing shader ${FILE}"
                     VERBATIM
  )
endforeach(FILE)

foreach(FILE ${GL_SHADERS})
  get_filename_component(FILE_NAME_WE ${FILE} NAME_WE)
  add_custom_command(TARGET ProcessTwBarLibShaders 
					 # Unfortunately it is not possible to set TARGET directly to GraphicsEngineD3D12-static/shared
					 # because PRE_BUILD is only supported on Visual Studio 8 or later. For all other generators 
					 # PRE_BUILD is treated as PRE_LINK.
					 COMMAND $<TARGET_FILE:File2String> ${CMAKE_CURRENT_SOURCE_DIR}/${FILE} ${SHADER_OUTPUT_DIR}/${FILE_NAME_WE}_GL.h
                     MAIN_DEPENDENCY ${FILE} # the primary input source file to the command
                     COMMENT "Processing shader ${FILE}"
                     VERBATIM
  )
endforeach(FILE)

add_dependencies(TwBarLib ProcessTwBarLibShaders)

target_compile_definitions(TwBarLib PRIVATE -DTW_STATIC)

if(MSVC)
	# Enable link-time code generation for release builds (I was not able to 
	# find any way to set these settings through interface library BuildSettings)
	set_target_properties(TwBarLib PROPERTIES
		STATIC_LIBRARY_FLAGS_RELEASE /LTCG
		STATIC_LIBRARY_FLAGS_MINSIZEREL /LTCG
		STATIC_LIBRARY_FLAGS_RELWITHDEBINFO /LTCG
	)
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
set_target_properties(ProcessTwBarLibShaders PROPERTIES
	FOLDER Samples/External
)