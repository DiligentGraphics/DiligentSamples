cmake_minimum_required (VERSION 3.10)

if (NOT PLATFORM_WEB)
	add_subdirectory(UnityEmulator)
	add_subdirectory(GhostCubeScene)
	add_subdirectory(GhostCubePlugin)
endif()
