cmake_minimum_required (VERSION 3.6)

if (NOT PLATFORM_EMSCRIPTEN)
	add_subdirectory(UnityEmulator)
	add_subdirectory(GhostCubeScene)
	add_subdirectory(GhostCubePlugin)
endif()
