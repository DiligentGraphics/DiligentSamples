cmake_minimum_required (VERSION 3.6)

if(PLATFORM_WIN32 OR PLATFORM_LINUX)
    option(INSTALL_DILIGENT_SAMPLES "Enable installation of samples and tutorials" ON)
endif()

add_subdirectory(External)
add_subdirectory(Samples)
add_subdirectory(Tutorials)