cmake_minimum_required (VERSION 3.23)

project(Tutorial03_Texturing-DotNet)

if((CMAKE_GENERATOR_PLATFORM STREQUAL "x64") OR (CMAKE_GENERATOR_PLATFORM STREQUAL ""))
    set(DOTNET_GENERATOR_PLATFORM "x64")
elseif(CMAKE_GENERATOR_PLATFORM STREQUAL "Win32")
    set(DOTNET_GENERATOR_PLATFORM "x86")
else()
    message(FATAL_ERROR "Generator platform is set to '${CMAKE_GENERATOR_PLATFORM}', which is not supported by managed projects")
endif()

include_external_msproject(${PROJECT_NAME}
    ${CMAKE_CURRENT_SOURCE_DIR}/${PROJECT_NAME}.csproj
    PLATFORM ${DOTNET_GENERATOR_PLATFORM})

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        FOLDER "DiligentSamples/Tutorials"
    )
