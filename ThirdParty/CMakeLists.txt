cmake_minimum_required (VERSION 3.6)

file(RELATIVE_PATH SAMPLES_DIR "${CMAKE_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/..")

if (NOT DILIGENT_NUKLEAR_DIR)
    set(DILIGENT_NUKLEAR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/nuklear" CACHE PATH "nuklear source directory")
endif()
if (NOT EXISTS "${DILIGENT_NUKLEAR_DIR}/nuklear.h")
    message(FATAL_ERROR "${DILIGENT_NUKLEAR_DIR}/nuklear.h does not exist. This indicates that nuklear submodule is not checked out or DILIGENT_NUKLEAR_DIR variable contains invalid path.")
endif()
install(FILES "${DILIGENT_NUKLEAR_DIR}/Readme.md" DESTINATION "Licenses/ThirdParty/${SAMPLES_DIR}" RENAME Nuklear-Readme.md)

if ((PLATFORM_WIN32 OR PLATFORM_LINUX OR PLATFORM_MACOS) AND (NOT TARGET glfw))
    set(GLFW_BUILD_DOCS OFF CACHE INTERNAL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE INTERNAL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE INTERNAL "" FORCE)
    set(GLFW_INSTALL OFF CACHE INTERNAL "" FORCE)
    add_subdirectory(glfw)
    set_target_properties(glfw update_mappings PROPERTIES FOLDER DiligentSamples/ThirdParty/GLFW)

    install(FILES "glfw/LICENSE.md" DESTINATION "Licenses/ThirdParty/${SAMPLES_DIR}" RENAME glfw-LICENSE.md)
endif()
