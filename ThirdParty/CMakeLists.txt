cmake_minimum_required (VERSION 3.6)

file(RELATIVE_PATH SAMPLES_DIR "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/..")
install(FILES nuklear/Readme.md DESTINATION "Licenses/ThirdParty/${SAMPLES_DIR}" RENAME Nuklear-Readme.md)
install(FILES glfw/LICENSE.md DESTINATION "Licenses/ThirdParty/${SAMPLES_DIR}" RENAME glfw-LICENSE.md)

if(PLATFORM_WIN32 OR PLATFORM_LINUX OR PLATFORM_MACOS)
    set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
    set(GLFW_INSTALL OFF CACHE INTERNAL "" FORCE)
    add_subdirectory(glfw)
    set_target_properties(glfw PROPERTIES FOLDER DiligentSamples/ThirdParty)
endif()
